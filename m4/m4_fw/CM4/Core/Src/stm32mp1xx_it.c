/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32mp1xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "main.h"
#include "stm32mp1xx_it.h"
#include "openamp_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ipcc.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* No printf, heap, RTOS locks or HAL calls in a fault handler. */
static void trace_fault(void)
{
#if defined(SERVO_MANUAL_TEST)
  extern void ServoTest_Stop(void);
  __disable_irq();
  ServoTest_Stop();
#endif
  trace_checkpoint(0xf0000000UL | __get_IPSR(), SCB->CFSR);
#if defined(__LOG_TRACE_IO_)
  extern void log_buff(int ch);
  static const char labels[][8] = {" IPSR=", " CFSR=", " HFSR=", " BFAR=", " MMFAR="};
  const uint32_t values[] = {__get_IPSR(), SCB->CFSR, SCB->HFSR, SCB->BFAR, SCB->MMFAR};
  const char *title = "\nFAULT";
  unsigned int i;
  while (*title) log_buff(*title++);
  for (i = 0; i < 5; ++i) {
    const char *label = labels[i];
    int shift;
    while (*label) log_buff(*label++);
    for (shift = 28; shift >= 0; shift -= 4)
      log_buff("0123456789abcdef"[(values[i] >> shift) & 15]);
  }
  log_buff('\n');
  __DSB();
#endif
}

void App_AssertFailed(unsigned long line)
{
  taskDISABLE_INTERRUPTS();
  trace_checkpoint(0xa0000000UL, line);
  for (;;) { }
}

void App_UnhandledInterrupt(void)
{
  __disable_irq();
  trace_checkpoint(0xd0000000UL | __get_IPSR(), SCB->ICSR);
  for (;;) { }
}

void IPCC_RX1_IRQHandler(void)
{
  HAL_IPCC_RX_IRQHandler(&hipcc);
}

void IPCC_TX1_IRQHandler(void)
{
  HAL_IPCC_TX_IRQHandler(&hipcc);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
  (void)name;
  taskDISABLE_INTERRUPTS();
  trace_checkpoint(0xb0000000UL, (uint32_t)task);
  for (;;) { }
}

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern TIM_HandleTypeDef htim6;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  trace_fault();
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  trace_fault();
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  trace_fault();
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  trace_fault();
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32MP1xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32mp1xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles TIM6 global interrupt.
  */
void TIM6_IRQHandler(void)
{
  /* USER CODE BEGIN TIM6_IRQn 0 */

  /* USER CODE END TIM6_IRQn 0 */
  if (htim6.Instance != NULL) {
    HAL_TIM_IRQHandler(&htim6);
  }
  else
  {
  /* Disable theTIM6global Interrupt */
    HAL_NVIC_DisableIRQ(TIM6_IRQn);
  }
  /* USER CODE BEGIN TIM6_IRQn 1 */

  /* USER CODE END TIM6_IRQn 1 */
}

/**
  * @brief This function handles RCC wake-up interrupt.
  */
void RCC_WAKEUP_IRQHandler(void)
{
  /* USER CODE BEGIN RCC_WAKEUP_IRQn 0 */

  /* USER CODE END RCC_WAKEUP_IRQn 0 */
  HAL_RCC_WAKEUP_IRQHandler();
  /* USER CODE BEGIN RCC_WAKEUP_IRQn 1 */

  /* USER CODE END RCC_WAKEUP_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
