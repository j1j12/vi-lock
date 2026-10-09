/**
  ******************************************************************************
  * @file    log.c
  * @author  MCD Application Team
  * @brief   Ressource table
  *
  *   This file provides services for logging
  *
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

/** @addtogroup STM32MP1xx_log
  * @{
  */

/** @addtogroup STM32MP1xx_Log_Private_Includes
  * @{
  */
#include "openamp_log.h"
#include "trace_format.h"
/**
  * @}
  */

/** @addtogroup STM32MP1xx_Log_Private_TypesDefinitions
  * @{
  */

/**
  * @}
  */

/** @addtogroup STM32MP1xx_Log_Private_Defines
  * @{
  */

/**
  * @}
  */

#if defined (__LOG_TRACE_IO_)
char system_log_buf[SYSTEM_TRACE_BUF_SZ];

#define TRACE_HEADER_SIZE 128

void trace_checkpoint(uint32_t phase, uint32_t value)
{
  const uint32_t words[] = {phase, value, SCB->CFSR, HAL_GetTick()};
  const char *names[] = {"phase=", "value=", "cfsr=", "tick="};
  volatile char *dst = system_log_buf;
  unsigned int pos = 0, i;
  /* Do not insert NULs in the header: debugfs trace is a C string. */
  for (i = 0; i < 4; ++i) {
    const char *p = names[i];
    int shift;
    while (*p) dst[pos++] = *p++;
    for (shift = 28; shift >= 0; shift -= 4)
      dst[pos++] = "0123456789abcdef"[(words[i] >> shift) & 15];
    dst[pos++] = ' ';
  }
  while (pos < TRACE_HEADER_SIZE - 1) dst[pos++] = ' ';
  dst[pos] = '\n';
  __DSB();
}

__weak void log_buff(int ch)
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the USART1 and Loop until the end of transmission */
 static int offset = 0;

	if (offset < TRACE_HEADER_SIZE || offset + 1 >= SYSTEM_TRACE_BUF_SZ)
		offset = TRACE_HEADER_SIZE;

	system_log_buf[offset] = ch;
	system_log_buf[offset++ + 1] = '\0';
}

int trace_printf(const char *format, ...)
{
  va_list args;
  int result;
  va_start(args, format);
  result = trace_vformat(log_buff, format, args);
  va_end(args);
  __DSB();
  return result;
}

#else
void trace_checkpoint(uint32_t phase, uint32_t value)
{
  (void)phase;
  (void)value;
}
#endif

#if defined ( __CC_ARM) || (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
#define PUTCHAR_PROTOTYPE int stdout_putchar(int ch)
#elif __GNUC__
/* With GCC/RAISONANCE, small log_info (option LD Linker->Libraries->Small log_info
   set to 'Yes') calls __io_putchar() */
#define PUTCHAR_PROTOTYPE int __attribute__(( weak )) __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int __attribute__(( weak )) fputc(int ch, FILE *f)
#endif /* __GNUC__ */

#if defined (__LOG_UART_IO_) || defined (__LOG_TRACE_IO_)
PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the USART1 and Loop until the end of transmission */
#if defined (__LOG_UART_IO_)
extern UART_HandleTypeDef huart;
  HAL_UART_Transmit(&huart, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
#endif
#if defined (__LOG_TRACE_IO_)
	log_buff(ch);
#endif
	return ch;
}
#else
/* No printf output */
#endif
