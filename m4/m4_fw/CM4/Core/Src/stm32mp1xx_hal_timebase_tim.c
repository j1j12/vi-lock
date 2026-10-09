/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32mp1xx_hal_timebase_tim.c
  * @brief   HAL time base based on the hardware TIM.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "stm32mp1xx_hal.h"
#include "stm32mp1xx_hal_tim.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef        htim6;
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  This function configures the TIM6 as a time base source.
  *         The time source is configured  to have 1ms time base with a dedicated
  *         Tick interrupt priority.
  * @note   This function is called  automatically at the beginning of program after
  *         reset by HAL_Init() or at any time when clock is configured, by HAL_RCC_ClockConfig().
  * @param  TickPriority: Tick interrupt priority.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  uint32_t              uwTimclock = 0;
  uint32_t              uwPrescalerValue = 0;
  uint32_t              divider;

  if (TickPriority >= (1UL << __NVIC_PRIO_BITS))
    return HAL_ERROR;

  /* TIM6 is in TIMG1: APB1 division and TIMG1PRE can multiply PCLK1.
     Use the BSP's complete kernel-clock calculation, not PCLK1 alone. */
  uwTimclock = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_TIM6);
  if (uwTimclock < 10000U)
    return HAL_ERROR;
  /* A 10 kHz counter supports all HAL tick periods (1/10/100 ms) within
     TIM6's 16-bit ARR. Round the divider to minimize quantization error. */
  divider = uwTimclock / 10000U + ((uwTimclock % 10000U) >= 5000U);
  if (divider == 0 || divider > 65536U ||
      (uwTickFreq != HAL_TICK_FREQ_1KHZ &&
       uwTickFreq != HAL_TICK_FREQ_100HZ &&
       uwTickFreq != HAL_TICK_FREQ_10HZ))
    return HAL_ERROR;
  uwPrescalerValue = divider - 1U;

  /* Enable TIM6 clock */
  __HAL_RCC_TIM6_CLK_ENABLE();
  __HAL_RCC_TIM6_FORCE_RESET();
  __HAL_RCC_TIM6_RELEASE_RESET();

  /* Initialize TIM6 */
  htim6.Instance = TIM6;

  /* Initialize TIMx peripheral as follow:
   * Counter approximately 10 kHz; 10 counts per millisecond.
   * HAL_IncTick adds uwTickFreq milliseconds per interrupt.
   * ClockDivision = 0
   * Counter direction = Up
   */
  htim6.Init.Period = 10U * (uint32_t)uwTickFreq - 1U;
  htim6.Init.Prescaler = uwPrescalerValue;
  htim6.Init.ClockDivision = 0;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;

  if(HAL_TIM_Base_Init(&htim6) == HAL_OK)
  {

    /*Configure the TIM6 IRQ priority */
     HAL_NVIC_SetPriority(TIM6_IRQn, TickPriority ,0);
     uwTickPrio = TickPriority;

    /* Enable the TIM6 global Interrupt */
     HAL_NVIC_EnableIRQ(TIM6_IRQn);

    /* Start the TIM time Base generation in interrupt mode */
    return HAL_TIM_Base_Start_IT(&htim6);
  }

  /* Return function status */
  return HAL_ERROR;
}

/**
  * @brief  Suspend Tick increment.
  * @note   Disable the tick increment by disabling TIM6 update interrupt.
  * @param  None
  * @retval None
  */
void HAL_SuspendTick(void)
{
  /* Disable TIM6 update Interrupt */
  __HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);
}

/**
  * @brief  Resume Tick increment.
  * @note   Enable the tick increment by Enabling TIM6 update interrupt.
  * @param  None
  * @retval None
  */
void HAL_ResumeTick(void)
{
  /* Enable TIM6 Update interrupt */
  __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
}

