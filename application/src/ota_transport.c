#include "ota_transport.h"

#include <stdbool.h>
#include <string.h>

static uint16_t OtaTransport_ReadU16(const uint8_t *input){
    return (uint16_t)input[0] | (uint16_t)((uint16_t)input[1] << 8U);
}

static uint32_t OtaTransport_ReadU32(const uint8_t *input){
    return (uint32_t)input[0] |
           ((uint32_t)input[1] << 8U) |
           ((uint32_t)input[2] << 16U) |
           ((uint32_t)input[3] << 24U);
}

static uint64_t OtaTransport_ReadU64(const uint8_t *input){
    uint64_t value = 0U;

    for(uint32_t index = 0U; index < 8U; ++index){
        value |= (uint64_t)input[index] << (index * 8U);
    }
    return value;
}

static void OtaTransport_WriteU16(uint8_t *output, uint16_t value){
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
}

static void OtaTransport_WriteU32(uint8_t *output, uint32_t value){
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
    output[2] = (uint8_t)(value >> 16U);
    output[3] = (uint8_t)(value >> 24U);
}

static void OtaTransport_WriteU64(uint8_t *output, uint64_t value){
    for(uint32_t index = 0U; index < 8U; ++index){
        output[index] = (uint8_t)(value >> (index * 8U));
    }
}

static bool OtaTransport_ArtifactTypeIsValid(uint8_t value){
    return (value == (uint8_t)OTA_ARTIFACT_FULL) ||
           (value == (uint8_t)OTA_ARTIFACT_DELTA);
}

static bool OtaTransport_StatusIsValid(
    const OtaTransport_StatusResponse_t *status
){
    const uint32_t known_flags =
        OTA_STATUS_FLAG_HEADER_COMMITTED |
        OTA_STATUS_FLAG_ARTIFACT_VALID |
        OTA_STATUS_FLAG_APPLY_STARTED |
        OTA_STATUS_FLAG_BACKUP_VALID |
        OTA_STATUS_FLAG_STAGING_READY |
        OTA_STATUS_FLAG_INSTALLED |
        OTA_STATUS_FLAG_CONFIRMED;

    if((status->receive_state > OTA_RECEIVE_INVALID) ||
       ((status->flags & ~known_flags) != 0U)){
        return false;
    }
    if(status->receive_state == OTA_RECEIVE_EMPTY){
        const uint8_t zero_id[OTA_ARTIFACT_ID_SIZE] = {0U};

        return (status->artifact_type == 0U) &&
               (status->transfer_id == UINT64_C(0)) &&
               (memcmp(status->artifact_id, zero_id, sizeof(zero_id)) == 0) &&
               (status->data_size == 0U) &&
               (status->chunk_size == 0U) &&
               (status->next_missing_offset == 0U) &&
               (status->flags == 0U);
    }
    if(((status->flags & OTA_STATUS_FLAG_CONFIRMED) != 0U) &&
       ((status->flags & OTA_STATUS_FLAG_INSTALLED) == 0U)){
        return false;
    }
    if(((status->flags & OTA_STATUS_FLAG_STAGING_READY) != 0U) &&
       ((status->flags & OTA_STATUS_FLAG_ARTIFACT_VALID) == 0U)){
        return false;
    }
    if(((status->flags & OTA_STATUS_FLAG_APPLY_STARTED) != 0U) &&
       (status->artifact_type != OTA_ARTIFACT_DELTA)){
        return false;
    }
    return OtaTransport_ArtifactTypeIsValid((uint8_t)status->artifact_type) &&
           (status->transfer_id != UINT64_C(0)) &&
           (status->data_size > 0U) &&
           (status->chunk_size > 0U) &&
           (status->chunk_size <= OTA_DATA_CHUNK_MAX_SIZE) &&
           (status->next_missing_offset <= status->data_size);
}

