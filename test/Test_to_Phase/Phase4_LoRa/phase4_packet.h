#ifndef AEGIS_PHASE4_PACKET_H
#define AEGIS_PHASE4_PACKET_H

/*
 * Packet tạm chỉ dùng để bring-up/đo link thô Phase 4.
 * Không được tái sử dụng làm OTA protocol production ở Phase 5.
 */

#include <stdint.h>

#define PHASE4_PACKET_MAGIC_0                   UINT8_C(0xA4)
#define PHASE4_PACKET_MAGIC_1                   UINT8_C(0x4C)
#define PHASE4_PACKET_HEADER_SIZE               9U
#define PHASE4_PACKET_CRC_SIZE                  4U
#define PHASE4_PACKET_MAX_PAYLOAD_SIZE          256U
#define PHASE4_PACKET_MAX_RAW_SIZE                                      \
    (PHASE4_PACKET_HEADER_SIZE + PHASE4_PACKET_MAX_PAYLOAD_SIZE +       \
     PHASE4_PACKET_CRC_SIZE)

#define PHASE4_PACKET_DIRECTION_OFFSET          2U
#define PHASE4_PACKET_COUNTER_OFFSET            3U
#define PHASE4_PACKET_LENGTH_OFFSET             7U
#define PHASE4_PACKET_PAYLOAD_OFFSET            9U

typedef enum{
    PHASE4_DIRECTION_GATEWAY_TO_NODE = 1U,
    PHASE4_DIRECTION_NODE_TO_GATEWAY = 2U
} Phase4_Direction_t;

typedef enum{
    PHASE4_PACKET_OK = 0U,
    PHASE4_PACKET_INVALID_ARGUMENT,
    PHASE4_PACKET_OUT_OF_RANGE,
    PHASE4_PACKET_BAD_MAGIC,
    PHASE4_PACKET_BAD_DIRECTION,
    PHASE4_PACKET_BAD_LENGTH,
    PHASE4_PACKET_BAD_CRC
} Phase4_PacketStatus_t;

typedef struct{
    Phase4_Direction_t direction;
    uint32_t counter;
    uint16_t payload_length;
    uint8_t payload[PHASE4_PACKET_MAX_PAYLOAD_SIZE];
} Phase4_Packet_t;

Phase4_PacketStatus_t Phase4Packet_Encode(
    Phase4_Direction_t direction,
    uint32_t counter,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t *raw,
    uint16_t raw_capacity,
    uint16_t *raw_length
);

Phase4_PacketStatus_t Phase4Packet_Decode(
    const uint8_t *raw,
    uint16_t raw_length,
    Phase4_Packet_t *packet
);

#endif /* AEGIS_PHASE4_PACKET_H */
