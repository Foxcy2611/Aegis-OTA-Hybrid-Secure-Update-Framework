#include "lora_as32.h"

#include <stddef.h>

#define LORA_AS32_COMMAND_WRITE_PERSISTENT      UINT8_C(0xC0)
#define LORA_AS32_COMMAND_WRITE_VOLATILE        UINT8_C(0xC2)
#define LORA_AS32_CONFIG_RESPONSE_PREFIX        UINT8_C(0xC0)
#define LORA_AS32_UART_BAUD_9600_BITS           UINT8_C(0x18)
#define LORA_AS32_UART_BAUD_MASK                UINT8_C(0x38)
#define LORA_AS32_UART_PARITY_MASK              UINT8_C(0xC0)
#define LORA_AS32_UART_PARITY_8N1_ALT           UINT8_C(0xC0)
#define LORA_AS32_AIR_RATE_MASK                 UINT8_C(0x07)
#define LORA_AS32_AIR_RATE_MAX                  UINT8_C(0x05)
#define LORA_AS32_CHANNEL_MAX                   UINT8_C(0x1F)
#define LORA_AS32_OPTION_RESERVED_MASK          UINT8_C(0x04)

bool LoraAs32_ConfigIsSupported(const LoraAs32_Config_t *config){
    uint8_t parity;

    if(config == NULL){
        return false;
    }

    parity = config->speed & LORA_AS32_UART_PARITY_MASK;

    /*
     * Driver đang chạy USART1 9600 8N1 nên không được tự ghi config làm mất
     * liên lạc UART. 0b11 cũng được datasheet định nghĩa tương đương 8N1.
     */
    if((config->speed & LORA_AS32_UART_BAUD_MASK) !=
       LORA_AS32_UART_BAUD_9600_BITS){
        return false;
    }

    if((parity != 0U) && (parity != LORA_AS32_UART_PARITY_8N1_ALT)){
        return false;
    }

    if((config->speed & LORA_AS32_AIR_RATE_MASK) >
       LORA_AS32_AIR_RATE_MAX){
        return false;
    }

    if(config->channel > LORA_AS32_CHANNEL_MAX){
        return false;
    }

    return (config->option & LORA_AS32_OPTION_RESERVED_MASK) == 0U;
}

LoraAs32_Status_t LoraAs32_DecodeConfigResponse(
    const uint8_t response[LORA_AS32_CONFIG_RESPONSE_SIZE],
    LoraAs32_Config_t *config
){
    if((response == NULL) || (config == NULL)){
        return LORA_AS32_INVALID_ARGUMENT;
    }

    if(response[0] != LORA_AS32_CONFIG_RESPONSE_PREFIX){
        return LORA_AS32_CONFIG_ERROR;
    }

    config->address_high = response[1];
    config->address_low = response[2];
    config->speed = response[3];
    config->channel = response[4];
    config->option = response[5];

    return LoraAs32_ConfigIsSupported(config) ?
               LORA_AS32_OK : LORA_AS32_CONFIG_ERROR;
}

LoraAs32_Status_t LoraAs32_BuildWriteCommand(
    const LoraAs32_Config_t *config,
    bool persist,
    uint8_t command[LORA_AS32_CONFIG_RESPONSE_SIZE]
){
    if((config == NULL) || (command == NULL)){
        return LORA_AS32_INVALID_ARGUMENT;
    }

    if(!LoraAs32_ConfigIsSupported(config)){
        return LORA_AS32_OUT_OF_RANGE;
    }

    command[0] = persist ?
                     LORA_AS32_COMMAND_WRITE_PERSISTENT :
                     LORA_AS32_COMMAND_WRITE_VOLATILE;
    command[1] = config->address_high;
    command[2] = config->address_low;
    command[3] = config->speed;
    command[4] = config->channel;
    command[5] = config->option;

    return LORA_AS32_OK;
}
