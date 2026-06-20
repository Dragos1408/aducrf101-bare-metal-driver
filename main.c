/*
 * ADuCRF101 (ARM Cortex-M3) + BMP280  --  bare-register driver, polling only
 *
 * Hardware
 *   UART output : P1.0 = RXD, P1.1 = TXD   (GP1CON: CON0=UART0RXD, CON1=UART0TXD)
 *   I2C master  : P1.4 = SCL, P1.5 = SDA   (GP1CON: CON4=I2C0SCL,  CON5=I2C0SDA)
 *   BMP280      : 7-bit address 0x76, external pull-ups on SCL/SDA assumed present
 *
 * Clock
 *   Core/peripheral clock = HFOSC 16 MHz / 2 = 8 MHz  (set via CLKCON CD=DIV2).
 *   UART 9600 8N1, I2C ~100 kHz, both derived from 8 MHz.
 *
 * No ADI peripheral libraries are used: everything goes through the
 * pADI_UART / pADI_I2C / pADI_GP1 / pADI_CLKCTL register pointers and the
 * bit-mask #defines from ADuCRF101.h. No interrupts, no DMA, no FPU
 * (BMP280 compensation uses the Bosch integer formulas only).
 */

#include <stdint.h>
#include "ADuCRF101.h"

/* ---- timeouts ---------------------------------------------------------- */
/* Finite poll counts so a stuck bus can never hang the firmware.
   At 8 MHz one I2C byte @100 kHz takes ~90 us; 100000 spins is several ms. */
#define I2C_TIMEOUT   100000

/* ---- BMP280 ------------------------------------------------------------ */
#define BMP280_ADDR   0x76        /* 7-bit I2C address                      */
#define REG_ID        0xD0        /* chip-id register (expect 0x58)         */
#define REG_CALIB     0x88        /* 0x88..0x9F : T1..T3, P1..P9 (24 bytes) */
#define REG_CTRL_MEAS 0xF4
#define REG_CONFIG    0xF5
#define REG_PRESS_MSB 0xF7        /* 0xF7..0xFC : press[3] then temp[3]     */

static void uart_init(void)
{
    /* Route P1.0 -> UART0 RXD (func 01) and P1.1 -> UART0 TXD (func 10).
       Read-modify-write so the I2C pins (P1.4/P1.5) are not disturbed. */
    pADI_GP1->GPCON = (pADI_GP1->GPCON & ~(GP1CON_CON0_MSK | GP1CON_CON1_MSK))
                    | GP1CON_CON0_UART0RXD | GP1CON_CON1_UART0TXD;

    pADI_UART->COMLCR = COMLCR_WLS_8BITS;   /* 8 data bits, 1 stop, no parity */

    /* Baud = UARTCLK / (16 * COMDIV).  8e6 / (16 * 9600) = 52.08 -> 52.
       Actual = 8e6 / (16 * 52) = 9615.4 baud  (+0.16 %, well within tol).  */
    pADI_UART->COMFBR = 0;                   /* fractional divider disabled    */
    pADI_UART->COMDIV = 52;

    pADI_UART->COMIEN = 0;                   /* polling only, no UART IRQs      */
}

static void uart_putc(char c)
{
    while (!(pADI_UART->COMLSR & COMLSR_THRE))  /* wait: TX holding reg empty */
        ;
    pADI_UART->COMTX = (uint8_t)c;
}

static void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

static void uart_hex8(uint8_t v)
{
    static const char h[] = "0123456789ABCDEF";
    uart_putc('0'); uart_putc('x');
    uart_putc(h[v >> 4]);
    uart_putc(h[v & 0x0F]);
}

