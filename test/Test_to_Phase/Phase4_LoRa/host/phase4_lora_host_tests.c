#include "lora_as32.h"
#include "phase4_packet.h"
#include "phase4_link_packet.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t check_count = 0U;
static uint32_t failure_count = 0U;

static void Check(bool condition, const char *name){
    ++check_count;

    if(!condition){
        ++failure_count;
        (void)printf("FAIL: %s\n", name);
    }
}

static void TestAs32ConfigContract(void){
    static const uint8_t factory_response[
        LORA_AS32_CONFIG_RESPONSE_SIZE
    ] = {
        0xC0U, 0x00U, 0x00U, 0x1AU, 0x17U, 0x40U
    };
    uint8_t response[LORA_AS32_CONFIG_RESPONSE_SIZE];
    uint8_t command[LORA_AS32_CONFIG_RESPONSE_SIZE];
    LoraAs32_Config_t config;
    LoraAs32_Config_t invalid;

    memcpy(response, factory_response, sizeof(response));
    Check(
        LoraAs32_DecodeConfigResponse(response, &config) ==
            LORA_AS32_OK,
        "decode config hợp lệ"
    );
    Check(
        (config.speed == 0x1AU) &&
        (config.channel == 0x17U) &&
        (config.option == 0x40U),
        "decode đúng năm config byte"
    );
    Check(
        LoraAs32_ConfigIsSupported(&config),
        "profile 9600 8N1 được hỗ trợ"
    );
    Check(
        !LoraAs32_ConfigIsSupported(NULL),
        "config NULL bị từ chối"
    );

    response[0] = 0xC1U;
    Check(
        LoraAs32_DecodeConfigResponse(response, &config) ==
            LORA_AS32_CONFIG_ERROR,
        "sai prefix response bị từ chối"
    );

    invalid = config;
    invalid.speed = 0x12U;
    Check(
        !LoraAs32_ConfigIsSupported(&invalid),
        "baud khác 9600 bị từ chối"
    );

    invalid = config;
    invalid.speed = 0x5AU;
    Check(
        !LoraAs32_ConfigIsSupported(&invalid),
        "parity khác 8N1 bị từ chối"
    );

    invalid = config;
    invalid.speed = 0x1EU;
    Check(
        !LoraAs32_ConfigIsSupported(&invalid),
        "air rate reserved bị từ chối"
    );

    invalid = config;
    invalid.channel = 0x20U;
    Check(
        !LoraAs32_ConfigIsSupported(&invalid),
        "channel ngoài dải bị từ chối"
    );

    invalid = config;
    invalid.option = 0x44U;
    Check(
        !LoraAs32_ConfigIsSupported(&invalid),
        "option reserved bit bị từ chối"
    );

    Check(
        LoraAs32_BuildWriteCommand(&config, true, command) ==
            LORA_AS32_OK,
        "tạo persistent config command"
    );
    Check(
        (command[0] == 0xC0U) &&
        (memcmp(&command[1], &factory_response[1], 5U) == 0),
        "persistent command dùng C0 và giữ config bytes"
    );

    Check(
        LoraAs32_BuildWriteCommand(&config, false, command) ==
            LORA_AS32_OK,
        "tạo volatile config command"
    );
    Check(
        command[0] == 0xC2U,
        "volatile command dùng C2"
    );
    Check(
        LoraAs32_BuildWriteCommand(NULL, false, command) ==
            LORA_AS32_INVALID_ARGUMENT,
        "write command NULL bị từ chối"
    );
}

