#include "ota_sender.h"

#include "ota_transport.h"

#include "monocypher.h"

#include <string.h>

static uint16_t OtaSender_ReadU16(const uint8_t *input){
    return (uint16_t)input[0] | (uint16_t)((uint16_t)input[1] << 8U);
}

static uint32_t OtaSender_ReadU32(const uint8_t *input){
    return (uint32_t)input[0] |
           ((uint32_t)input[1] << 8U) |
           ((uint32_t)input[2] << 16U) |
           ((uint32_t)input[3] << 24U);
}

static void OtaSender_WriteU16(uint8_t *output, uint16_t value){
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
}

static void OtaSender_WriteU32(uint8_t *output, uint32_t value){
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
    output[2] = (uint8_t)(value >> 16U);
    output[3] = (uint8_t)(value >> 24U);
}

static void OtaSender_WriteU64(uint8_t *output, uint64_t value){
    for(uint32_t index = 0U; index < 8U; ++index){
        output[index] = (uint8_t)(value >> (index * 8U));
    }
}

static uint32_t OtaSender_Crc32(const uint8_t *data, size_t length){
    uint32_t crc = UINT32_MAX;

    for(size_t index = 0U; index < length; ++index){
        crc ^= data[index];
        for(uint32_t bit = 0U; bit < 8U; ++bit){
            const uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (UINT32_C(0xEDB88320) & mask);
        }
    }
    return ~crc;
}

static void OtaSender_BuildNonce(
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE],
    uint32_t counter,
    uint8_t nonce[CRYPTO_AEAD_NONCE_SIZE]
){
    memcpy(nonce, session_id, CRYPTO_SESSION_ID_SIZE);
    OtaSender_WriteU32(&nonce[CRYPTO_NONCE_FRAME_COUNTER_OFFSET], counter);
}

static void OtaSender_BuildAad(
    uint8_t type,
    uint16_t sequence,
    uint16_t length,
    uint8_t aad[CRYPTO_FRAME_AAD_SIZE]
){
    aad[0] = type;
    OtaSender_WriteU16(&aad[1], sequence);
    OtaSender_WriteU16(&aad[3], length);
}

static OtaSender_Status_t OtaSender_Encode(
    OtaSender_Context_t *context,
    uint8_t type,
    uint16_t sequence,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t raw[CRYPTO_FRAME_MAX_RAW_SIZE],
    size_t *raw_length
){
    crypto_aead_ctx crypto;
    uint8_t nonce[CRYPTO_AEAD_NONCE_SIZE];
    uint8_t aad[CRYPTO_FRAME_AAD_SIZE];
    uint32_t crc;

    if(context->tx_counter > CRYPTO_HOST_TO_DEVICE_COUNTER_MAX){
        return OTA_SENDER_COUNTER_EXHAUSTED;
    }
    raw[0] = CRYPTO_FRAME_SYNC_BYTE_0;
    raw[1] = CRYPTO_FRAME_SYNC_BYTE_1;
    memcpy(&raw[CRYPTO_FRAME_SESSION_ID_OFFSET],
           context->session_id, CRYPTO_SESSION_ID_SIZE);
    OtaSender_WriteU32(&raw[CRYPTO_FRAME_COUNTER_OFFSET], context->tx_counter);
    raw[CRYPTO_FRAME_TYPE_OFFSET] = type;
    OtaSender_WriteU16(&raw[CRYPTO_FRAME_SEQUENCE_OFFSET], sequence);
    OtaSender_WriteU16(&raw[CRYPTO_FRAME_LENGTH_OFFSET], payload_length);
    OtaSender_BuildNonce(context->session_id, context->tx_counter, nonce);
    OtaSender_BuildAad(type, sequence, payload_length, aad);
    crypto_aead_init_ietf(&crypto, context->key, nonce);
    crypto_aead_write(
        &crypto,
        &raw[CRYPTO_FRAME_CIPHERTEXT_OFFSET],
        &raw[CRYPTO_FRAME_TAG_OFFSET(payload_length)],
        aad,
        sizeof(aad),
        payload,
        payload_length
    );
    crc = OtaSender_Crc32(
        &raw[CRYPTO_FRAME_CRC_COVERAGE_OFFSET],
        CRYPTO_FRAME_CRC_COVERAGE_SIZE(payload_length)
    );
    OtaSender_WriteU32(&raw[CRYPTO_FRAME_CRC32_OFFSET(payload_length)], crc);
    *raw_length = CRYPTO_FRAME_RAW_SIZE(payload_length);
    ++context->tx_counter;
    crypto_wipe(&crypto, sizeof(crypto));
    crypto_wipe(nonce, sizeof(nonce));
    crypto_wipe(aad, sizeof(aad));
    return OTA_SENDER_OK;
}