OtaTransport_Status_t OtaTransport_ParseBegin(
    const uint8_t *payload,
    size_t payload_length,
    OtaTransport_Begin_t *begin
){
    if((payload == NULL) || (begin == NULL)){
        return OTA_TRANSPORT_INVALID_ARGUMENT;
    }
    if(payload_length != OTA_BEGIN_FIXED_PAYLOAD_SIZE){
        return OTA_TRANSPORT_BAD_LENGTH;
    }
    if(!OtaTransport_ArtifactTypeIsValid(
        payload[OTA_BEGIN_ARTIFACT_TYPE_OFFSET]
    )){
        return OTA_TRANSPORT_BAD_VALUE;
    }

    begin->transfer_id = OtaTransport_ReadU64(
        &payload[OTA_BEGIN_TRANSFER_ID_OFFSET]
    );
    memcpy(
        begin->artifact_id,
        &payload[OTA_BEGIN_ARTIFACT_ID_OFFSET],
        sizeof(begin->artifact_id)
    );
    begin->operation_id = OtaTransport_ReadU64(
        &payload[OTA_BEGIN_OPERATION_ID_OFFSET]
    );
    begin->artifact_type = (OtaProtocol_ArtifactType_t)
        payload[OTA_BEGIN_ARTIFACT_TYPE_OFFSET];
    begin->manifest_size = OtaTransport_ReadU16(
        &payload[OTA_BEGIN_MANIFEST_SIZE_OFFSET]
    );
    begin->data_size = OtaTransport_ReadU32(
        &payload[OTA_BEGIN_DATA_SIZE_OFFSET]
    );
    begin->chunk_size = OtaTransport_ReadU16(
        &payload[OTA_BEGIN_CHUNK_SIZE_OFFSET]
    );

    if((begin->transfer_id == UINT64_C(0)) ||
       (begin->operation_id == UINT64_C(0)) ||
       (begin->manifest_size == 0U) ||
       (begin->data_size == 0U) ||
       (begin->chunk_size == 0U) ||
       (begin->chunk_size > OTA_DATA_CHUNK_MAX_SIZE)){
        return OTA_TRANSPORT_BAD_VALUE;
    }
    return OTA_TRANSPORT_OK;
}

OtaTransport_Status_t OtaTransport_ParseData(
    const uint8_t *payload,
    size_t payload_length,
    OtaTransport_DataView_t *data
){
    uint16_t declared_length;

    if((payload == NULL) || (data == NULL)){
        return OTA_TRANSPORT_INVALID_ARGUMENT;
    }
    if(payload_length < OTA_DATA_ENVELOPE_SIZE){
        return OTA_TRANSPORT_BAD_LENGTH;
    }
    declared_length = OtaTransport_ReadU16(
        &payload[OTA_DATA_LENGTH_OFFSET]
    );
    if((declared_length == 0U) ||
       (declared_length > OTA_DATA_CHUNK_MAX_SIZE) ||
       (payload_length != (size_t)OTA_DATA_ENVELOPE_SIZE + declared_length)){
        return OTA_TRANSPORT_BAD_LENGTH;
    }
    if((payload[OTA_DATA_SECTION_OFFSET] != OTA_SECTION_MANIFEST) &&
       (payload[OTA_DATA_SECTION_OFFSET] != OTA_SECTION_DATA)){
        return OTA_TRANSPORT_BAD_VALUE;
    }

    data->transfer_id = OtaTransport_ReadU64(
        &payload[OTA_DATA_TRANSFER_ID_OFFSET]
    );
    data->section = (OtaProtocol_Section_t)
        payload[OTA_DATA_SECTION_OFFSET];
    data->offset = OtaTransport_ReadU32(&payload[OTA_DATA_OFFSET_OFFSET]);
    data->data_length = declared_length;
    data->data = &payload[OTA_DATA_BYTES_OFFSET];

    return data->transfer_id == UINT64_C(0) ?
        OTA_TRANSPORT_BAD_VALUE : OTA_TRANSPORT_OK;
}

OtaTransport_Status_t OtaTransport_ParseId(
    const uint8_t *payload,
    size_t payload_length,
    uint64_t *id
){
    if((payload == NULL) || (id == NULL)){
        return OTA_TRANSPORT_INVALID_ARGUMENT;
    }
    if(payload_length != 8U){
        return OTA_TRANSPORT_BAD_LENGTH;
    }
    *id = OtaTransport_ReadU64(payload);
    return OTA_TRANSPORT_OK;
}