static void TestPacketLength(uint16_t payload_length){
    uint8_t payload[PHASE4_PACKET_MAX_PAYLOAD_SIZE];
    uint8_t raw[PHASE4_PACKET_MAX_RAW_SIZE];
    uint16_t raw_length = 0U;
    Phase4_Packet_t decoded;

    for(uint16_t index = 0U; index < payload_length; ++index){
        payload[index] = (uint8_t)(index ^ 0xA5U);
    }

    Check(
        Phase4Packet_Encode(
            PHASE4_DIRECTION_GATEWAY_TO_NODE,
            UINT32_C(0x78563412),
            payload,
            payload_length,
            raw,
            sizeof(raw),
            &raw_length
        ) == PHASE4_PACKET_OK,
        "encode packet boundary"
    );
    Check(
        raw_length ==
            (uint16_t)(
                PHASE4_PACKET_HEADER_SIZE +
                payload_length +
                PHASE4_PACKET_CRC_SIZE
            ),
        "raw length đúng"
    );
    Check(
        (raw[PHASE4_PACKET_COUNTER_OFFSET] == 0x12U) &&
        (raw[PHASE4_PACKET_COUNTER_OFFSET + 1U] == 0x34U) &&
        (raw[PHASE4_PACKET_COUNTER_OFFSET + 2U] == 0x56U) &&
        (raw[PHASE4_PACKET_COUNTER_OFFSET + 3U] == 0x78U),
        "counter dùng little-endian"
    );
    Check(
        Phase4Packet_Decode(raw, raw_length, &decoded) ==
            PHASE4_PACKET_OK,
        "decode packet boundary"
    );
    Check(
        (decoded.direction == PHASE4_DIRECTION_GATEWAY_TO_NODE) &&
        (decoded.counter == UINT32_C(0x78563412)) &&
        (decoded.payload_length == payload_length) &&
        (memcmp(decoded.payload, payload, payload_length) == 0),
        "packet round-trip giữ nguyên dữ liệu"
    );
}

static void TestPacketNegativeCases(void){
    uint8_t payload[PHASE4_PACKET_MAX_PAYLOAD_SIZE + 1U] = {0U};
    uint8_t raw[PHASE4_PACKET_MAX_RAW_SIZE + 1U];
    uint16_t raw_length = 0U;
    Phase4_Packet_t decoded;

    Check(
        Phase4Packet_Encode(
            PHASE4_DIRECTION_GATEWAY_TO_NODE,
            1U,
            NULL,
            1U,
            raw,
            sizeof(raw),
            &raw_length
        ) == PHASE4_PACKET_INVALID_ARGUMENT,
        "payload NULL với length khác 0 bị từ chối"
    );
    Check(
        Phase4Packet_Encode(
            (Phase4_Direction_t)0U,
            1U,
            payload,
            1U,
            raw,
            sizeof(raw),
            &raw_length
        ) == PHASE4_PACKET_BAD_DIRECTION,
        "direction lạ bị từ chối khi encode"
    );
    Check(
        Phase4Packet_Encode(
            PHASE4_DIRECTION_GATEWAY_TO_NODE,
            1U,
            payload,
            PHASE4_PACKET_MAX_PAYLOAD_SIZE + 1U,
            raw,
            sizeof(raw),
            &raw_length
        ) == PHASE4_PACKET_OUT_OF_RANGE,
        "MTU cộng một bị từ chối"
    );
    Check(
        Phase4Packet_Encode(
            PHASE4_DIRECTION_GATEWAY_TO_NODE,
            1U,
            payload,
            1U,
            raw,
            PHASE4_PACKET_HEADER_SIZE,
            &raw_length
        ) == PHASE4_PACKET_OUT_OF_RANGE,
        "output capacity thiếu bị từ chối"
    );

    Check(
        Phase4Packet_Encode(
            PHASE4_DIRECTION_GATEWAY_TO_NODE,
            1U,
            payload,
            1U,
            raw,
            sizeof(raw),
            &raw_length
        ) == PHASE4_PACKET_OK,
        "tạo packet gốc cho negative test"
    );
    Check(
        Phase4Packet_Decode(
            raw,
            PHASE4_PACKET_HEADER_SIZE,
            &decoded
        ) == PHASE4_PACKET_BAD_LENGTH,
        "packet quá ngắn bị từ chối"
    );

    raw[0] ^= 1U;
    Check(
        Phase4Packet_Decode(raw, raw_length, &decoded) ==
            PHASE4_PACKET_BAD_MAGIC,
        "magic hỏng bị từ chối"
    );
    raw[0] ^= 1U;

    raw[PHASE4_PACKET_DIRECTION_OFFSET] = 3U;
    Check(
        Phase4Packet_Decode(raw, raw_length, &decoded) ==
            PHASE4_PACKET_BAD_DIRECTION,
        "direction hỏng bị từ chối khi decode"
    );
    raw[PHASE4_PACKET_DIRECTION_OFFSET] =
        PHASE4_DIRECTION_GATEWAY_TO_NODE;

    Check(
        Phase4Packet_Decode(raw, (uint16_t)(raw_length - 1U), &decoded) ==
            PHASE4_PACKET_BAD_LENGTH,
        "packet bị cắt ngắn bị từ chối"
    );
    Check(
        Phase4Packet_Decode(raw, (uint16_t)(raw_length + 1U), &decoded) ==
            PHASE4_PACKET_BAD_LENGTH,
        "packet thừa byte bị từ chối"
    );

    raw[PHASE4_PACKET_PAYLOAD_OFFSET] ^= 1U;
    Check(
        Phase4Packet_Decode(raw, raw_length, &decoded) ==
            PHASE4_PACKET_BAD_CRC,
        "payload corrupt bị CRC bắt"
    );
}

