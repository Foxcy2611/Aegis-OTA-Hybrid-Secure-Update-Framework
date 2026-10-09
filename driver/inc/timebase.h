#ifndef AEGIS_TIMEBASE_H
#define AEGIS_TIMEBASE_H

/* Timebase 1 ms dùng chung cho mọi timeout hữu hạn phía STM32 Application. */

#include <stdbool.h>
#include <stdint.h>

typedef enum{
    TIMEBASE_OK = 0U,
    /* SysTick 1 ms đã được cấu hình. */

    TIMEBASE_INVALID_CLOCK
    /* SystemCoreClock không tạo được SysTick 1 kHz hợp lệ. */
} Timebase_Status_t;

Timebase_Status_t Timebase_Init(void);
uint32_t Timebase_Millis(void);
bool Timebase_HasElapsed(uint32_t start_ms, uint32_t timeout_ms);
void Timebase_DelayMs(uint32_t delay_ms);

#endif /* AEGIS_TIMEBASE_H */
