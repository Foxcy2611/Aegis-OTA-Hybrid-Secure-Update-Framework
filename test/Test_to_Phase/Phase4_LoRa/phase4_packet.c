#include "phase4_packet.h"

#include "crc32.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static void Phase4Packet_WriteU32Le(uint8_t *output, uint32_t value){
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
    output[2] = (uint8_t)(value >> 16U);
    output[3] = (uint8_t)(value >> 24U);
}

static uint32_t Phase4Packet_ReadU32Le(const uint8_t *input){
    return ((uint32_t)input[0]) |
           ((uint32_t)input[1] << 8U) |
           ((uint32_t)input[2] << 16U) |
           ((uint32_t)input[3] << 24U);
}

static bool Phase4Packet_DirectionIsValid(Phase4_Direction_t direction){
    return (direction == PHASE4_DIRECTION_GATEWAY_TO_NODE) ||
           (direction == PHASE4_DIRECTION_NODE_TO_GATEWAY);
}

Phase4_PacketStatus_t Phase4Packet_Encode(
    Phase4_Direction_t direction,
    uint32_t counter,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t *raw,
    uint16_t raw_capacity,
    uint16_t *raw_length
){
    uint16_t required_size;
    uint32_t crc;

    if((raw == NULL) || (raw_length == NULL) ||
       ((payload_length > 0U) && (payload == NULL))){
        return PHASE4_PACKET_INVALID_ARGUMENT;
    }

    if(!Phase4Packet_DirectionIsValid(direction)){
        return PHASE4_PACKET_BAD_DIRECTION;
    }

    if(payload_length > PHASE4_PACKET_MAX_PAYLOAD_SIZE){
        return PHASE4_PACKET_OUT_OF_RANGE;
    }

    required_size = (uint16_t)(
        PHASE4_PACKET_HEADER_SIZE +
        payload_length +
        PHASE4_PACKET_CRC_SIZE
    );
    if(raw_capacity < required_size){
        return PHASE4_PACKET_OUT_OF_RANGE;
    }

    raw[0] = PHASE4_PACKET_MAGIC_0;
    raw[1] = PHASE4_PACKET_MAGIC_1;
    raw[PHASE4_PACKET_DIRECTION_OFFSET] = (uint8_t)direction;
    Phase4Packet_WriteU32Le(
        &raw[PHASE4_PACKET_COUNTER_OFFSET],
        counter
    );
    raw[PHASE4_PACKET_LENGTH_OFFSET] = (uint8_t)payload_length;
    raw[PHASE4_PACKET_LENGTH_OFFSET + 1U] =
        (uint8_t)(payload_length >> 8U);

    if(payload_length > 0U){
        memcpy(
            &raw[PHASE4_PACKET_PAYLOAD_OFFSET],
            payload,
            payload_length
        );
    }

    crc = Crc32_Compute(
        &raw[PHASE4_PACKET_DIRECTION_OFFSET],
        (size_t)(PHASE4_PACKET_HEADER_SIZE - PHASE4_PACKET_DIRECTION_OFFSET) +
            payload_length
    );
    Phase4Packet_WriteU32Le(
        &raw[PHASE4_PACKET_PAYLOAD_OFFSET + payload_length],
        crc
    );
    *raw_length = required_size;
    return PHASE4_PACKET_OK;
}

Phase4_PacketStatus_t Phase4Packet_Decode(
    const uint8_t *raw,
    uint16_t raw_length,
    Phase4_Packet_t *packet
){
    uint16_t payload_length;
    uint16_t expected_size;
    uint32_t expected_crc;
    uint32_t actual_crc;

    if((raw == NULL) || (packet == NULL)){
        return PHASE4_PACKET_INVALID_ARGUMENT;
    }

    if(raw_length < (PHASE4_PACKET_HEADER_SIZE + PHASE4_PACKET_CRC_SIZE)){
        return PHASE4_PACKET_BAD_LENGTH;
    }

    if((raw[0] != PHASE4_PACKET_MAGIC_0) ||
       (raw[1] != PHASE4_PACKET_MAGIC_1)){
        return PHASE4_PACKET_BAD_MAGIC;
    }

    packet->direction =
        (Phase4_Direction_t)raw[PHASE4_PACKET_DIRECTION_OFFSET];
    if(!Phase4Packet_DirectionIsValid(packet->direction)){
        return PHASE4_PACKET_BAD_DIRECTION;
    }

    payload_length = (uint16_t)raw[PHASE4_PACKET_LENGTH_OFFSET] |
                     (uint16_t)(
                         (uint16_t)raw[PHASE4_PACKET_LENGTH_OFFSET + 1U] <<
                         8U
                     );
    if(payload_length > PHASE4_PACKET_MAX_PAYLOAD_SIZE){
        return PHASE4_PACKET_OUT_OF_RANGE;
    }

    expected_size = (uint16_t)(
        PHASE4_PACKET_HEADER_SIZE +
        payload_length +
        PHASE4_PACKET_CRC_SIZE
    );
    if(raw_length != expected_size){
        return PHASE4_PACKET_BAD_LENGTH;
    }

    expected_crc = Phase4Packet_ReadU32Le(
        &raw[PHASE4_PACKET_PAYLOAD_OFFSET + payload_length]
    );
    actual_crc = Crc32_Compute(
        &raw[PHASE4_PACKET_DIRECTION_OFFSET],
        (size_t)(PHASE4_PACKET_HEADER_SIZE - PHASE4_PACKET_DIRECTION_OFFSET) +
            payload_length
    );
    if(actual_crc != expected_crc){
        return PHASE4_PACKET_BAD_CRC;
    }

    packet->counter = Phase4Packet_ReadU32Le(
        &raw[PHASE4_PACKET_COUNTER_OFFSET]
    );
    packet->payload_length = payload_length;
    if(payload_length > 0U){
        memcpy(
            packet->payload,
            &raw[PHASE4_PACKET_PAYLOAD_OFFSET],
            payload_length
        );
    }

    return PHASE4_PACKET_OK;
}
