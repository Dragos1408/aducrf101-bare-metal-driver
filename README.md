# Bare-Metal Peripheral Drivers: ADuCRF101 ARM Cortex-M3 + BMP280

Complete bare-metal firmware in C for the **ADuCRF101** (Analog Devices, ARM Cortex-M3, 8 MHz)
with no HAL, no vendor BSP, and no abstraction layers. All peripheral access is performed
directly through CMSIS pointer structs and bitmask operations on ADuCRF101.h register definitions.

---

## What Makes This Unusual

Most embedded projects rely on HAL drivers that hide hardware details. This project implements
everything from the datasheet up:

- GPIO mux configured manually via `GP1CON` registers
- Baud-rate divisor computed from first principles:
  `COMDIV = 52` → +0.16% error at 9600 baud, 8 MHz clock
- I2C SCL timing derived from datasheet formula:
  `I2CDIV = 0x27` → exact 100 kHz at 8 MHz
- Final binary: **648 bytes Flash, 0 errors, 0 warnings**, correct behavior on first hardware deployment

---

## Drivers Implemented

### UART Driver
- Manual GPIO mux configuration via `GP1CON`
- Baud-rate divisor computed from datasheet formula
- Polling-based transmit and receive

### I2C Master Driver
- SCL timing computed from datasheet formula
- Start/stop condition generation
- Single-byte and burst read/write operations

---

## BMP280 Sensor Integration (via I2C)

- Chip-ID verification on startup
- 24-byte factory calibration readout from registers `0x88–0x9F`
- Burst register acquisition from `0xF7–0xFC`
- Bosch integer-only compensation formulas for temperature and pressure
  (no floating-point arithmetic)

---

## Bring-Up Methodology

Each firmware layer was validated in isolation before integration:

1. GPIO and clock configuration verified
2. UART loopback test confirmed baud rate accuracy
3. I2C start/stop waveforms verified on oscilloscope
4. BMP280 chip-ID read confirmed I2C communication
5. Calibration read and compensation output validated against reference values
6. Full integration tested on hardware

---

## Toolchain

| Tool        | Details                                        |
|-------------|------------------------------------------------|
| IDE         | Keil µVision 5                                 |
| Compiler    | armclang v6 (ARM Compiler 6)                   |
| CMSIS       | Cortex Microcontroller Software Interface Standard |
| Header      | ADuCRF101.h (Analog Devices register definitions) |
| Target MCU  | ADuCRF101 — ARM Cortex-M3, 8 MHz              |
| Sensor      | Bosch BMP280 (temperature + pressure)          |

---

## Repository Structure

```
proj_1/
├── main.c          # Top-level firmware entry point and bring-up sequence
├── uart.c          # UART driver implementation
├── uart.h          # UART driver interface
├── i2c.c           # I2C master driver implementation
├── i2c.h           # I2C master driver interface
├── bmp280.c        # BMP280 sensor driver and compensation formulas
├── bmp280.h        # BMP280 driver interface and calibration structs
└── ADuCRF101.h     # Analog Devices register definition header (CMSIS)
```

> Update the structure above to match your actual filenames if they differ.

---

## Project Context

Academic project completed during MSc in Intelligent Electronic Systems,
Gheorghe Asachi Technical University of Iași (TUIASI), 2026.