#include "timebase.h"

#include "stm32f10x.h"

static volatile uint32_t timebase_milliseconds = 0U;
static bool timebase_initialized = false;

Timebase_Status_t Timebase_Init(void){
    if(timebase_initialized){
        return TIMEBASE_OK;
    }

    SystemCoreClockUpdate();
    if((SystemCoreClock < 1000U) ||
       (SysTick_Config(SystemCoreClock / 1000U) != 0U)){
        return TIMEBASE_INVALID_CLOCK;
    }

    timebase_milliseconds = 0U;
    timebase_initialized = true;
    return TIMEBASE_OK;
}

uint32_t Timebase_Millis(void){
    return timebase_milliseconds;
}

bool Timebase_HasElapsed(uint32_t start_ms, uint32_t timeout_ms){
    return (uint32_t)(Timebase_Millis() - start_ms) >= timeout_ms;
}

void Timebase_DelayMs(uint32_t delay_ms){
    const uint32_t start_ms = Timebase_Millis();

    while(!Timebase_HasElapsed(start_ms, delay_ms)){
        __NOP();
    }
}

void SysTick_Handler(void){
    ++timebase_milliseconds;
}
