// SPDX-License-Identifier: Apache-2.0

#include <das/cortex_m/startup.h>

#include "stm32h755xx.h"

#include <stdint.h>

extern uint32_t __StackTop;

/*
 * STM32H755 external-vector layout in architectural IRQ order.
 *
 * Keep the vector layout independent from DAS feature selection. IRQ slots are
 * fixed by the MCU, while the linked handler implementation is selected by
 * normal weak/strong symbol resolution. A peripheral, application, test or
 * RTOS therefore overrides only the handler symbols it owns and never needs to
 * copy the whole table just to use one interrupt.
 */
#define DAS_STM32H755_EXTERNAL_VECTOR_LIST(IRQ, RESERVED) \
    IRQ(WWDG_IRQHandler) \
    IRQ(PVD_AVD_IRQHandler) \
    IRQ(TAMP_STAMP_IRQHandler) \
    IRQ(RTC_WKUP_IRQHandler) \
    IRQ(FLASH_IRQHandler) \
    IRQ(RCC_IRQHandler) \
    IRQ(EXTI0_IRQHandler) \
    IRQ(EXTI1_IRQHandler) \
    IRQ(EXTI2_IRQHandler) \
    IRQ(EXTI3_IRQHandler) \
    IRQ(EXTI4_IRQHandler) \
    IRQ(DMA1_Stream0_IRQHandler) \
    IRQ(DMA1_Stream1_IRQHandler) \
    IRQ(DMA1_Stream2_IRQHandler) \
    IRQ(DMA1_Stream3_IRQHandler) \
    IRQ(DMA1_Stream4_IRQHandler) \
    IRQ(DMA1_Stream5_IRQHandler) \
    IRQ(DMA1_Stream6_IRQHandler) \
    IRQ(ADC_IRQHandler) \
    IRQ(FDCAN1_IT0_IRQHandler) \
    IRQ(FDCAN2_IT0_IRQHandler) \
    IRQ(FDCAN1_IT1_IRQHandler) \
    IRQ(FDCAN2_IT1_IRQHandler) \
    IRQ(EXTI9_5_IRQHandler) \
    IRQ(TIM1_BRK_IRQHandler) \
    IRQ(TIM1_UP_IRQHandler) \
    IRQ(TIM1_TRG_COM_IRQHandler) \
    IRQ(TIM1_CC_IRQHandler) \
    IRQ(TIM2_IRQHandler) \
    IRQ(TIM3_IRQHandler) \
    IRQ(TIM4_IRQHandler) \
    IRQ(I2C1_EV_IRQHandler) \
    IRQ(I2C1_ER_IRQHandler) \
    IRQ(I2C2_EV_IRQHandler) \
    IRQ(I2C2_ER_IRQHandler) \
    IRQ(SPI1_IRQHandler) \
    IRQ(SPI2_IRQHandler) \
    IRQ(USART1_IRQHandler) \
    IRQ(USART2_IRQHandler) \
    IRQ(USART3_IRQHandler) \
    IRQ(EXTI15_10_IRQHandler) \
    IRQ(RTC_Alarm_IRQHandler) \
    RESERVED() \
    IRQ(TIM8_BRK_TIM12_IRQHandler) \
    IRQ(TIM8_UP_TIM13_IRQHandler) \
    IRQ(TIM8_TRG_COM_TIM14_IRQHandler) \
    IRQ(TIM8_CC_IRQHandler) \
    IRQ(DMA1_Stream7_IRQHandler) \
    IRQ(FMC_IRQHandler) \
    IRQ(SDMMC1_IRQHandler) \
    IRQ(TIM5_IRQHandler) \
    IRQ(SPI3_IRQHandler) \
    IRQ(UART4_IRQHandler) \
    IRQ(UART5_IRQHandler) \
    IRQ(TIM6_DAC_IRQHandler) \
    IRQ(TIM7_IRQHandler) \
    IRQ(DMA2_Stream0_IRQHandler) \
    IRQ(DMA2_Stream1_IRQHandler) \
    IRQ(DMA2_Stream2_IRQHandler) \
    IRQ(DMA2_Stream3_IRQHandler) \
    IRQ(DMA2_Stream4_IRQHandler) \
    IRQ(ETH_IRQHandler) \
    IRQ(ETH_WKUP_IRQHandler) \
    IRQ(FDCAN_CAL_IRQHandler) \
    IRQ(CM7_SEV_IRQHandler) \
    IRQ(CM4_SEV_IRQHandler) \
    RESERVED() \
    RESERVED() \
    IRQ(DMA2_Stream5_IRQHandler) \
    IRQ(DMA2_Stream6_IRQHandler) \
    IRQ(DMA2_Stream7_IRQHandler) \
    IRQ(USART6_IRQHandler) \
    IRQ(I2C3_EV_IRQHandler) \
    IRQ(I2C3_ER_IRQHandler) \
    IRQ(OTG_HS_EP1_OUT_IRQHandler) \
    IRQ(OTG_HS_EP1_IN_IRQHandler) \
    IRQ(OTG_HS_WKUP_IRQHandler) \
    IRQ(OTG_HS_IRQHandler) \
    IRQ(DCMI_IRQHandler) \
    IRQ(CRYP_IRQHandler) \
    IRQ(HASH_RNG_IRQHandler) \
    IRQ(FPU_IRQHandler) \
    IRQ(UART7_IRQHandler) \
    IRQ(UART8_IRQHandler) \
    IRQ(SPI4_IRQHandler) \
    IRQ(SPI5_IRQHandler) \
    IRQ(SPI6_IRQHandler) \
    IRQ(SAI1_IRQHandler) \
    IRQ(LTDC_IRQHandler) \
    IRQ(LTDC_ER_IRQHandler) \
    IRQ(DMA2D_IRQHandler) \
    IRQ(SAI2_IRQHandler) \
    IRQ(QUADSPI_IRQHandler) \
    IRQ(LPTIM1_IRQHandler) \
    IRQ(CEC_IRQHandler) \
    IRQ(I2C4_EV_IRQHandler) \
    IRQ(I2C4_ER_IRQHandler) \
    IRQ(SPDIF_RX_IRQHandler) \
    IRQ(OTG_FS_EP1_OUT_IRQHandler) \
    IRQ(OTG_FS_EP1_IN_IRQHandler) \
    IRQ(OTG_FS_WKUP_IRQHandler) \
    IRQ(OTG_FS_IRQHandler) \
    IRQ(DMAMUX1_OVR_IRQHandler) \
    IRQ(HRTIM1_Master_IRQHandler) \
    IRQ(HRTIM1_TIMA_IRQHandler) \
    IRQ(HRTIM1_TIMB_IRQHandler) \
    IRQ(HRTIM1_TIMC_IRQHandler) \
    IRQ(HRTIM1_TIMD_IRQHandler) \
    IRQ(HRTIM1_TIME_IRQHandler) \
    IRQ(HRTIM1_FLT_IRQHandler) \
    IRQ(DFSDM1_FLT0_IRQHandler) \
    IRQ(DFSDM1_FLT1_IRQHandler) \
    IRQ(DFSDM1_FLT2_IRQHandler) \
    IRQ(DFSDM1_FLT3_IRQHandler) \
    IRQ(SAI3_IRQHandler) \
    IRQ(SWPMI1_IRQHandler) \
    IRQ(TIM15_IRQHandler) \
    IRQ(TIM16_IRQHandler) \
    IRQ(TIM17_IRQHandler) \
    IRQ(MDIOS_WKUP_IRQHandler) \
    IRQ(MDIOS_IRQHandler) \
    IRQ(JPEG_IRQHandler) \
    IRQ(MDMA_IRQHandler) \
    RESERVED() \
    IRQ(SDMMC2_IRQHandler) \
    IRQ(HSEM1_IRQHandler) \
    IRQ(HSEM2_IRQHandler) \
    IRQ(ADC3_IRQHandler) \
    IRQ(DMAMUX2_OVR_IRQHandler) \
    IRQ(BDMA_Channel0_IRQHandler) \
    IRQ(BDMA_Channel1_IRQHandler) \
    IRQ(BDMA_Channel2_IRQHandler) \
    IRQ(BDMA_Channel3_IRQHandler) \
    IRQ(BDMA_Channel4_IRQHandler) \
    IRQ(BDMA_Channel5_IRQHandler) \
    IRQ(BDMA_Channel6_IRQHandler) \
    IRQ(BDMA_Channel7_IRQHandler) \
    IRQ(COMP1_IRQHandler) \
    IRQ(LPTIM2_IRQHandler) \
    IRQ(LPTIM3_IRQHandler) \
    IRQ(LPTIM4_IRQHandler) \
    IRQ(LPTIM5_IRQHandler) \
    IRQ(LPUART1_IRQHandler) \
    IRQ(WWDG_RST_IRQHandler) \
    IRQ(CRS_IRQHandler) \
    IRQ(ECC_IRQHandler) \
    IRQ(SAI4_IRQHandler) \
    RESERVED() \
    IRQ(HOLD_CORE_IRQHandler) \
    IRQ(WAKEUP_PIN_IRQHandler)

