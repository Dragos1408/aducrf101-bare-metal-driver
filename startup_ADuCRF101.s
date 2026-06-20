;******************************************************************************
; startup_ADuCRF101.s
; Minimal startup file for ADuCRF101 (ARM Cortex-M3) for Keil MDK / armasm
; (Arm Compiler 5 syntax). Provides the vector table and the reset handler.
; Reset_Handler calls SystemInit (from system_ADuCRF101.c) then __main.
;
; Enough device-interrupt slots are reserved and pointed at a default handler.
; This is sufficient for polled (no-interrupt) firmware such as the
; UART + I2C + HTS221 bring-up.
;******************************************************************************

Stack_Size      EQU     0x00000400
                AREA    STACK, NOINIT, READWRITE, ALIGN=3
Stack_Mem       SPACE   Stack_Size
__initial_sp

Heap_Size       EQU     0x00000200
                AREA    HEAP, NOINIT, READWRITE, ALIGN=3
__heap_base
Heap_Mem        SPACE   Heap_Size
__heap_limit

                PRESERVE8
                THUMB

;------------------------------------------------------------------------------
; Vector Table  (placed at the start of flash, address 0x00000000)
;------------------------------------------------------------------------------
                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors
                EXPORT  __Vectors_End
                EXPORT  __Vectors_Size

__Vectors       DCD     __initial_sp              ; Top of Stack
                DCD     Reset_Handler             ; Reset Handler
                DCD     NMI_Handler               ; NMI Handler
                DCD     HardFault_Handler         ; Hard Fault Handler
                DCD     MemManage_Handler         ; MPU Fault Handler
                DCD     BusFault_Handler          ; Bus Fault Handler
                DCD     UsageFault_Handler        ; Usage Fault Handler
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     SVC_Handler               ; SVCall Handler
                DCD     DebugMon_Handler          ; Debug Monitor Handler
                DCD     0                         ; Reserved
                DCD     PendSV_Handler            ; PendSV Handler
                DCD     SysTick_Handler           ; SysTick Handler

                ; --- External (device) interrupts: generic, unused by polled code ---
                DCD     Default_Handler           ; 0
                DCD     Default_Handler           ; 1
                DCD     Default_Handler           ; 2
                DCD     Default_Handler           ; 3
                DCD     Default_Handler           ; 4
                DCD     Default_Handler           ; 5
                DCD     Default_Handler           ; 6
                DCD     Default_Handler           ; 7
                DCD     Default_Handler           ; 8
                DCD     Default_Handler           ; 9
                DCD     Default_Handler           ; 10
                DCD     Default_Handler           ; 11
                DCD     Default_Handler           ; 12
                DCD     Default_Handler           ; 13
                DCD     Default_Handler           ; 14
                DCD     Default_Handler           ; 15
                DCD     Default_Handler           ; 16
                DCD     Default_Handler           ; 17
                DCD     Default_Handler           ; 18
                DCD     Default_Handler           ; 19
                DCD     Default_Handler           ; 20
                DCD     Default_Handler           ; 21
                DCD     Default_Handler           ; 22
                DCD     Default_Handler           ; 23
                DCD     Default_Handler           ; 24
                DCD     Default_Handler           ; 25
                DCD     Default_Handler           ; 26
                DCD     Default_Handler           ; 27
                DCD     Default_Handler           ; 28
                DCD     Default_Handler           ; 29
                DCD     Default_Handler           ; 30
                DCD     Default_Handler           ; 31
__Vectors_End
__Vectors_Size  EQU     __Vectors_End - __Vectors

                AREA    |.text|, CODE, READONLY

;------------------------------------------------------------------------------
; Reset Handler
;------------------------------------------------------------------------------
Reset_Handler   PROC
                EXPORT  Reset_Handler             [WEAK]
                IMPORT  __main
                IMPORT  SystemInit
                LDR     R0, =SystemInit
                BLX     R0
                LDR     R0, =__main
                BX      R0
                ENDP

;------------------------------------------------------------------------------
; Core exception handlers (infinite loop = catch faults during bring-up)
;------------------------------------------------------------------------------
NMI_Handler     PROC
                EXPORT  NMI_Handler               [WEAK]
                B       .
                ENDP
HardFault_Handler PROC
                EXPORT  HardFault_Handler         [WEAK]
                B       .
                ENDP
MemManage_Handler PROC
                EXPORT  MemManage_Handler         [WEAK]
                B       .
                ENDP
BusFault_Handler PROC
                EXPORT  BusFault_Handler          [WEAK]
                B       .
                ENDP
UsageFault_Handler PROC
                EXPORT  UsageFault_Handler        [WEAK]
                B       .
                ENDP
SVC_Handler     PROC
                EXPORT  SVC_Handler               [WEAK]
                B       .
                ENDP
DebugMon_Handler PROC
                EXPORT  DebugMon_Handler          [WEAK]
                B       .
                ENDP
PendSV_Handler  PROC
                EXPORT  PendSV_Handler            [WEAK]
                B       .
                ENDP
SysTick_Handler PROC
                EXPORT  SysTick_Handler           [WEAK]
                B       .
                ENDP
Default_Handler PROC
                EXPORT  Default_Handler           [WEAK]
                B       .
                ENDP

                ALIGN

;------------------------------------------------------------------------------
; User stack and heap initialization (for the C library)
;------------------------------------------------------------------------------
                IF      :DEF:__MICROLIB
                EXPORT  __initial_sp
                EXPORT  __heap_base
                EXPORT  __heap_limit
                ELSE
                IMPORT  __use_two_region_memory
                EXPORT  __user_initial_stackheap
__user_initial_stackheap PROC
                LDR     R0, =Heap_Mem
                LDR     R1, =(Stack_Mem + Stack_Size)
                LDR     R2, =(Heap_Mem + Heap_Size)
                LDR     R3, =Stack_Mem
                BX      LR
                ENDP
                ALIGN
                ENDIF

                END