static void uart_u32(uint32_t v)
{
    char b[10];
    int  i = 0;
    if (v == 0) { uart_putc('0'); return; }
    while (v) { b[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i--) uart_putc(b[i]);
}

/* Print a signed value expressed in hundredths as "D.DD" (e.g. 2345 -> 23.45) */
static void uart_centi(int32_t v)
{
    uint32_t f;
    if (v < 0) { uart_putc('-'); v = -v; }
    uart_u32((uint32_t)(v / 100));
    uart_putc('.');
    f = (uint32_t)(v % 100);
    uart_putc((char)('0' + f / 10));
    uart_putc((char)('0' + f % 10));
}

static void i2c_init(void)
{
    /* Route P1.4 -> I2C0 SCL (func 01) and P1.5 -> I2C0 SDA (func 01).
       Read-modify-write so the UART pins (P1.0/P1.1) are not disturbed. */
    pADI_GP1->GPCON = (pADI_GP1->GPCON & ~(GP1CON_CON4_MSK | GP1CON_CON5_MSK))
                    | GP1CON_CON4_I2C0SCL | GP1CON_CON5_I2C0SDA;

    /* SCL period = (HIGH+1)+(LOW+1) UCLK ticks.
       100 kHz from 8 MHz: 8e6/100e3 = 80 ticks -> HIGH=LOW=39=0x27.
       (39+1)+(39+1)=80 -> 8e6/80 = 100 kHz exactly.                       */
    pADI_I2C->I2CDIV = (0x27 << 8) | 0x27;   /* I2CDIV[HIGH]=0x27, [LOW]=0x27 */

    pADI_I2C->I2CMCON = I2CMCON_MAS_EN;       /* enable master, no IRQ/DMA      */
}

/*
 * Write n bytes to a device (n <= TX FIFO depth used here = 2).
 * Issues:  START | addr+W | data[0..n-1] | STOP
 * Returns  0 ok | -1 timeout | -2 NACK (address or data).
 */
static int i2c_write(uint8_t dev7, const uint8_t *data, int n)
{
    int      i, to;
    uint16_t s;

    (void)pADI_I2C->I2CMSTA;                  /* clear stale read-clear flags  */

    /* Preload the FIFO (depth 2) before issuing the START so a single-byte
       transfer cannot underrun (= premature STOP) before the byte is queued. */
    for (i = 0; i < n && i < 2; i++)
        pADI_I2C->I2CMTX = data[i];

    pADI_I2C->I2CADR0 = (uint8_t)((dev7 << 1) | 0);   /* addr + WRITE -> START  */

    /* Feed any remaining bytes as the FIFO drains. */
    for (; i < n; i++) {
        for (to = I2C_TIMEOUT; to; to--) {
            s = pADI_I2C->I2CMSTA;
            if (s & (I2CMSTA_NACKADDR | I2CMSTA_NACKDATA)) return -2;
            if (s & I2CMSTA_TXREQ) break;     /* FIFO ready for another byte    */
        }
        if (!to) return -1;
        pADI_I2C->I2CMTX = data[i];
    }

    /* Wait for STOP (transaction complete), catching a late NACK. */
    for (to = I2C_TIMEOUT; to; to--) {
        s = pADI_I2C->I2CMSTA;
        if (s & (I2CMSTA_NACKADDR | I2CMSTA_NACKDATA)) return -2;
        if (s & I2CMSTA_TCOMP) return 0;
    }
    return -1;
}

/*
 * Read n bytes from a device into buf.
 * Issues:  START | addr+R | n bytes | STOP   (BMP280 auto-increments regs)
 * Returns  0 ok | -1 timeout | -2 NACK (address).
 */
static int i2c_read_n(uint8_t dev7, uint8_t *buf, int n)
{
    int      i, to;
    uint16_t s;

    (void)pADI_I2C->I2CMSTA;                  /* clear stale read-clear flags  */

    pADI_I2C->I2CMRXCNT = (uint16_t)(n - 1);  /* COUNT = bytes-required minus 1 */
    pADI_I2C->I2CADR0   = (uint8_t)((dev7 << 1) | 1);  /* addr + READ -> START   */

    for (i = 0; i < n; i++) {
        for (to = I2C_TIMEOUT; to; to--) {
            s = pADI_I2C->I2CMSTA;
            if (s & I2CMSTA_NACKADDR) return -2;
            if (s & I2CMSTA_RXREQ) break;     /* a byte is waiting in RX FIFO   */
        }
        if (!to) return -1;
        buf[i] = (uint8_t)pADI_I2C->I2CMRX;
    }

    /* Wait for STOP so the bus is idle before the next transaction. */
    for (to = I2C_TIMEOUT; to && !(pADI_I2C->I2CMSTA & I2CMSTA_TCOMP); to--)
        ;
    return to ? 0 : -1;
}

/* Set the register pointer (write), then read one byte. */
static int i2c_read_reg(uint8_t dev7, uint8_t reg, uint8_t *out)
{
    int r = i2c_write(dev7, &reg, 1);
    if (r) return r;
    return i2c_read_n(dev7, out, 1);
}

/* Set the register pointer (write), then burst-read n consecutive registers. */
static int i2c_read_regs(uint8_t dev7, uint8_t reg, uint8_t *buf, int n)
{
    int r = i2c_write(dev7, &reg, 1);
    if (r) return r;
    return i2c_read_n(dev7, buf, n);
}

/* Write a single value to a register:  addr+W | reg | val | STOP. */
static int i2c_write_reg(uint8_t dev7, uint8_t reg, uint8_t val)
{
    uint8_t b[2];
    b[0] = reg;
    b[1] = val;
    return i2c_write(dev7, b, 2);
}

typedef int32_t  BMP280_S32_t;
typedef uint32_t BMP280_U32_t;
typedef int64_t  BMP280_S64_t;

/* Calibration words (little-endian); T1/P1 unsigned, the rest signed. */
static uint16_t dig_T1;
static int16_t  dig_T2, dig_T3;
static uint16_t dig_P1;
static int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;

static BMP280_S32_t t_fine;       /* carries fine temperature between T and P */

/* Returns temperature in 0.01 degC. "5123" == 51.23 degC. */
static BMP280_S32_t bmp280_compensate_T_int32(BMP280_S32_t adc_T)
{
    BMP280_S32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((BMP280_S32_t)dig_T1 << 1))) * ((BMP280_S32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((BMP280_S32_t)dig_T1)) * ((adc_T >> 4) - ((BMP280_S32_t)dig_T1))) >> 12) *
            ((BMP280_S32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    return T;
}

/* Returns pressure in Pa as Q24.8 (unsigned). "24674867" == 24674867/256 = 96386.2 Pa. */
static BMP280_U32_t bmp280_compensate_P_int64(BMP280_S32_t adc_P)
{
    BMP280_S64_t var1, var2, p;
    var1 = ((BMP280_S64_t)t_fine) - 128000;
    var2 = var1 * var1 * (BMP280_S64_t)dig_P6;
    var2 = var2 + ((var1 * (BMP280_S64_t)dig_P5) << 17);
    var2 = var2 + (((BMP280_S64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (BMP280_S64_t)dig_P3) >> 8) + ((var1 * (BMP280_S64_t)dig_P2) << 12);
    var1 = (((((BMP280_S64_t)1) << 47) + var1)) * ((BMP280_S64_t)dig_P1) >> 33;
    if (var1 == 0)
        return 0;                 /* avoid division by zero                  */
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((BMP280_S64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((BMP280_S64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((BMP280_S64_t)dig_P7) << 4);
    return (BMP280_U32_t)p;
}

static void delay_ms(unsigned int ms)
{
    volatile unsigned int i;
    for (; ms; ms--)
        for (i = 1600; i; i--)    /* ~1 ms at 8 MHz (approximate)            */
            ;
}

static int read_calibration(void)
{
    uint8_t c[24];                /* 0x88..0x9F */
    int r = i2c_read_regs(BMP280_ADDR, REG_CALIB, c, 24);
    if (r) return r;

    dig_T1 = (uint16_t)(c[0]  | (c[1]  << 8));
    dig_T2 = (int16_t) (c[2]  | (c[3]  << 8));
    dig_T3 = (int16_t) (c[4]  | (c[5]  << 8));
    dig_P1 = (uint16_t)(c[6]  | (c[7]  << 8));
    dig_P2 = (int16_t) (c[8]  | (c[9]  << 8));
    dig_P3 = (int16_t) (c[10] | (c[11] << 8));
    dig_P4 = (int16_t) (c[12] | (c[13] << 8));
    dig_P5 = (int16_t) (c[14] | (c[15] << 8));
    dig_P6 = (int16_t) (c[16] | (c[17] << 8));
    dig_P7 = (int16_t) (c[18] | (c[19] << 8));
    dig_P8 = (int16_t) (c[20] | (c[21] << 8));
    dig_P9 = (int16_t) (c[22] | (c[23] << 8));
    return 0;
}


int main(void)
{
    uint8_t id;
    int     r;

    /* Core/peripheral clock = HFOSC 16 MHz / 2 = 8 MHz.
       (Internal osc is fixed 16 MHz, so 8 MHz == CD=DIV2.)               */
    pADI_CLKCTL->CLKCON = (pADI_CLKCTL->CLKCON & ~CLKCON_CD_MSK) | CLKCON_CD_DIV2;

    uart_init();
    i2c_init();

    delay_ms(50);                 /* BMP280 power-on / startup margin        */

    uart_puts("\r\n=== ADuCRF101 + BMP280 (bare register) ===\r\n");

    /* ---- chip ID ---- */
    r = i2c_read_reg(BMP280_ADDR, REG_ID, &id);
    if (r) {
        uart_puts(r == -2 ? "ERROR: NACK reading chip ID (no device at 0x76?)\r\n"
                          : "ERROR: I2C timeout reading chip ID (pull-ups/pins?)\r\n");
        for (;;) ;
    }
    uart_puts("CHIP_ID = ");
    uart_hex8(id);
    uart_puts("\r\n");
    if (id == 0x60)
        uart_puts("note: 0x60 -> this is a BME280, not BMP280 (continuing)\r\n");
    else if (id != 0x58)
        uart_puts("warning: unexpected chip ID (expected 0x58)\r\n");

    /* ---- calibration ---- */
    if (read_calibration()) {
        uart_puts("ERROR: failed to read calibration\r\n");
        for (;;) ;
    }

    /* ---- configure: normal mode, oversampling x1, filter off ---- */
    i2c_write_reg(BMP280_ADDR, REG_CTRL_MEAS, 0x27);   /* osrs_t=x1, osrs_p=x1, normal */
    i2c_write_reg(BMP280_ADDR, REG_CONFIG,    0x00);   /* t_sb=0, filter off, 4-wire    */

    /* ---- 1 Hz measurement loop ---- */
    for (;;) {
        uint8_t      raw[6];          /* 0xF7..0xFC: P[msb,lsb,xlsb], T[msb,lsb,xlsb] */
        BMP280_S32_t adc_P, adc_T, T;
        BMP280_U32_t P;

        if (i2c_read_regs(BMP280_ADDR, REG_PRESS_MSB, raw, 6)) {
            uart_puts("ERROR: burst read failed\r\n");
            delay_ms(1000);
            continue;
        }

        adc_P = ((BMP280_S32_t)raw[0] << 12) | ((BMP280_S32_t)raw[1] << 4) | (raw[2] >> 4);
        adc_T = ((BMP280_S32_t)raw[3] << 12) | ((BMP280_S32_t)raw[4] << 4) | (raw[5] >> 4);

        T = bmp280_compensate_T_int32(adc_T);          /* 0.01 degC, sets t_fine */
        P = bmp280_compensate_P_int64(adc_P) >> 8;     /* Q24.8 -> integer Pa    */

        uart_puts("T = ");
        uart_centi(T);                /* e.g. 23.45 */
        uart_puts(" C   P = ");
        uart_u32(P);
        uart_puts(" Pa\r\n");

        delay_ms(1000);
    }
}