/* All unused external IRQs share one default implementation. */
void das_stm32h755_default_irq_handler(void) {
    Default_Handler();
}

#define DAS_STM32H755_DEFINE_DEFAULT_IRQ(name) \
    void name(void) \
        __attribute__((weak, alias("das_stm32h755_default_irq_handler")));
#define DAS_STM32H755_DEFINE_RESERVED()
DAS_STM32H755_EXTERNAL_VECTOR_LIST(DAS_STM32H755_DEFINE_DEFAULT_IRQ,
                                    DAS_STM32H755_DEFINE_RESERVED)
#undef DAS_STM32H755_DEFINE_DEFAULT_IRQ
#undef DAS_STM32H755_DEFINE_RESERVED

/*
 * Default device vector table.
 *
 * The table itself is weak so firmware with a genuinely custom startup/vector
 * policy can replace g_das_vector_table wholesale. Normal applications should
 * keep this table and override only the standard handler symbols they own.
 */
__attribute__((weak, used, section(".isr_vector"), aligned(1024)))
const uintptr_t g_das_vector_table[] = {
    [0] = (uintptr_t)&__StackTop,
    [1] = (uintptr_t)&Reset_Handler,
    [2] = (uintptr_t)&NMI_Handler,
    [3] = (uintptr_t)&HardFault_Handler,
    [4] = (uintptr_t)&MemManage_Handler,
    [5] = (uintptr_t)&BusFault_Handler,
    [6] = (uintptr_t)&UsageFault_Handler,
    [7] = UINT32_C(0),
    [8] = UINT32_C(0),
    [9] = UINT32_C(0),
    [10] = UINT32_C(0),
    [11] = (uintptr_t)&SVC_Handler,
    [12] = (uintptr_t)&DebugMon_Handler,
    [13] = UINT32_C(0),
    [14] = (uintptr_t)&PendSV_Handler,
    [15] = (uintptr_t)&SysTick_Handler,
#define DAS_STM32H755_VECTOR_IRQ(name) (uintptr_t)&name,
#define DAS_STM32H755_VECTOR_RESERVED() UINT32_C(0),
    DAS_STM32H755_EXTERNAL_VECTOR_LIST(DAS_STM32H755_VECTOR_IRQ,
                                        DAS_STM32H755_VECTOR_RESERVED)
#undef DAS_STM32H755_VECTOR_IRQ
#undef DAS_STM32H755_VECTOR_RESERVED
};

_Static_assert(
    (sizeof(g_das_vector_table) / sizeof(g_das_vector_table[0])) ==
        (UINT32_C(16) + (uint32_t)WAKEUP_PIN_IRQn + UINT32_C(1)),
    "STM32H755 vector table must match CMSIS IRQ layout");

#undef DAS_STM32H755_EXTERNAL_VECTOR_LIST
