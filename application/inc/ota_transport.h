#ifndef AEGIS_OTA_TRANSPORT_H
#define AEGIS_OTA_TRANSPORT_H

/* Serialize/parse logical OTA payload; không biết AS32, crypto hay W25Q32. */

#include "ota_protocol.h"

#include <stddef.h>
#include <stdint.h>

typedef enum{
    OTA_TRANSPORT_OK = 0U,
    OTA_TRANSPORT_INVALID_ARGUMENT,
    OTA_TRANSPORT_BAD_TYPE,
    OTA_TRANSPORT_BAD_LENGTH,
    OTA_TRANSPORT_OUT_OF_RANGE,
    OTA_TRANSPORT_BAD_VALUE
} OtaTransport_Status_t;

typedef struct{
    uint64_t transfer_id;
    uint8_t artifact_id[OTA_ARTIFACT_ID_SIZE];
    uint64_t operation_id;
    OtaProtocol_ArtifactType_t artifact_type;
    uint16_t manifest_size;
    uint32_t data_size;
    uint16_t chunk_size;
} OtaTransport_Begin_t;

typedef struct{
    uint64_t transfer_id;
    OtaProtocol_Section_t section;
    uint32_t offset;
    uint16_t data_length;
    const uint8_t *data;
} OtaTransport_DataView_t;

typedef struct{
    uint8_t request_type;
    uint8_t status_or_reason;
    uint64_t transfer_id;
    uint32_t next_missing_offset;
} OtaTransport_Ack_t;

typedef struct{
    OtaProtocol_ReceiveState_t receive_state;
    OtaProtocol_ArtifactType_t artifact_type;
    uint64_t transfer_id;
    uint8_t artifact_id[OTA_ARTIFACT_ID_SIZE];
    uint32_t data_size;
    uint16_t chunk_size;
    uint32_t next_missing_offset;
    uint32_t flags;
} OtaTransport_StatusResponse_t;

OtaTransport_Status_t OtaTransport_ParseBegin(
    const uint8_t *payload,
    size_t payload_length,
    OtaTransport_Begin_t *begin
);

OtaTransport_Status_t OtaTransport_ParseData(
    const uint8_t *payload,
    size_t payload_length,
    OtaTransport_DataView_t *data
);

OtaTransport_Status_t OtaTransport_ParseId(
    const uint8_t *payload,
    size_t payload_length,
    uint64_t *id
);

OtaTransport_Status_t OtaTransport_BuildBegin(
    const OtaTransport_Begin_t *begin,
    uint8_t payload[OTA_BEGIN_FIXED_PAYLOAD_SIZE]
);

OtaTransport_Status_t OtaTransport_BuildData(
    uint64_t transfer_id,
    OtaProtocol_Section_t section,
    uint32_t offset,
    const uint8_t *data,
    uint16_t data_length,
    uint8_t *payload,
    size_t payload_capacity,
    uint16_t *payload_length
);

OtaTransport_Status_t OtaTransport_BuildAck(
    const OtaTransport_Ack_t *ack,
    uint8_t payload[OTA_ACK_FIXED_PAYLOAD_SIZE]
);

OtaTransport_Status_t OtaTransport_ParseAck(
    const uint8_t *payload,
    size_t payload_length,
    OtaTransport_Ack_t *ack
);

OtaTransport_Status_t OtaTransport_BuildStatusResponse(
    const OtaTransport_StatusResponse_t *status,
    uint8_t payload[OTA_STATUS_RESPONSE_PAYLOAD_SIZE]
);

OtaTransport_Status_t OtaTransport_ParseStatusResponse(
    const uint8_t *payload,
    size_t payload_length,
    OtaTransport_StatusResponse_t *status
);

#endif /* AEGIS_OTA_TRANSPORT_H */