OtaTransport_Status_t OtaTransport_BuildBegin(
    const OtaTransport_Begin_t *begin,
    uint8_t payload[OTA_BEGIN_FIXED_PAYLOAD_SIZE]
){
    if((begin == NULL) || (payload == NULL)){
        return OTA_TRANSPORT_INVALID_ARGUMENT;
    }
    if((begin->transfer_id == UINT64_C(0)) ||
       (begin->operation_id == UINT64_C(0)) ||
       !OtaTransport_ArtifactTypeIsValid((uint8_t)begin->artifact_type) ||
       (begin->manifest_size == 0U) || (begin->data_size == 0U) ||
       (begin->chunk_size == 0U) ||
       (begin->chunk_size > OTA_DATA_CHUNK_MAX_SIZE)){
        return OTA_TRANSPORT_BAD_VALUE;
    }

    OtaTransport_WriteU64(&payload[OTA_BEGIN_TRANSFER_ID_OFFSET], begin->transfer_id);
    memcpy(&payload[OTA_BEGIN_ARTIFACT_ID_OFFSET], begin->artifact_id,
           sizeof(begin->artifact_id));
    OtaTransport_WriteU64(&payload[OTA_BEGIN_OPERATION_ID_OFFSET], begin->operation_id);
    payload[OTA_BEGIN_ARTIFACT_TYPE_OFFSET] = (uint8_t)begin->artifact_type;
    OtaTransport_WriteU16(&payload[OTA_BEGIN_MANIFEST_SIZE_OFFSET], begin->manifest_size);
    OtaTransport_WriteU32(&payload[OTA_BEGIN_DATA_SIZE_OFFSET], begin->data_size);
    OtaTransport_WriteU16(&payload[OTA_BEGIN_CHUNK_SIZE_OFFSET], begin->chunk_size);
    return OTA_TRANSPORT_OK;
}

OtaTransport_Status_t OtaTransport_BuildData(
    uint64_t transfer_id,
    OtaProtocol_Section_t section,
    uint32_t offset,
    const uint8_t *data,
    uint16_t data_length,
    uint8_t *payload,
    size_t payload_capacity,
    uint16_t *payload_length
){
    const size_t required = (size_t)OTA_DATA_ENVELOPE_SIZE + data_length;

    if((data == NULL) || (payload == NULL) || (payload_length == NULL)){
        return OTA_TRANSPORT_INVALID_ARGUMENT;
    }
    if((transfer_id == UINT64_C(0)) ||
       ((section != OTA_SECTION_MANIFEST) && (section != OTA_SECTION_DATA))){
        return OTA_TRANSPORT_BAD_VALUE;
    }
    if((data_length == 0U) || (data_length > OTA_DATA_CHUNK_MAX_SIZE) ||
       (payload_capacity < required)){
        return OTA_TRANSPORT_OUT_OF_RANGE;
    }

    OtaTransport_WriteU64(&payload[OTA_DATA_TRANSFER_ID_OFFSET], transfer_id);
    payload[OTA_DATA_SECTION_OFFSET] = (uint8_t)section;
    OtaTransport_WriteU32(&payload[OTA_DATA_OFFSET_OFFSET], offset);
    OtaTransport_WriteU16(&payload[OTA_DATA_LENGTH_OFFSET], data_length);
    memcpy(&payload[OTA_DATA_BYTES_OFFSET], data, data_length);
    *payload_length = (uint16_t)required;
    return OTA_TRANSPORT_OK;
}

OtaTransport_Status_t OtaTransport_BuildAck(
    const OtaTransport_Ack_t *ack,
    uint8_t payload[OTA_ACK_FIXED_PAYLOAD_SIZE]
){
    if((ack == NULL) || (payload == NULL)){
        return OTA_TRANSPORT_INVALID_ARGUMENT;
    }
    OtaTransport_WriteU64(&payload[OTA_ACK_TRANSFER_ID_OFFSET], ack->transfer_id);
    payload[OTA_ACK_REQUEST_TYPE_OFFSET] = ack->request_type;
    payload[OTA_ACK_STATUS_REASON_OFFSET] = ack->status_or_reason;
    OtaTransport_WriteU32(&payload[OTA_ACK_NEXT_OFFSET_OFFSET], ack->next_missing_offset);
    return OTA_TRANSPORT_OK;
}

