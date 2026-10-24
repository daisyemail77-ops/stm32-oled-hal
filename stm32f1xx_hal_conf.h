/**
  ******************************************************************************
  * @file    stm32f1xx_hal_conf.h
  * @brief   HAL 库配置文件（精简版：仅启用 FLASH / GPIO / RCC / CORTEX）
  ******************************************************************************
  */

#ifndef __STM32F1xx_HAL_CONF_H
#define __STM32F1xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 模块使能开关 ==================== */
#define HAL_MODULE_ENABLED
/* #define HAL_ADC_MODULE_ENABLED   */
/* #define HAL_CAN_MODULE_ENABLED   */
/* #define HAL_DAC_MODULE_ENABLED   */
#define HAL_DMA_MODULE_ENABLED
/* #define HAL_EXTI_MODULE_ENABLED  */
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
/* #define HAL_IWDG_MODULE_ENABLED  */
/* #define HAL_PWR_MODULE_ENABLED   */
#define HAL_RCC_MODULE_ENABLED
/* #define HAL_RTC_MODULE_ENABLED   */
/* #define HAL_SPI_MODULE_ENABLED   */
/* #define HAL_TIM_MODULE_ENABLED   */
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
/* #define HAL_USART_MODULE_ENABLED */
/* #define HAL_WWDG_MODULE_ENABLED  */
#define HAL_CORTEX_MODULE_ENABLED
/* #define HAL_SRAM_MODULE_ENABLED  */
/* #define HAL_FLASH_MODULE_ENABLED */

/* ==================== 各模块头文件 ==================== */
#include "stm32f1xx_hal_rcc.h"
#include "stm32f1xx_hal_flash.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_cortex.h"
#include "stm32f1xx_hal_dma.h"
#include "stm32f1xx_hal_tim.h"
#include "stm32f1xx_hal_i2c.h"
#include "stm32f1xx_hal_uart.h"

/* ==================== 振荡器相关参数 ==================== */
#if !defined(HSE_VALUE)
#define HSE_VALUE    8000000U  /*!< 外部 8MHz 晶振 */
#endif

#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT 100U
#endif

#if !defined(HSI_VALUE)
#define HSI_VALUE    8000000U  /*!< 内部 8MHz RC 振荡器 */
#endif

#if !defined(LSI_VALUE)
#define LSI_VALUE    40000U    /*!< 内部 40kHz RC */
#endif

#if !defined(LSE_VALUE)
#define LSE_VALUE    32768U    /*!< 外部 32.768kHz 晶振 */
#endif

#if !defined(LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT 5000U
#endif

/* ==================== 时基（SysTick）配置 ==================== */
#define TICK_INT_PRIORITY    0U
#define USE_RTOS             0U
#define PREFETCH_ENABLE      1U

/* ==================== 断言 ==================== */
#ifdef USE_FULL_ASSERT
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
void assert_failed(uint8_t *file, uint32_t line);
#else
#define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __STM32F1xx_HAL_CONF_H */
