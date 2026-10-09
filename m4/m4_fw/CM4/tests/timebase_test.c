#include <assert.h>
#include <stdio.h>
#include "stm32mp1xx_hal.h"
HAL_TickFreqTypeDef uwTickFreq = HAL_TICK_FREQ_1KHZ;
uint32_t uwTickPrio;
static uint32_t clock_hz = 200000000;
static HAL_StatusTypeDef init_status = HAL_OK, start_status = HAL_OK;
uint32_t HAL_RCCEx_GetPeriphCLKFreq(uint64_t peripheral)
{ assert(peripheral == RCC_PERIPHCLK_TIM6); return clock_hz; }
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *timer)
{ assert(timer->Instance == TIM6); return init_status; }
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *timer)
{ assert(timer->Instance == TIM6); return start_status; }
void HAL_NVIC_SetPriority(int irq, uint32_t priority, uint32_t subpriority)
{ assert(irq == TIM6_IRQn && priority == 3 && subpriority == 0); }
void HAL_NVIC_EnableIRQ(int irq) { assert(irq == TIM6_IRQn); }
/* Compile the real implementation against a fake hardware boundary. */
#include "../Core/Src/stm32mp1xx_hal_timebase_tim.c"
int main(void)
{
    const uint32_t clocks[] = {64000000, 200000000, 209000000, 209123456, 400000000, 655360000};
    const HAL_TickFreqTypeDef periods[] = {HAL_TICK_FREQ_1KHZ, HAL_TICK_FREQ_100HZ, HAL_TICK_FREQ_10HZ};
    unsigned int i, j;
    for (i=0; i<sizeof(clocks)/sizeof(clocks[0]); ++i) {
        for (j=0; j<sizeof(periods)/sizeof(periods[0]); ++j) {
            double ms_per_second;
            clock_hz = clocks[i]; uwTickFreq = periods[j];
            assert(HAL_InitTick(3) == HAL_OK);
            assert(htim6.Init.Prescaler <= 65535 && htim6.Init.Period <= 65535);
            assert(uwTickPrio == 3);
            ms_per_second = (double)clock_hz / (htim6.Init.Prescaler+1) /
                            (htim6.Init.Period+1) * uwTickFreq;
            assert(ms_per_second > 999 && ms_per_second < 1001);
        }
    }
    clock_hz=200000000; uwTickFreq=HAL_TICK_FREQ_1KHZ;
    assert(HAL_InitTick(3) == HAL_OK);
    assert(htim6.Init.Prescaler == 19999 && htim6.Init.Period == 9);
    assert(HAL_InitTick(16) == HAL_ERROR);
    clock_hz=0; assert(HAL_InitTick(3) == HAL_ERROR);
    clock_hz=UINT32_MAX; assert(HAL_InitTick(3) == HAL_ERROR);
    clock_hz=200000000; uwTickFreq=(HAL_TickFreqTypeDef)2;
    assert(HAL_InitTick(3) == HAL_ERROR);
    uwTickFreq=HAL_TICK_FREQ_1KHZ; init_status=HAL_ERROR;
    assert(HAL_InitTick(3) == HAL_ERROR);
    init_status=HAL_OK; start_status=HAL_ERROR;
    assert(HAL_InitTick(3) == HAL_ERROR);
    puts("TIM6 real-source tests: 18 clock/period cases, exact divider and 6 failure cases PASS");
    return 0;
}
