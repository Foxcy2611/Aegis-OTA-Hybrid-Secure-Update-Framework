#ifndef AEGIS_LORA_AS32_H
#define AEGIS_LORA_AS32_H

/*
 * Adapter mức thấp cho AS32-TTL-100-C phía STM32 Application.
 *
 * Module sở hữu USART1, MD0/MD1, AUX, ring buffer RX và timeout. Nó không biết
 * secure frame, OTA command, W25Q32 hoặc firmware validity.
 */

#include <stdbool.h>
#include <stdint.h>

#define LORA_AS32_CONFIG_REGISTER_SIZE          5U
#define LORA_AS32_CONFIG_RESPONSE_SIZE          6U
#define LORA_AS32_RX_RING_CAPACITY              512U
#define LORA_AS32_MAX_TRANSFER_SIZE             512U
#define LORA_AS32_VERSION_MAX_SIZE              32U

#define LORA_AS32_DEFAULT_TIMEOUT_MS            1000U
#define LORA_AS32_AUX_STABLE_TIME_MS            2U
#define LORA_AS32_MODE_GUARD_TIME_MS            5U
#define LORA_AS32_RESPONSE_IDLE_TIME_MS         10U

typedef enum{
    LORA_AS32_OK = 0U,
    /* Thao tác hoàn thành. */

    LORA_AS32_INVALID_ARGUMENT,
    /* Con trỏ NULL, length bằng 0 hoặc timeout/config không hợp lệ. */

    LORA_AS32_NOT_READY,
    /* Driver chưa Init hoặc module chưa ở mode cần thiết. */

    LORA_AS32_BUSY,
    /* AUX đang low trong phép kiểm tra non-blocking. */

    LORA_AS32_TIMEOUT,
    /* UART hoặc AUX không hoàn thành trước deadline. */

    LORA_AS32_UART_ERROR,
    /* USART báo parity/framing/noise/overrun. */

    LORA_AS32_CONFIG_ERROR,
    /* Response/config read-back sai format hoặc khác giá trị đã ghi. */

    LORA_AS32_IO_ERROR,
    /* Peripheral/timebase không khởi tạo được. */

    LORA_AS32_OUT_OF_RANGE,
    /* Length/config field vượt contract của AS32. */

    LORA_AS32_RX_OVERFLOW
    /* ISR phải bỏ byte vì ring buffer đầy. */
} LoraAs32_Status_t;

typedef enum{
    LORA_AS32_MODE_NORMAL = 0U,
    /* MD0=0, MD1=0: transparent UART <-> LoRa. */

    LORA_AS32_MODE_CONFIG
    /* MD0=1, MD1=1: command/configuration mode. */
} LoraAs32_Mode_t;

typedef struct{
    uint8_t address_high;
    /* Byte địa chỉ cao của module. */

    uint8_t address_low;
    /* Byte địa chỉ thấp của module. */

    uint8_t speed;
    /* [7:6] parity, [5:3] UART baud, [2:0] air rate. */

    uint8_t channel;
    /* Kênh 0x00..0x1F; frequency contract do hardware profile quyết định. */

    uint8_t option;
    /* Fixed/transparent, IO type, wake time và transmit power. */
} LoraAs32_Config_t;

/* Pure helpers dùng chung cho driver và host regression. */
bool LoraAs32_ConfigIsSupported(const LoraAs32_Config_t *config);
LoraAs32_Status_t LoraAs32_DecodeConfigResponse(
    const uint8_t response[LORA_AS32_CONFIG_RESPONSE_SIZE],
    LoraAs32_Config_t *config
);
LoraAs32_Status_t LoraAs32_BuildWriteCommand(
    const LoraAs32_Config_t *config,
    bool persist,
    uint8_t command[LORA_AS32_CONFIG_RESPONSE_SIZE]
);

LoraAs32_Status_t LoraAs32_Init(void);
LoraAs32_Status_t LoraAs32_EnterConfig(uint32_t timeout_ms);
LoraAs32_Status_t LoraAs32_EnterNormal(uint32_t timeout_ms);
LoraAs32_Status_t LoraAs32_WaitAuxReady(uint32_t timeout_ms);

LoraAs32_Status_t LoraAs32_ReadConfig(
    LoraAs32_Config_t *config,
    uint32_t timeout_ms
);

LoraAs32_Status_t LoraAs32_WriteConfig(
    const LoraAs32_Config_t *config,
    bool persist,
    uint32_t timeout_ms
);

LoraAs32_Status_t LoraAs32_ReadVersion(
    uint8_t *version,
    uint16_t capacity,
    uint16_t *version_length,
    uint32_t timeout_ms
);

LoraAs32_Status_t LoraAs32_Reset(uint32_t timeout_ms);

LoraAs32_Status_t LoraAs32_Send(
    const uint8_t *data,
    uint16_t length,
    uint32_t timeout_ms
);

/*
 * Đọc đúng requested_length byte. Nếu timeout sau partial RX, received_length
 * vẫn cho biết số byte đã lấy để caller log/resync nhưng status là TIMEOUT.
 */
LoraAs32_Status_t LoraAs32_Receive(
    uint8_t *data,
    uint16_t requested_length,
    uint16_t *received_length,
    uint32_t timeout_ms
);

uint16_t LoraAs32_Available(void);
void LoraAs32_FlushRx(void);
LoraAs32_Status_t LoraAs32_GetStatus(void);
void LoraAs32_ClearError(void);

#endif /* AEGIS_LORA_AS32_H */
