#include "phase4_link_packet.h"

#include <stdbool.h>
#include <string.h>

#define PHASE4_CRC32_POLYNOMIAL                  UINT32_C(0xEDB88320)

static void Phase4Link_WriteU32Le(uint8_t *output, uint32_t value){
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
    output[2] = (uint8_t)(value >> 16U);
    output[3] = (uint8_t)(value >> 24U);
}

static uint32_t Phase4Link_ReadU32Le(const uint8_t *input){
    return ((uint32_t)input[0]) |
           ((uint32_t)input[1] << 8U) |
           ((uint32_t)input[2] << 16U) |
           ((uint32_t)input[3] << 24U);
}

static uint32_t Phase4Link_Crc32(
    const uint8_t *data,
    size_t length
){
    uint32_t crc = UINT32_MAX;

    for(size_t index = 0U; index < length; ++index){
        crc ^= data[index];
        for(uint32_t bit = 0U; bit < 8U; ++bit){
            const uint32_t mask = 0U - (crc & 1U);

            crc = (crc >> 1U) ^ (PHASE4_CRC32_POLYNOMIAL & mask);
        }
    }
    return ~crc;
}

static bool Phase4Link_DirectionIsValid(
    Phase4Link_Direction_t direction
){
    return (direction == PHASE4_LINK_GATEWAY_TO_NODE) ||
           (direction == PHASE4_LINK_NODE_TO_GATEWAY);
}

Phase4Link_Status_t Phase4Link_Encode(
    Phase4Link_Direction_t direction,
    uint32_t counter,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t *raw,
    size_t raw_capacity,
    size_t *raw_length
){
    size_t required_size;
    uint32_t crc;

    if((raw == NULL) || (raw_length == NULL) ||
       ((payload_length > 0U) && (payload == NULL))){
        return PHASE4_LINK_INVALID_ARGUMENT;
    }
    if(!Phase4Link_DirectionIsValid(direction)){
        return PHASE4_LINK_BAD_DIRECTION;
    }
    if(payload_length > PHASE4_LINK_MAX_PAYLOAD_SIZE){
        return PHASE4_LINK_OUT_OF_RANGE;
    }

    required_size =
        PHASE4_LINK_HEADER_SIZE +
        payload_length +
        PHASE4_LINK_CRC_SIZE;
    if(raw_capacity < required_size){
        return PHASE4_LINK_OUT_OF_RANGE;
    }

    raw[0] = PHASE4_LINK_MAGIC_0;
    raw[1] = PHASE4_LINK_MAGIC_1;
    raw[PHASE4_LINK_DIRECTION_OFFSET] = (uint8_t)direction;
    Phase4Link_WriteU32Le(&raw[PHASE4_LINK_COUNTER_OFFSET], counter);
    raw[PHASE4_LINK_LENGTH_OFFSET] = (uint8_t)payload_length;
    raw[PHASE4_LINK_LENGTH_OFFSET + 1U] =
        (uint8_t)(payload_length >> 8U);
    if(payload_length > 0U){
        memcpy(
            &raw[PHASE4_LINK_PAYLOAD_OFFSET],
            payload,
            payload_length
        );
    }

    crc = Phase4Link_Crc32(
        &raw[PHASE4_LINK_DIRECTION_OFFSET],
        (PHASE4_LINK_HEADER_SIZE - PHASE4_LINK_DIRECTION_OFFSET) +
            payload_length
    );
    Phase4Link_WriteU32Le(
        &raw[PHASE4_LINK_PAYLOAD_OFFSET + payload_length],
        crc
    );
    *raw_length = required_size;
    return PHASE4_LINK_OK;
}

Phase4Link_Status_t Phase4Link_Decode(
    const uint8_t *raw,
    size_t raw_length,
    Phase4Link_Packet_t *packet
){
    uint16_t payload_length;
    size_t expected_length;
    uint32_t expected_crc;
    uint32_t actual_crc;

    if((raw == NULL) || (packet == NULL)){
        return PHASE4_LINK_INVALID_ARGUMENT;
    }
    if(raw_length < (PHASE4_LINK_HEADER_SIZE + PHASE4_LINK_CRC_SIZE)){
        return PHASE4_LINK_BAD_LENGTH;
    }
    if((raw[0] != PHASE4_LINK_MAGIC_0) ||
       (raw[1] != PHASE4_LINK_MAGIC_1)){
        return PHASE4_LINK_BAD_MAGIC;
    }

    packet->direction =
        (Phase4Link_Direction_t)raw[PHASE4_LINK_DIRECTION_OFFSET];
    if(!Phase4Link_DirectionIsValid(packet->direction)){
        return PHASE4_LINK_BAD_DIRECTION;
    }

    payload_length = (uint16_t)raw[PHASE4_LINK_LENGTH_OFFSET] |
        (uint16_t)(
            (uint16_t)raw[PHASE4_LINK_LENGTH_OFFSET + 1U] << 8U
        );
    if(payload_length > PHASE4_LINK_MAX_PAYLOAD_SIZE){
        return PHASE4_LINK_OUT_OF_RANGE;
    }
    expected_length =
        PHASE4_LINK_HEADER_SIZE +
        payload_length +
        PHASE4_LINK_CRC_SIZE;
    if(raw_length != expected_length){
        return PHASE4_LINK_BAD_LENGTH;
    }

    expected_crc = Phase4Link_ReadU32Le(
        &raw[PHASE4_LINK_PAYLOAD_OFFSET + payload_length]
    );
    actual_crc = Phase4Link_Crc32(
        &raw[PHASE4_LINK_DIRECTION_OFFSET],
        (PHASE4_LINK_HEADER_SIZE - PHASE4_LINK_DIRECTION_OFFSET) +
            payload_length
    );
    if(actual_crc != expected_crc){
        return PHASE4_LINK_BAD_CRC;
    }

    packet->counter = Phase4Link_ReadU32Le(
        &raw[PHASE4_LINK_COUNTER_OFFSET]
    );
    packet->payload_length = payload_length;
    if(payload_length > 0U){
        memcpy(
            packet->payload,
            &raw[PHASE4_LINK_PAYLOAD_OFFSET],
            payload_length
        );
    }
    return PHASE4_LINK_OK;
}
