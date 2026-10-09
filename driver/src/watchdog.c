#include "watchdog.h"

#include "stm32f10x_iwdg.h"

#include <stdint.h>

#define WATCHDOG_RELOAD_MAX              UINT16_C(0x0FFF)
#define WATCHDOG_SYNC_WAIT_LIMIT         UINT32_C(100000)

Watchdog_Status_t Watchdog_StartMaxTimeout(void){
    uint32_t remaining = WATCHDOG_SYNC_WAIT_LIMIT;

    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_256);
    IWDG_SetReload(WATCHDOG_RELOAD_MAX);

    while((IWDG_GetFlagStatus(IWDG_FLAG_PVU) == SET) ||
          (IWDG_GetFlagStatus(IWDG_FLAG_RVU) == SET)){
        if(remaining == 0U){
            return WATCHDOG_SYNC_TIMEOUT;
        }
        --remaining;
    }

    IWDG_ReloadCounter();
    IWDG_Enable();
    return WATCHDOG_OK;
}

void Watchdog_Feed(void){
    IWDG_ReloadCounter();
}
