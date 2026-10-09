#include "lora_as32.h"
#include "memory_map.h"
#include "phase4_packet.h"
#include "pinout_hw.h"
#include "timebase.h"

#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

#include <stdbool.h>
#include <stdint.h>

#define PHASE4_BOARD_IO_TIMEOUT_MS              3000U

volatile uint32_t phase4_valid_rx_count = 0U;
volatile uint32_t phase4_invalid_rx_count = 0U;
volatile uint32_t phase4_tx_count = 0U;
volatile uint32_t phase4_timeout_count = 0U;
volatile LoraAs32_Status_t phase4_last_lora_status = LORA_AS32_NOT_READY;
volatile Phase4_PacketStatus_t phase4_last_packet_status =
    PHASE4_PACKET_INVALID_ARGUMENT;
volatile LoraAs32_Config_t phase4_read_config;
volatile uint8_t phase4_version[LORA_AS32_VERSION_MAX_SIZE];
volatile uint16_t phase4_version_length = 0U;

static void Phase4Board_LedInit(void){
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = STATUS_LED_PIN;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(STATUS_LED_PORT, &gpio);
    GPIO_SetBits(STATUS_LED_PORT, STATUS_LED_PIN);
}

static void Phase4Board_ToggleLed(void){
    STATUS_LED_PORT->ODR ^= STATUS_LED_PIN;
}

static void Phase4Board_Fatal(LoraAs32_Status_t status){
    phase4_last_lora_status = status;

    for(;;){
        Phase4Board_ToggleLed();
        for(volatile uint32_t delay = 0U; delay < 800000U; ++delay){
            __NOP();
        }
    }
}

static LoraAs32_Status_t Phase4Board_ReceiveOne(uint8_t *byte){
    uint16_t received_length = 0U;

    return LoraAs32_Receive(
        byte,
        1U,
        &received_length,
        PHASE4_BOARD_IO_TIMEOUT_MS
    );
}

static LoraAs32_Status_t Phase4Board_ReceiveRawPacket(
    uint8_t raw[PHASE4_PACKET_MAX_RAW_SIZE],
    uint16_t *raw_length
){
    bool first_magic_seen = false;
    uint8_t byte;
    uint16_t received_length;
    uint16_t payload_length;
    uint16_t remaining_length;
    LoraAs32_Status_t status;

    for(;;){
        status = Phase4Board_ReceiveOne(&byte);
        if(status != LORA_AS32_OK){
            return status;
        }

        if(!first_magic_seen){
            first_magic_seen = (byte == PHASE4_PACKET_MAGIC_0);
            continue;
        }

        if(byte == PHASE4_PACKET_MAGIC_1){
            raw[0] = PHASE4_PACKET_MAGIC_0;
            raw[1] = PHASE4_PACKET_MAGIC_1;
            break;
        }

        first_magic_seen = (byte == PHASE4_PACKET_MAGIC_0);
    }

    status = LoraAs32_Receive(
        &raw[2],
        (uint16_t)(PHASE4_PACKET_HEADER_SIZE - 2U),
        &received_length,
        PHASE4_BOARD_IO_TIMEOUT_MS
    );
    if(status != LORA_AS32_OK){
        return status;
    }

    payload_length = (uint16_t)raw[PHASE4_PACKET_LENGTH_OFFSET] |
                     (uint16_t)(
                         (uint16_t)raw[PHASE4_PACKET_LENGTH_OFFSET + 1U] <<
                         8U
                     );
    if(payload_length > PHASE4_PACKET_MAX_PAYLOAD_SIZE){
        return LORA_AS32_OUT_OF_RANGE;
    }

    remaining_length = (uint16_t)(
        payload_length + PHASE4_PACKET_CRC_SIZE
    );
    status = LoraAs32_Receive(
        &raw[PHASE4_PACKET_HEADER_SIZE],
        remaining_length,
        &received_length,
        PHASE4_BOARD_IO_TIMEOUT_MS
    );
    if(status != LORA_AS32_OK){
        return status;
    }

    *raw_length = (uint16_t)(
        PHASE4_PACKET_HEADER_SIZE + remaining_length
    );
    return LORA_AS32_OK;
}