static OtaSender_Status_t OtaSender_DecodePayload(
    OtaSender_Context_t *context,
    const uint8_t *raw,
    size_t raw_length,
    uint16_t sequence,
    uint8_t expected_type,
    uint8_t alternate_type,
    uint16_t expected_length,
    uint8_t *payload,
    uint8_t *actual_type
){
    crypto_aead_ctx crypto;
    uint8_t nonce[CRYPTO_AEAD_NONCE_SIZE];
    uint8_t aad[CRYPTO_FRAME_AAD_SIZE];
    uint16_t length;
    uint32_t counter;
    int result;

    if((payload == NULL) || (actual_type == NULL) ||
       (raw_length < CRYPTO_FRAME_FIXED_OVERHEAD_SIZE) ||
       (raw[0] != CRYPTO_FRAME_SYNC_BYTE_0) ||
       (raw[1] != CRYPTO_FRAME_SYNC_BYTE_1) ||
       (memcmp(&raw[CRYPTO_FRAME_SESSION_ID_OFFSET], context->session_id,
               CRYPTO_SESSION_ID_SIZE) != 0)){
        return OTA_SENDER_BAD_RESPONSE;
    }
    length = OtaSender_ReadU16(&raw[CRYPTO_FRAME_LENGTH_OFFSET]);
    if((length != expected_length) ||
       (raw_length != CRYPTO_FRAME_RAW_SIZE(length)) ||
       (OtaSender_ReadU32(&raw[CRYPTO_FRAME_CRC32_OFFSET(length)]) !=
        OtaSender_Crc32(
            &raw[CRYPTO_FRAME_CRC_COVERAGE_OFFSET],
            CRYPTO_FRAME_CRC_COVERAGE_SIZE(length)
        )) ||
       (OtaSender_ReadU16(&raw[CRYPTO_FRAME_SEQUENCE_OFFSET]) != sequence)){
        return OTA_SENDER_BAD_RESPONSE;
    }
    counter = OtaSender_ReadU32(&raw[CRYPTO_FRAME_COUNTER_OFFSET]);
    if((counter < CRYPTO_DEVICE_TO_HOST_COUNTER_MIN) ||
       (context->has_rx_counter && (counter <= context->last_rx_counter))){
        return OTA_SENDER_BAD_RESPONSE;
    }
    if((raw[CRYPTO_FRAME_TYPE_OFFSET] != expected_type) &&
       (raw[CRYPTO_FRAME_TYPE_OFFSET] != alternate_type)){
        return OTA_SENDER_BAD_RESPONSE;
    }
    OtaSender_BuildNonce(context->session_id, counter, nonce);
    OtaSender_BuildAad(raw[CRYPTO_FRAME_TYPE_OFFSET], sequence, length, aad);
    crypto_aead_init_ietf(&crypto, context->key, nonce);
    result = crypto_aead_read(
        &crypto,
        payload,
        &raw[CRYPTO_FRAME_TAG_OFFSET(length)],
        aad,
        sizeof(aad),
        &raw[CRYPTO_FRAME_CIPHERTEXT_OFFSET],
        length
    );
    crypto_wipe(&crypto, sizeof(crypto));
    crypto_wipe(nonce, sizeof(nonce));
    crypto_wipe(aad, sizeof(aad));
    if(result != 0){
        crypto_wipe(payload, expected_length);
        return OTA_SENDER_BAD_RESPONSE;
    }
    context->last_rx_counter = counter;
    context->has_rx_counter = true;
    *actual_type = raw[CRYPTO_FRAME_TYPE_OFFSET];
    return OTA_SENDER_OK;
}