OtaTransport_Status_t OtaTransport_ParseAck(
    const uint8_t *payload,
    size_t payload_length,
    OtaTransport_Ack_t *ack
){
    if((payload == NULL) || (ack == NULL)){
        return OTA_TRANSPORT_INVALID_ARGUMENT;
    }
    if(payload_length != OTA_ACK_FIXED_PAYLOAD_SIZE){
        return OTA_TRANSPORT_BAD_LENGTH;
    }
    ack->request_type = payload[OTA_ACK_REQUEST_TYPE_OFFSET];
    ack->status_or_reason = payload[OTA_ACK_STATUS_REASON_OFFSET];
    ack->transfer_id = OtaTransport_ReadU64(&payload[OTA_ACK_TRANSFER_ID_OFFSET]);
    ack->next_missing_offset = OtaTransport_ReadU32(&payload[OTA_ACK_NEXT_OFFSET_OFFSET]);
    return OTA_TRANSPORT_OK;
}

OtaTransport_Status_t OtaTransport_BuildStatusResponse(
    const OtaTransport_StatusResponse_t *status,
    uint8_t payload[OTA_STATUS_RESPONSE_PAYLOAD_SIZE]
){
    if((status == NULL) || (payload == NULL)){
        return OTA_TRANSPORT_INVALID_ARGUMENT;
    }
    if(!OtaTransport_StatusIsValid(status)){
        return OTA_TRANSPORT_BAD_VALUE;
    }
    payload[OTA_STATUS_STATE_OFFSET] = (uint8_t)status->receive_state;
    payload[OTA_STATUS_ARTIFACT_TYPE_OFFSET] = (uint8_t)status->artifact_type;
    OtaTransport_WriteU64(&payload[OTA_STATUS_TRANSFER_ID_OFFSET], status->transfer_id);
    memcpy(&payload[OTA_STATUS_ARTIFACT_ID_OFFSET], status->artifact_id,
           sizeof(status->artifact_id));
    OtaTransport_WriteU32(&payload[OTA_STATUS_DATA_SIZE_OFFSET], status->data_size);
    OtaTransport_WriteU16(&payload[OTA_STATUS_CHUNK_SIZE_OFFSET], status->chunk_size);
    OtaTransport_WriteU32(&payload[OTA_STATUS_NEXT_OFFSET_OFFSET], status->next_missing_offset);
    OtaTransport_WriteU32(&payload[OTA_STATUS_FLAGS_OFFSET], status->flags);
    return OTA_TRANSPORT_OK;
}

OtaTransport_Status_t OtaTransport_ParseStatusResponse(
    const uint8_t *payload,
    size_t payload_length,
    OtaTransport_StatusResponse_t *status
){
    if((payload == NULL) || (status == NULL)){
        return OTA_TRANSPORT_INVALID_ARGUMENT;
    }
    if(payload_length != OTA_STATUS_RESPONSE_PAYLOAD_SIZE){
        return OTA_TRANSPORT_BAD_LENGTH;
    }
    status->receive_state = (OtaProtocol_ReceiveState_t)
        payload[OTA_STATUS_STATE_OFFSET];
    status->artifact_type = (OtaProtocol_ArtifactType_t)
        payload[OTA_STATUS_ARTIFACT_TYPE_OFFSET];
    status->transfer_id = OtaTransport_ReadU64(&payload[OTA_STATUS_TRANSFER_ID_OFFSET]);
    memcpy(status->artifact_id, &payload[OTA_STATUS_ARTIFACT_ID_OFFSET],
           sizeof(status->artifact_id));
    status->data_size = OtaTransport_ReadU32(&payload[OTA_STATUS_DATA_SIZE_OFFSET]);
    status->chunk_size = OtaTransport_ReadU16(&payload[OTA_STATUS_CHUNK_SIZE_OFFSET]);
    status->next_missing_offset = OtaTransport_ReadU32(&payload[OTA_STATUS_NEXT_OFFSET_OFFSET]);
    status->flags = OtaTransport_ReadU32(&payload[OTA_STATUS_FLAGS_OFFSET]);
    return OtaTransport_StatusIsValid(status) ?
        OTA_TRANSPORT_OK : OTA_TRANSPORT_BAD_VALUE;
}