static void TestCrossImplementationCompatibility(void){
    uint8_t payload[64];
    uint8_t raw[PHASE4_PACKET_MAX_RAW_SIZE];
    uint16_t stm32_raw_length = 0U;
    size_t esp32_raw_length = 0U;
    Phase4_Packet_t stm32_packet;
    Phase4Link_Packet_t esp32_packet;

    for(size_t index = 0U; index < sizeof(payload); ++index){
        payload[index] = (uint8_t)(index * 3U);
    }

    Check(
        Phase4Packet_Encode(
            PHASE4_DIRECTION_GATEWAY_TO_NODE,
            UINT32_C(0x10203040),
            payload,
            sizeof(payload),
            raw,
            sizeof(raw),
            &stm32_raw_length
        ) == PHASE4_PACKET_OK,
        "STM32 codec tạo packet cross-test"
    );
    Check(
        Phase4Link_Decode(
            raw,
            stm32_raw_length,
            &esp32_packet
        ) == PHASE4_LINK_OK,
        "ESP32 decode được packet STM32"
    );
    Check(
        (esp32_packet.counter == UINT32_C(0x10203040)) &&
        (esp32_packet.payload_length == sizeof(payload)) &&
        (memcmp(
             esp32_packet.payload,
             payload,
             sizeof(payload)
         ) == 0),
        "ESP32 giữ nguyên field/payload STM32"
    );

    Check(
        Phase4Link_Encode(
            PHASE4_LINK_NODE_TO_GATEWAY,
            UINT32_C(0x50607080),
            payload,
            sizeof(payload),
            raw,
            sizeof(raw),
            &esp32_raw_length
        ) == PHASE4_LINK_OK,
        "ESP32 codec tạo packet cross-test"
    );
    Check(
        Phase4Packet_Decode(
            raw,
            (uint16_t)esp32_raw_length,
            &stm32_packet
        ) == PHASE4_PACKET_OK,
        "STM32 decode được packet ESP32"
    );
    Check(
        (stm32_packet.counter == UINT32_C(0x50607080)) &&
        (stm32_packet.payload_length == sizeof(payload)) &&
        (memcmp(
             stm32_packet.payload,
             payload,
             sizeof(payload)
         ) == 0),
        "STM32 giữ nguyên field/payload ESP32"
    );
}

int main(void){
    TestAs32ConfigContract();
    TestPacketLength(0U);
    TestPacketLength(1U);
    TestPacketLength(PHASE4_PACKET_MAX_PAYLOAD_SIZE - 1U);
    TestPacketLength(PHASE4_PACKET_MAX_PAYLOAD_SIZE);
    TestPacketNegativeCases();
    TestCrossImplementationCompatibility();

    (void)printf(
        "Phase 4 host: %lu checks, %lu failures\n",
        (unsigned long)check_count,
        (unsigned long)failure_count
    );

    return failure_count == 0U ? 0 : 1;
}
