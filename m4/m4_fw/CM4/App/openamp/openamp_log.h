/**
  ******************************************************************************
  * @file    log.h
  * @author  MCD Application Team
  * @brief   logging services
  ******************************************************************************
  *
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  *
  ******************************************************************************
  */

/** @addtogroup LOG
  * @{
  */

/** @addtogroup stm32mp1xx_Log
  * @{
  */

/**
  * @brief Define to prevent recursive inclusion
  */
#ifndef __LOG_STM32MP1XX_H
#define __LOG_STM32MP1XX_H

#ifdef __cplusplus
 extern "C" {
#endif

/** @addtogroup STM32MP1xx_Log_Includes
  * @{
  */
#include "stm32mp1xx_hal.h"
#include "stdio.h"
/* Fixed trace header: no printf, allocation, locks or RTOS dependency. */
void trace_checkpoint(uint32_t phase, uint32_t value);
#if defined(__LOG_TRACE_IO_)
int trace_printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
#define OPENAMP_LOG_PRINT trace_printf
#else
#define OPENAMP_LOG_PRINT printf
#endif
/**
  * @}
  */

/** @addtogroup STM32MP1xx_Log_Exported_Constants
  * @{
  */
#if defined (__LOG_TRACE_IO_)
#define SYSTEM_TRACE_BUF_SZ 8192
#endif

#define LOGQUIET 0
#define	LOGERR	  1
#define	LOGWARN  2
#define	LOGINFO  3
#define	LOGDBG   4

#ifndef LOGLEVEL
#define LOGLEVEL LOGINFO
#endif


/**
  * @}
  */

/** @addtogroup STM32MP1xx_Log_Exported_types
  * @{
  */
#if defined (__LOG_TRACE_IO_)
extern char system_log_buf[SYSTEM_TRACE_BUF_SZ]; /*!< buffer for debug traces */
#endif /* __LOG_TRACE_IO_ */
/**
  * @}
  */

/** @addtogroup STM32MP1xx_Log_Exported_Macros
  * @{
  */
#if defined (__LOG_TRACE_IO_) || defined(__LOG_UART_IO_)
#if LOGLEVEL >= LOGDBG
#define log_dbg(fmt, ...)  OPENAMP_LOG_PRINT("[%05lu.%03lu][DBG  ]" fmt, (unsigned long)(HAL_GetTick()/1000), (unsigned long)(HAL_GetTick() % 1000), ##__VA_ARGS__)
#else
#define log_dbg(fmt, ...)
#endif
#if LOGLEVEL >= LOGINFO
#define log_info(fmt, ...) OPENAMP_LOG_PRINT("[%05lu.%03lu][INFO ]" fmt, (unsigned long)(HAL_GetTick()/1000), (unsigned long)(HAL_GetTick() % 1000), ##__VA_ARGS__)
#else
#define log_info(fmt, ...)
#endif
#if LOGLEVEL >= LOGWARN
#define log_warn(fmt, ...) OPENAMP_LOG_PRINT("[%05lu.%03lu][WARN ]" fmt, (unsigned long)(HAL_GetTick()/1000), (unsigned long)(HAL_GetTick() % 1000), ##__VA_ARGS__)
#else
#define log_warn(fmt, ...)
#endif
#if LOGLEVEL >= LOGERR
#define log_err(fmt, ...)  OPENAMP_LOG_PRINT("[%05lu.%03lu][ERR  ]" fmt, (unsigned long)(HAL_GetTick()/1000), (unsigned long)(HAL_GetTick() % 1000), ##__VA_ARGS__)
#else
#define log_err(fmt, ...)
#endif
#else
#define log_dbg(fmt, ...)
#define log_info(fmt, ...)
#define log_warn(fmt, ...)
#define log_err(fmt, ...)
#endif /* __LOG_TRACE_IO_ */
/**
  * @}
  */

/** @addtogroup STM32MP1xx_Log_Exported_Functions
  * @{
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /*__LOG_STM32MP1XX_H */

/**
  * @}
  */

/**
  * @}
  */