static OtaSender_Status_t OtaSender_DecodeAck(
    OtaSender_Context_t *context,
    const uint8_t *raw,
    size_t raw_length,
    uint16_t sequence,
    OtaTransport_Ack_t *ack,
    bool *accepted
){
    uint8_t payload[OTA_ACK_FIXED_PAYLOAD_SIZE];
    uint8_t actual_type;
    OtaSender_Status_t status = OtaSender_DecodePayload(
        context,
        raw,
        raw_length,
        sequence,
        OTA_RESPONSE_ACK,
        OTA_RESPONSE_NACK,
        sizeof(payload),
        payload,
        &actual_type
    );

    if((status != OTA_SENDER_OK) ||
       (OtaTransport_ParseAck(payload, sizeof(payload), ack) !=
            OTA_TRANSPORT_OK)){
        crypto_wipe(payload, sizeof(payload));
        return OTA_SENDER_BAD_RESPONSE;
    }
    *accepted = actual_type == OTA_RESPONSE_ACK;
    crypto_wipe(payload, sizeof(payload));
    return OTA_SENDER_OK;
}

static OtaSender_Status_t OtaSender_Request(
    OtaSender_Context_t *context,
    uint8_t type,
    const uint8_t *payload,
    uint16_t payload_length,
    uint64_t expected_transfer_id,
    OtaTransport_Ack_t *ack
){
    uint8_t request[CRYPTO_FRAME_MAX_RAW_SIZE];
    uint8_t response[CRYPTO_FRAME_MAX_RAW_SIZE];
    const uint16_t sequence = context->next_sequence++;

    for(uint8_t attempt = 0U; attempt <= context->max_retries; ++attempt){
        size_t request_length;
        size_t response_length = 0U;
        bool accepted = false;
        OtaSender_Status_t status = OtaSender_Encode(
            context, type, sequence, payload, payload_length,
            request, &request_length
        );
        if(status != OTA_SENDER_OK){
            return status;
        }
        if(!context->exchange(
               context->exchange_context,
               request, request_length,
               response, sizeof(response), &response_length,
               context->timeout_ms
           )){
            continue;
        }
        status = OtaSender_DecodeAck(
            context, response, response_length, sequence, ack, &accepted
        );
        if(status != OTA_SENDER_OK){
            continue;
        }
        if((ack->request_type != type) ||
           (ack->transfer_id != expected_transfer_id) ||
           (accepted && (ack->status_or_reason != OTA_NACK_NONE))){
            return OTA_SENDER_BAD_RESPONSE;
        }
        return accepted ? OTA_SENDER_OK : OTA_SENDER_REJECTED;
    }
    return OTA_SENDER_RETRY_EXHAUSTED;
}

OtaSender_Status_t OtaSender_QueryStatus(
    OtaSender_Context_t *context,
    uint64_t transfer_id,
    OtaTransport_StatusResponse_t *persistent_status
){
    uint8_t request[CRYPTO_FRAME_MAX_RAW_SIZE];
    uint8_t response[CRYPTO_FRAME_MAX_RAW_SIZE];
    uint8_t query[OTA_STATUS_QUERY_PAYLOAD_SIZE];
    uint8_t payload[OTA_STATUS_RESPONSE_PAYLOAD_SIZE];
    const uint16_t sequence = context != NULL ?
        context->next_sequence++ : 0U;

    if((context == NULL) || (persistent_status == NULL)){
        return OTA_SENDER_INVALID_ARGUMENT;
    }
    OtaSender_WriteU64(query, transfer_id);
    for(uint8_t attempt = 0U; attempt <= context->max_retries; ++attempt){
        size_t request_length;
        size_t response_length = 0U;
        uint8_t actual_type;
        OtaSender_Status_t status = OtaSender_Encode(
            context,
            OTA_COMMAND_STATUS,
            sequence,
            query,
            sizeof(query),
            request,
            &request_length
        );
        if(status != OTA_SENDER_OK){
            return status;
        }
        if(!context->exchange(
               context->exchange_context,
               request,
               request_length,
               response,
               sizeof(response),
               &response_length,
               context->timeout_ms
           )){
            continue;
        }
        status = OtaSender_DecodePayload(
            context,
            response,
            response_length,
            sequence,
            OTA_RESPONSE_STATUS,
            OTA_RESPONSE_STATUS,
            sizeof(payload),
            payload,
            &actual_type
        );
        if((status != OTA_SENDER_OK) ||
           (actual_type != OTA_RESPONSE_STATUS) ||
           (OtaTransport_ParseStatusResponse(
                payload,
                sizeof(payload),
                persistent_status
            ) != OTA_TRANSPORT_OK)){
            crypto_wipe(payload, sizeof(payload));
            continue;
        }
        crypto_wipe(payload, sizeof(payload));
        return OTA_SENDER_OK;
    }
    return OTA_SENDER_RETRY_EXHAUSTED;
}

