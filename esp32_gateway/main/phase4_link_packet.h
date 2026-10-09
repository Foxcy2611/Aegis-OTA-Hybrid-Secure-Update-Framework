#ifndef AEGIS_ESP32_PHASE4_LINK_PACKET_H
#define AEGIS_ESP32_PHASE4_LINK_PACKET_H

/*
 * Packet tạm dùng riêng cho đo link P2P thô Phase 4.
 * Phase 5 phải thay bằng secure/reliable transport production.
 */

#include <stddef.h>
#include <stdint.h>

#define PHASE4_LINK_MAGIC_0                      UINT8_C(0xA4)
#define PHASE4_LINK_MAGIC_1                      UINT8_C(0x4C)
#define PHASE4_LINK_HEADER_SIZE                  9U
#define PHASE4_LINK_CRC_SIZE                     4U
#define PHASE4_LINK_MAX_PAYLOAD_SIZE             256U
#define PHASE4_LINK_MAX_RAW_SIZE                                      \
    (PHASE4_LINK_HEADER_SIZE + PHASE4_LINK_MAX_PAYLOAD_SIZE +         \
     PHASE4_LINK_CRC_SIZE)

#define PHASE4_LINK_DIRECTION_OFFSET             2U
#define PHASE4_LINK_COUNTER_OFFSET               3U
#define PHASE4_LINK_LENGTH_OFFSET                7U
#define PHASE4_LINK_PAYLOAD_OFFSET               9U

typedef enum{
    PHASE4_LINK_GATEWAY_TO_NODE = 1U,
    PHASE4_LINK_NODE_TO_GATEWAY = 2U
} Phase4Link_Direction_t;

typedef enum{
    PHASE4_LINK_OK = 0U,
    PHASE4_LINK_INVALID_ARGUMENT,
    PHASE4_LINK_OUT_OF_RANGE,
    PHASE4_LINK_BAD_MAGIC,
    PHASE4_LINK_BAD_DIRECTION,
    PHASE4_LINK_BAD_LENGTH,
    PHASE4_LINK_BAD_CRC
} Phase4Link_Status_t;

typedef struct{
    Phase4Link_Direction_t direction;
    uint32_t counter;
    uint16_t payload_length;
    uint8_t payload[PHASE4_LINK_MAX_PAYLOAD_SIZE];
} Phase4Link_Packet_t;

Phase4Link_Status_t Phase4Link_Encode(
    Phase4Link_Direction_t direction,
    uint32_t counter,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t *raw,
    size_t raw_capacity,
    size_t *raw_length
);

Phase4Link_Status_t Phase4Link_Decode(
    const uint8_t *raw,
    size_t raw_length,
    Phase4Link_Packet_t *packet
);

#endif /* AEGIS_ESP32_PHASE4_LINK_PACKET_H */
