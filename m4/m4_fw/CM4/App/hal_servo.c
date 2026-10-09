#include "hal_servo.h"
#include "main.h"
#include "tim.h"
#include "servo_timing.h"
#include "servo_cycle.h"
#if defined(SERVO_MANUAL_TEST)
#include "openamp_log.h"

static volatile uint32_t test_periods;
static volatile uint32_t test_phase;
static uint32_t test_hz, test_divider;
static uint32_t test_last_start;
static uint8_t test_started;
static volatile uint8_t test_ready;

/* Caller has interrupts masked, or is the dedicated TIM5 handler. No HAL
 * locks, allocation, RTOS calls or logging in this stop path. */
void ServoTest_Stop(void)
{
    if (!test_ready) return; /* Early faults before peripheral clocks exist. */
    TIM5->DIER = 0;
    TIM5->CCER = 0;
    TIM5->CR1 &= ~TIM_CR1_CEN;
    TIM5->CCR4 = 0;
    GPIOI->BSRR = GPIO_PIN_0 << 16U;
    GPIOI->MODER = (GPIOI->MODER & ~3UL) | 1UL; /* PI0 output low */
    TIM5->SR = 0;
    test_periods = 0;
    test_phase = 0;
    __DSB();
}

void TIM5_IRQHandler(void)
{
    if ((TIM5->SR & TIM_SR_UIF) && (TIM5->DIER & TIM_DIER_UIE)) {
        TIM5->SR = ~TIM_SR_UIF;
        uint32_t action = ServoCycle_Tick(&test_periods, &test_phase);
        if (action == 1) {
            TIM5->CCER = 0;
            GPIOI->BSRR = GPIO_PIN_0 << 16U;
            GPIOI->MODER = (GPIOI->MODER & ~3UL) | 1UL;
        } else if (action == 2) {
            TIM5->CCR4 = Servo_Counts(test_hz, test_divider, 1500);
            TIM5->EGR = TIM_EGR_UG;
            TIM5->CNT = 0;
            TIM5->SR = 0;
            test_last_start = HAL_GetTick();
            GPIOI->MODER = (GPIOI->MODER & ~3UL) | 2UL;
            TIM5->CCER = TIM_CCER_CC4E;
        } else if (action == 3) ServoTest_Stop();
    }
}

void ServoTest_Init(void)
{
    test_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_TIM5);
    test_divider = Servo_Divider(test_hz);
    __HAL_RCC_GPIOI_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_0, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOI, &gpio);
    test_ready = 1;
    ServoTest_Stop();
    /* Above RTOS syscall IRQ priorities; handler makes no RTOS calls. */
    HAL_NVIC_SetPriority(TIM5_IRQn, 0, 0);
    HAL_NVIC_ClearPendingIRQ(TIM5_IRQn);
    HAL_NVIC_EnableIRQ(TIM5_IRQn);
#if defined(SERVO_ONESHOT_TEST)
    log_info("SERVO v13: idle-low; explicit one-shot cycle supported\n");
#else
    log_info("SERVO MANUAL v11: idle-low; status query; no auto unlock\n");
#endif
}

uint32_t ServoTest_State(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    uint32_t active = test_periods || (TIM5->CCER & TIM_CCER_CC4E) ||
                      (TIM5->CR1 & TIM_CR1_CEN);
    __set_PRIMASK(mask);
    return active ? 1u : 0u;
}

/* Diagnostic only. 0=stop, 1500/1600=one burst. Result codes: 0 stopped,
 * 1 started, 2 busy/cooldown, 3 invalid. No requests are queued. */
uint32_t ServoTest_Command(uint32_t us)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (!us) { ServoTest_Stop(); __set_PRIMASK(mask); return 0; }
    if ((us != 1500 && us != 1600) || !test_divider) {
        __set_PRIMASK(mask); return 3;
    }
    uint32_t now = HAL_GetTick();
    if (test_periods || (test_started && (uint32_t)(now-test_last_start) < 3000)) {
        __set_PRIMASK(mask); return 2;
    }
    TIM5->CCER = 0;
    TIM5->CR1 &= ~TIM_CR1_CEN;
    TIM5->CCR4 = Servo_Counts(test_hz, test_divider, us);
    TIM5->EGR = TIM_EGR_UG;
    TIM5->CNT = 0;
    TIM5->SR = 0;
    HAL_NVIC_ClearPendingIRQ(TIM5_IRQn);
    test_periods = 50;
    test_last_start = now;
    test_started = 1;
    GPIOI->AFR[0] = (GPIOI->AFR[0] & ~15UL) | GPIO_AF2_TIM5;
    GPIOI->MODER = (GPIOI->MODER & ~3UL) | 2UL;
    TIM5->DIER = TIM_DIER_UIE;
    TIM5->CCER = TIM_CCER_CC4E;
    TIM5->CR1 |= TIM_CR1_CEN;
    __DSB();
    __set_PRIMASK(mask);
    return 1;
}

#if defined(SERVO_ONESHOT_TEST)
uint32_t ServoTest_Cycle(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    uint32_t result = ServoTest_Command(1600);
    if (result == 1) test_phase = 1;
    __set_PRIMASK(mask);
    return result;
}
#endif
#endif

/*
 * SG90 servo, TIM5_CH4 (PI0).
 *
 * Read the existing TIM5 kernel clock; never assume engineering-mode HSI.
 * Pulse endpoints remain historical experimental values, not calibrated
 * mechanical travel limits. Do not deploy until pin ownership and power pass.
 */
static uint32_t timer_hz;
static uint32_t timer_divider;

#define SERVO_PULSE_MIN_US   500u       /* 0.5 ms -> 0 deg */
#define SERVO_PULSE_MAX_US   2500u      /* 2.5 ms -> 180 deg */

static uint32_t AngleToPulse(uint16_t angle_deg)
{
    uint32_t a = angle_deg;
    if (a > SERVO_ANGLE_MAX) {
        a = SERVO_ANGLE_MAX;
    }
    uint32_t us = SERVO_PULSE_MIN_US
           + (a * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US)) / SERVO_ANGLE_MAX;
    return Servo_Counts(timer_hz, timer_divider, us);
}

void Servo_Init(void)
{
    HAL_TIM_PWM_Stop(&htim5, TIM_CHANNEL_4);
    timer_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_TIM5);
    timer_divider = Servo_Divider(timer_hz);
    uint32_t period = Servo_Counts(timer_hz, timer_divider, 20000u);
    if (!timer_divider || period < 2u) {
        Error_Handler();
        return;
    }

    __HAL_TIM_SET_PRESCALER(&htim5, timer_divider - 1u);
    __HAL_TIM_SET_AUTORELOAD(&htim5, period - 1u);
    __HAL_TIM_SET_COUNTER(&htim5, 0);

    /* Load prescaler/auto-reload shadow registers */
    __HAL_TIM_URS_ENABLE(&htim5);
    htim5.Instance->EGR = TIM_EGR_UG;
    __HAL_TIM_URS_DISABLE(&htim5);

    __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_4, AngleToPulse(SERVO_ANGLE_MIN));

    if (HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_4) != HAL_OK) Error_Handler();
}

void Servo_SetAngle(uint16_t angle_deg)
{
    __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_4, AngleToPulse(angle_deg));
}