OtaSender_Status_t OtaSender_Init(
    OtaSender_Context_t *context,
    const uint8_t key[CRYPTO_TRANSPORT_KEY_SIZE],
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE],
    uint8_t max_retries,
    uint32_t timeout_ms,
    OtaSender_ExchangeCallback_t exchange,
    void *exchange_context
){
    bool key_is_nonzero = false;
    bool session_is_nonzero = false;

    if((context == NULL) || (key == NULL) || (session_id == NULL) ||
       (timeout_ms == 0U) || (exchange == NULL)){
        return OTA_SENDER_INVALID_ARGUMENT;
    }
    for(size_t index = 0U; index < CRYPTO_TRANSPORT_KEY_SIZE; ++index){
        key_is_nonzero = key_is_nonzero || (key[index] != 0U);
    }
    for(size_t index = 0U; index < CRYPTO_SESSION_ID_SIZE; ++index){
        session_is_nonzero = session_is_nonzero || (session_id[index] != 0U);
    }
    if(!key_is_nonzero || !session_is_nonzero){
        return OTA_SENDER_INVALID_ARGUMENT;
    }
    memset(context, 0, sizeof(*context));
    memcpy(context->key, key, sizeof(context->key));
    memcpy(context->session_id, session_id, sizeof(context->session_id));
    context->tx_counter = CRYPTO_HOST_TO_DEVICE_COUNTER_MIN;
    context->max_retries = max_retries;
    context->timeout_ms = timeout_ms;
    context->exchange = exchange;
    context->exchange_context = exchange_context;
    return OTA_SENDER_OK;
}