int main(void){
    uint8_t raw_rx[PHASE4_PACKET_MAX_RAW_SIZE];
    uint8_t raw_tx[PHASE4_PACKET_MAX_RAW_SIZE];
    uint8_t version[LORA_AS32_VERSION_MAX_SIZE];
    LoraAs32_Config_t config;
    Phase4_Packet_t packet;
    uint16_t raw_rx_length = 0U;
    uint16_t raw_tx_length;
    uint16_t version_length;

    SCB->VTOR = BOOTLOADER_BASE_ADDRESS;
    Phase4Board_LedInit();

    phase4_last_lora_status = LoraAs32_Init();
    if(phase4_last_lora_status != LORA_AS32_OK){
        Phase4Board_Fatal(phase4_last_lora_status);
    }

    phase4_last_lora_status = LoraAs32_EnterConfig(
        PHASE4_BOARD_IO_TIMEOUT_MS
    );
    if(phase4_last_lora_status != LORA_AS32_OK){
        Phase4Board_Fatal(phase4_last_lora_status);
    }

    phase4_last_lora_status = LoraAs32_ReadConfig(
        &config,
        PHASE4_BOARD_IO_TIMEOUT_MS
    );
    if(phase4_last_lora_status != LORA_AS32_OK){
        Phase4Board_Fatal(phase4_last_lora_status);
    }
    phase4_read_config = config;

    phase4_last_lora_status = LoraAs32_ReadVersion(
        version,
        sizeof(version),
        &version_length,
        PHASE4_BOARD_IO_TIMEOUT_MS
    );
    if(phase4_last_lora_status != LORA_AS32_OK){
        Phase4Board_Fatal(phase4_last_lora_status);
    }
    phase4_version_length = version_length;
    for(uint16_t index = 0U; index < version_length; ++index){
        phase4_version[index] = version[index];
    }

    phase4_last_lora_status = LoraAs32_EnterNormal(
        PHASE4_BOARD_IO_TIMEOUT_MS
    );
    if(phase4_last_lora_status != LORA_AS32_OK){
        Phase4Board_Fatal(phase4_last_lora_status);
    }

    for(;;){
        phase4_last_lora_status = Phase4Board_ReceiveRawPacket(
            raw_rx,
            &raw_rx_length
        );
        if(phase4_last_lora_status == LORA_AS32_TIMEOUT){
            ++phase4_timeout_count;
            continue;
        }
        if(phase4_last_lora_status != LORA_AS32_OK){
            ++phase4_invalid_rx_count;
            LoraAs32_ClearError();
            LoraAs32_FlushRx();
            continue;
        }

        phase4_last_packet_status = Phase4Packet_Decode(
            raw_rx,
            raw_rx_length,
            &packet
        );
        if((phase4_last_packet_status != PHASE4_PACKET_OK) ||
           (packet.direction != PHASE4_DIRECTION_GATEWAY_TO_NODE)){
            ++phase4_invalid_rx_count;
            continue;
        }

        ++phase4_valid_rx_count;
        phase4_last_packet_status = Phase4Packet_Encode(
            PHASE4_DIRECTION_NODE_TO_GATEWAY,
            packet.counter,
            packet.payload,
            packet.payload_length,
            raw_tx,
            sizeof(raw_tx),
            &raw_tx_length
        );
        if(phase4_last_packet_status != PHASE4_PACKET_OK){
            ++phase4_invalid_rx_count;
            continue;
        }

        phase4_last_lora_status = LoraAs32_Send(
            raw_tx,
            raw_tx_length,
            PHASE4_BOARD_IO_TIMEOUT_MS
        );
        if(phase4_last_lora_status == LORA_AS32_OK){
            ++phase4_tx_count;
            Phase4Board_ToggleLed();
        }
    }
}
