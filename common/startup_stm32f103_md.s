.syntax unified
.cpu cortex-m3
.fpu softvfp
.thumb

.global g_pfnVectors
.global Reset_Handler
.global Default_Handler

.extern SystemInit
.extern main

/* -------------------------------------------------------------------------- */
/* Reset handler                                                              */
/* -------------------------------------------------------------------------- */

.section .text.Reset_Handler,"ax",%progbits
.type Reset_Handler, %function
.thumb_func
Reset_Handler:
    /* Copy initialized data from its Flash load address to SRAM. */
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata

.L_copy_data:
    cmp r1, r2
    bcs .L_zero_bss_start
    ldr r3, [r0], #4
    str r3, [r1], #4
    b .L_copy_data

.L_zero_bss_start:
    /* Zero the .bss section. */
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0

.L_zero_bss:
    cmp r1, r2
    bcs .L_call_main
    str r3, [r1], #4
    b .L_zero_bss

.L_call_main:
    bl SystemInit
    bl main

/* main() must not return. Trap safely if it does. */
.L_main_returned:
    b .L_main_returned

.size Reset_Handler, .-Reset_Handler

/* -------------------------------------------------------------------------- */
/* Default exception/IRQ handler                                              */
/* -------------------------------------------------------------------------- */

.section .text.Default_Handler,"ax",%progbits
.type Default_Handler, %function
.thumb_func
Default_Handler:
    b Default_Handler
.size Default_Handler, .-Default_Handler

/* -------------------------------------------------------------------------- */
/* Vector table: STM32F103 medium-density, external IRQ 0..42                 */
/* -------------------------------------------------------------------------- */

.section .isr_vector,"a",%progbits
.align 2
.type g_pfnVectors, %object

g_pfnVectors:
    /* Cortex-M3 core exceptions. */
    .word _estack
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler
    .word DebugMon_Handler
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler

    /* STM32F103 medium-density peripheral interrupts. */
    .word WWDG_IRQHandler
    .word PVD_IRQHandler
    .word TAMPER_IRQHandler
    .word RTC_IRQHandler
    .word FLASH_IRQHandler
    .word RCC_IRQHandler
    .word EXTI0_IRQHandler
    .word EXTI1_IRQHandler
    .word EXTI2_IRQHandler
    .word EXTI3_IRQHandler
    .word EXTI4_IRQHandler
    .word DMA1_Channel1_IRQHandler
    .word DMA1_Channel2_IRQHandler
    .word DMA1_Channel3_IRQHandler
    .word DMA1_Channel4_IRQHandler
    .word DMA1_Channel5_IRQHandler
    .word DMA1_Channel6_IRQHandler
    .word DMA1_Channel7_IRQHandler
    .word ADC1_2_IRQHandler
    .word USB_HP_CAN1_TX_IRQHandler
    .word USB_LP_CAN1_RX0_IRQHandler
    .word CAN1_RX1_IRQHandler
    .word CAN1_SCE_IRQHandler
    .word EXTI9_5_IRQHandler
    .word TIM1_BRK_IRQHandler
    .word TIM1_UP_IRQHandler
    .word TIM1_TRG_COM_IRQHandler
    .word TIM1_CC_IRQHandler
    .word TIM2_IRQHandler
    .word TIM3_IRQHandler
    .word TIM4_IRQHandler
    .word I2C1_EV_IRQHandler
    .word I2C1_ER_IRQHandler
    .word I2C2_EV_IRQHandler
    .word I2C2_ER_IRQHandler
    .word SPI1_IRQHandler
    .word SPI2_IRQHandler
    .word USART1_IRQHandler
    .word USART2_IRQHandler
    .word USART3_IRQHandler
    .word EXTI15_10_IRQHandler
    .word RTCAlarm_IRQHandler
    .word USBWakeUp_IRQHandler

.size g_pfnVectors, .-g_pfnVectors

/* -------------------------------------------------------------------------- */
/* Weak aliases                                                               */
/* -------------------------------------------------------------------------- */