OtaSender_Status_t OtaSender_Run(
    OtaSender_Context_t *context,
    const OtaSender_Artifact_t *artifact
){
    OtaTransport_Begin_t begin;
    OtaTransport_Ack_t ack;
    OtaTransport_StatusResponse_t persistent_status;
    uint8_t payload[CRYPTO_FRAME_MAX_PAYLOAD_SIZE];
    uint16_t payload_length;
    OtaSender_Status_t status;
    uint32_t next_data_offset = 0U;

    if((context == NULL) || (artifact == NULL) ||
       (artifact->manifest == NULL) || (artifact->data == NULL) ||
       (artifact->manifest_size == 0U) || (artifact->data_size == 0U) ||
       (artifact->chunk_size == 0U) ||
       (artifact->chunk_size > OTA_DATA_CHUNK_MAX_SIZE)){
        return OTA_SENDER_INVALID_ARGUMENT;
    }
    memset(&begin, 0, sizeof(begin));
    begin.transfer_id = artifact->transfer_id;
    begin.operation_id = artifact->operation_id;
    begin.artifact_type = artifact->type;
    begin.manifest_size = artifact->manifest_size;
    begin.data_size = artifact->data_size;
    begin.chunk_size = artifact->chunk_size;
    memcpy(begin.artifact_id, artifact->artifact_id, sizeof(begin.artifact_id));
    if(OtaTransport_BuildBegin(&begin, payload) != OTA_TRANSPORT_OK){
        return OTA_SENDER_INVALID_ARGUMENT;
    }
    status = OtaSender_Request(
        context, OTA_COMMAND_BEGIN, payload, OTA_BEGIN_FIXED_PAYLOAD_SIZE,
        artifact->transfer_id, &ack
    );
    if(status != OTA_SENDER_OK){
        return status;
    }

    for(uint32_t offset = 0U; offset < artifact->manifest_size;){
        const uint32_t remaining = artifact->manifest_size - offset;
        const uint16_t chunk = remaining < artifact->chunk_size ?
            (uint16_t)remaining : artifact->chunk_size;
        if(OtaTransport_BuildData(
               artifact->transfer_id, OTA_SECTION_MANIFEST, offset,
               &artifact->manifest[offset], chunk,
               payload, sizeof(payload), &payload_length
           ) != OTA_TRANSPORT_OK){
            return OTA_SENDER_INVALID_ARGUMENT;
        }
        status = OtaSender_Request(
            context, OTA_COMMAND_DATA, payload, payload_length,
            artifact->transfer_id, &ack
        );
        if(status != OTA_SENDER_OK){
            return status;
        }
        next_data_offset = ack.next_missing_offset;
        offset += chunk;
    }

    if(next_data_offset > artifact->data_size ||
       ((next_data_offset % artifact->chunk_size) != 0U &&
        next_data_offset != artifact->data_size)){
        return OTA_SENDER_BAD_RESPONSE;
    }
    for(uint32_t offset = next_data_offset; offset < artifact->data_size;){
        const uint32_t remaining = artifact->data_size - offset;
        const uint16_t chunk = remaining < artifact->chunk_size ?
            (uint16_t)remaining : artifact->chunk_size;
        if(OtaTransport_BuildData(
               artifact->transfer_id, OTA_SECTION_DATA, offset,
               &artifact->data[offset], chunk,
               payload, sizeof(payload), &payload_length
           ) != OTA_TRANSPORT_OK){
            return OTA_SENDER_INVALID_ARGUMENT;
        }
        status = OtaSender_Request(
            context, OTA_COMMAND_DATA, payload, payload_length,
            artifact->transfer_id, &ack
        );
        if(status != OTA_SENDER_OK){
            return status;
        }
        if((ack.next_missing_offset <= offset) ||
           (ack.next_missing_offset > artifact->data_size)){
            return OTA_SENDER_BAD_RESPONSE;
        }
        offset = ack.next_missing_offset;
    }

    for(uint32_t index = 0U; index < 8U; ++index){
        payload[index] = (uint8_t)(artifact->transfer_id >> (index * 8U));
    }
    status = OtaSender_Request(
        context, OTA_COMMAND_END, payload, OTA_END_PAYLOAD_SIZE,
        artifact->transfer_id, &ack
    );
    if(status != OTA_SENDER_OK){
        return status;
    }
    status = OtaSender_QueryStatus(
        context,
        artifact->transfer_id,
        &persistent_status
    );
    if((status != OTA_SENDER_OK) ||
       (persistent_status.receive_state != OTA_RECEIVE_VALID) ||
       (persistent_status.artifact_type != artifact->type) ||
       (persistent_status.transfer_id != artifact->transfer_id) ||
       (memcmp(
            persistent_status.artifact_id,
            artifact->artifact_id,
            OTA_ARTIFACT_ID_SIZE
        ) != 0) ||
       (persistent_status.data_size != artifact->data_size) ||
       (persistent_status.chunk_size != artifact->chunk_size) ||
       ((persistent_status.flags &
         (OTA_STATUS_FLAG_HEADER_COMMITTED |
          OTA_STATUS_FLAG_ARTIFACT_VALID)) !=
        (OTA_STATUS_FLAG_HEADER_COMMITTED |
         OTA_STATUS_FLAG_ARTIFACT_VALID))){
        return status == OTA_SENDER_OK ?
            OTA_SENDER_BAD_RESPONSE : status;
    }
    for(uint32_t index = 0U; index < 8U; ++index){
        payload[index] = (uint8_t)(artifact->operation_id >> (index * 8U));
    }
    return OtaSender_Request(
        context, OTA_COMMAND_BOOT, payload, OTA_BOOT_PAYLOAD_SIZE,
        artifact->transfer_id, &ack
    );
}
