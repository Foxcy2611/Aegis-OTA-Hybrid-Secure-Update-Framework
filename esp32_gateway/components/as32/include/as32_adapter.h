#ifndef AEGIS_ESP32_AS32_ADAPTER_H
#define AEGIS_ESP32_AS32_ADAPTER_H

/*
 * Adapter AS32 phía ESP32 Gateway.
 *
 * Module này chỉ sở hữu UART, MD0/MD1, AUX, mode và timeout. Nó không parse
 * OTA, không ký firmware và không quyết định artifact có hợp lệ hay không.
 */

#include "driver/gpio.h"
#include "driver/uart.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AS32_ADAPTER_CONFIG_SIZE                 5U
#define AS32_ADAPTER_RESPONSE_SIZE               6U
#define AS32_ADAPTER_VERSION_MAX_SIZE            32U
#define AS32_ADAPTER_MAX_TRANSFER_SIZE           512U
#define AS32_ADAPTER_DEFAULT_TIMEOUT_MS          1000U

typedef enum{
    AS32_ADAPTER_OK = 0U,
    /* Thao tác hoàn thành. */

    AS32_ADAPTER_INVALID_ARGUMENT,
    /* Con trỏ, length, timeout hoặc cấu hình đầu vào không hợp lệ. */

    AS32_ADAPTER_NOT_READY,
    /* Driver chưa Init hoặc module chưa ở đúng mode. */

    AS32_ADAPTER_BUSY,
    /* AUX đang low trong phép kiểm tra non-blocking. */

    AS32_ADAPTER_TIMEOUT,
    /* UART/AUX không hoàn thành trước deadline. */

    AS32_ADAPTER_UART_ERROR,
    /* ESP-IDF UART driver trả lỗi hoặc không gửi/nhận đủ byte. */

    AS32_ADAPTER_CONFIG_ERROR,
    /* Response/read-back config sai format hoặc sai giá trị. */

    AS32_ADAPTER_IO_ERROR,
    /* Không cấu hình được UART/GPIO hoặc không cài được driver. */

    AS32_ADAPTER_OUT_OF_RANGE
    /* Length/config field vượt contract. */
} As32Adapter_Status_t;

typedef enum{
    AS32_ADAPTER_MODE_NORMAL = 0U,
    /* MD0=0, MD1=0: transparent UART <-> LoRa. */

    AS32_ADAPTER_MODE_CONFIG
    /* MD0=1, MD1=1: command/configuration mode. */
} As32Adapter_Mode_t;

typedef struct{
    uart_port_t uart_port;
    /* UART controller nối AS32; mặc định UART2. */

    gpio_num_t tx_pin;
    /* ESP32 TX nối AS32 RX. */

    gpio_num_t rx_pin;
    /* ESP32 RX nối AS32 TX. */

    gpio_num_t md0_pin;
    /* Chân chọn mode MD0. */

    gpio_num_t md1_pin;
    /* Chân chọn mode MD1. */

    gpio_num_t aux_pin;
    /* Chân AUX báo module ready/busy. */

    uint32_t baud_rate;
    /* UART link với AS32; contract Phase 4 là 9600. */
} As32Adapter_Hardware_t;

typedef struct{
    uint8_t address_high;
    uint8_t address_low;
    uint8_t speed;
    uint8_t channel;
    uint8_t option;
} As32Adapter_Config_t;

As32Adapter_Status_t As32Adapter_Init(
    const As32Adapter_Hardware_t *hardware
);
As32Adapter_Status_t As32Adapter_EnterConfig(uint32_t timeout_ms);
As32Adapter_Status_t As32Adapter_EnterNormal(uint32_t timeout_ms);
As32Adapter_Status_t As32Adapter_WaitAuxReady(uint32_t timeout_ms);

As32Adapter_Status_t As32Adapter_ReadConfig(
    As32Adapter_Config_t *config,
    uint32_t timeout_ms
);
As32Adapter_Status_t As32Adapter_WriteConfig(
    const As32Adapter_Config_t *config,
    bool persist,
    uint32_t timeout_ms
);
As32Adapter_Status_t As32Adapter_ReadVersion(
    uint8_t *version,
    size_t capacity,
    size_t *version_length,
    uint32_t timeout_ms
);
As32Adapter_Status_t As32Adapter_Reset(uint32_t timeout_ms);

As32Adapter_Status_t As32Adapter_Send(
    const uint8_t *data,
    size_t length,
    uint32_t timeout_ms
);
As32Adapter_Status_t As32Adapter_Receive(
    uint8_t *data,
    size_t requested_length,
    size_t *received_length,
    uint32_t timeout_ms
);

size_t As32Adapter_Available(void);
void As32Adapter_FlushRx(void);
As32Adapter_Status_t As32Adapter_GetStatus(void);
void As32Adapter_ClearError(void);

#endif /* AEGIS_ESP32_AS32_ADAPTER_H */