.macro WEAK_DEFAULT_HANDLER handler_name
    .weak \handler_name
    .thumb_set \handler_name, Default_Handler
.endm

WEAK_DEFAULT_HANDLER NMI_Handler
WEAK_DEFAULT_HANDLER HardFault_Handler
WEAK_DEFAULT_HANDLER MemManage_Handler
WEAK_DEFAULT_HANDLER BusFault_Handler
WEAK_DEFAULT_HANDLER UsageFault_Handler
WEAK_DEFAULT_HANDLER SVC_Handler
WEAK_DEFAULT_HANDLER DebugMon_Handler
WEAK_DEFAULT_HANDLER PendSV_Handler
WEAK_DEFAULT_HANDLER SysTick_Handler

WEAK_DEFAULT_HANDLER WWDG_IRQHandler
WEAK_DEFAULT_HANDLER PVD_IRQHandler
WEAK_DEFAULT_HANDLER TAMPER_IRQHandler
WEAK_DEFAULT_HANDLER RTC_IRQHandler
WEAK_DEFAULT_HANDLER FLASH_IRQHandler
WEAK_DEFAULT_HANDLER RCC_IRQHandler
WEAK_DEFAULT_HANDLER EXTI0_IRQHandler
WEAK_DEFAULT_HANDLER EXTI1_IRQHandler
WEAK_DEFAULT_HANDLER EXTI2_IRQHandler
WEAK_DEFAULT_HANDLER EXTI3_IRQHandler
WEAK_DEFAULT_HANDLER EXTI4_IRQHandler
WEAK_DEFAULT_HANDLER DMA1_Channel1_IRQHandler
WEAK_DEFAULT_HANDLER DMA1_Channel2_IRQHandler
WEAK_DEFAULT_HANDLER DMA1_Channel3_IRQHandler
WEAK_DEFAULT_HANDLER DMA1_Channel4_IRQHandler
WEAK_DEFAULT_HANDLER DMA1_Channel5_IRQHandler
WEAK_DEFAULT_HANDLER DMA1_Channel6_IRQHandler
WEAK_DEFAULT_HANDLER DMA1_Channel7_IRQHandler
WEAK_DEFAULT_HANDLER ADC1_2_IRQHandler
WEAK_DEFAULT_HANDLER USB_HP_CAN1_TX_IRQHandler
WEAK_DEFAULT_HANDLER USB_LP_CAN1_RX0_IRQHandler
WEAK_DEFAULT_HANDLER CAN1_RX1_IRQHandler
WEAK_DEFAULT_HANDLER CAN1_SCE_IRQHandler
WEAK_DEFAULT_HANDLER EXTI9_5_IRQHandler
WEAK_DEFAULT_HANDLER TIM1_BRK_IRQHandler
WEAK_DEFAULT_HANDLER TIM1_UP_IRQHandler
WEAK_DEFAULT_HANDLER TIM1_TRG_COM_IRQHandler
WEAK_DEFAULT_HANDLER TIM1_CC_IRQHandler
WEAK_DEFAULT_HANDLER TIM2_IRQHandler
WEAK_DEFAULT_HANDLER TIM3_IRQHandler
WEAK_DEFAULT_HANDLER TIM4_IRQHandler
WEAK_DEFAULT_HANDLER I2C1_EV_IRQHandler
WEAK_DEFAULT_HANDLER I2C1_ER_IRQHandler
WEAK_DEFAULT_HANDLER I2C2_EV_IRQHandler
WEAK_DEFAULT_HANDLER I2C2_ER_IRQHandler
WEAK_DEFAULT_HANDLER SPI1_IRQHandler
WEAK_DEFAULT_HANDLER SPI2_IRQHandler
WEAK_DEFAULT_HANDLER USART1_IRQHandler
WEAK_DEFAULT_HANDLER USART2_IRQHandler
WEAK_DEFAULT_HANDLER USART3_IRQHandler
WEAK_DEFAULT_HANDLER EXTI15_10_IRQHandler
WEAK_DEFAULT_HANDLER RTCAlarm_IRQHandler
WEAK_DEFAULT_HANDLER USBWakeUp_IRQHandler
