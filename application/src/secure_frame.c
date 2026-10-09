#include "secure_frame.h"

#include "crc32.h"
#include "ota_protocol.h"
#include "secure_transport.h"

#include "monocypher.h"

#include <string.h>

static uint16_t SecureFrame_ReadU16(const uint8_t *input){
    return (uint16_t)input[0] | (uint16_t)((uint16_t)input[1] << 8U);
}

static uint32_t SecureFrame_ReadU32(const uint8_t *input){
    return (uint32_t)input[0] |
           ((uint32_t)input[1] << 8U) |
           ((uint32_t)input[2] << 16U) |
           ((uint32_t)input[3] << 24U);
}

static void SecureFrame_WriteU16(uint8_t *output, uint16_t value){
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
}

static void SecureFrame_WriteU32(uint8_t *output, uint32_t value){
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
    output[2] = (uint8_t)(value >> 16U);
    output[3] = (uint8_t)(value >> 24U);
}

static bool SecureFrame_KeyIsNonzero(const uint8_t *key){
    uint8_t combined = 0U;

    for(uint32_t index = 0U; index < CRYPTO_TRANSPORT_KEY_SIZE; ++index){
        combined |= key[index];
    }
    return combined != 0U;
}

SecureFrame_Status_t SecureFrame_Init(
    SecureFrame_Context_t *context,
    const uint8_t key[CRYPTO_TRANSPORT_KEY_SIZE]
){
    if((context == NULL) || (key == NULL)){
        return SECURE_FRAME_INVALID_ARGUMENT;
    }
    if(!SecureFrame_KeyIsNonzero(key)){
        return SECURE_FRAME_BAD_KEY;
    }
    memset(context, 0, sizeof(*context));
    memcpy(context->key, key, sizeof(context->key));
    context->next_tx_counter = CRYPTO_DEVICE_TO_HOST_COUNTER_MIN;
    return SECURE_FRAME_OK;
}

void SecureFrame_SetSessionAcceptor(
    SecureFrame_Context_t *context,
    SecureFrame_SessionAcceptCallback_t session_accept,
    void *session_accept_context
){
    if(context == NULL){
        return;
    }
    context->session_accept = session_accept;
    context->session_accept_context = session_accept_context;
}

void SecureFrame_ResetSession(SecureFrame_Context_t *context){
    if(context == NULL){
        return;
    }
    memset(context->session_id, 0, sizeof(context->session_id));
    context->last_rx_counter = 0U;
    context->next_tx_counter = CRYPTO_DEVICE_TO_HOST_COUNTER_MIN;
    context->session_active = false;
    context->has_rx_counter = false;
}

SecureFrame_Status_t SecureFrame_Decode(
    SecureFrame_Context_t *context,
    const uint8_t *raw,
    size_t raw_length,
    SecureFrame_Message_t *message
){
    uint16_t payload_length;
    uint32_t counter;
    uint32_t stored_crc;
    uint32_t computed_crc;
    bool opening_session;
    SecureTransport_Status_t crypto_status;

    if((context == NULL) || (raw == NULL) || (message == NULL)){
        return SECURE_FRAME_INVALID_ARGUMENT;
    }
    if(raw_length < CRYPTO_FRAME_FIXED_OVERHEAD_SIZE){
        return SECURE_FRAME_BAD_LENGTH;
    }
    if((raw[0] != CRYPTO_FRAME_SYNC_BYTE_0) ||
       (raw[1] != CRYPTO_FRAME_SYNC_BYTE_1)){
        return SECURE_FRAME_BAD_SYNC;
    }
    payload_length = SecureFrame_ReadU16(&raw[CRYPTO_FRAME_LENGTH_OFFSET]);
    if((payload_length > CRYPTO_FRAME_MAX_PAYLOAD_SIZE) ||
       (raw_length != CRYPTO_FRAME_RAW_SIZE(payload_length))){
        return SECURE_FRAME_BAD_LENGTH;
    }
    stored_crc = SecureFrame_ReadU32(
        &raw[CRYPTO_FRAME_CRC32_OFFSET(payload_length)]
    );
    computed_crc = Crc32_Compute(
        &raw[CRYPTO_FRAME_CRC_COVERAGE_OFFSET],
        CRYPTO_FRAME_CRC_COVERAGE_SIZE(payload_length)
    );
    if(stored_crc != computed_crc){
        return SECURE_FRAME_BAD_CRC;
    }

    counter = SecureFrame_ReadU32(&raw[CRYPTO_FRAME_COUNTER_OFFSET]);
    if(counter > CRYPTO_HOST_TO_DEVICE_COUNTER_MAX){
        return SECURE_FRAME_REPLAY;
    }
    opening_session = !context->session_active ||
        (memcmp(context->session_id,
                &raw[CRYPTO_FRAME_SESSION_ID_OFFSET],
                CRYPTO_SESSION_ID_SIZE) != 0);
    /* Session mới chỉ được mở bằng BEGIN/STATUS và phải qua TAG + journal. */
    if(opening_session &&
       (raw[CRYPTO_FRAME_TYPE_OFFSET] != OTA_COMMAND_BEGIN) &&
       (raw[CRYPTO_FRAME_TYPE_OFFSET] != OTA_COMMAND_STATUS)){
        return SECURE_FRAME_BAD_SESSION;
    }
    if(!opening_session && context->has_rx_counter &&
       (counter <= context->last_rx_counter)){
        return SECURE_FRAME_REPLAY;
    }

    message->type = raw[CRYPTO_FRAME_TYPE_OFFSET];
    message->sequence = SecureFrame_ReadU16(&raw[CRYPTO_FRAME_SEQUENCE_OFFSET]);
    message->payload_length = payload_length;
    crypto_status = SecureTransport_Decrypt(
        context->key,
        &raw[CRYPTO_FRAME_SESSION_ID_OFFSET],
        counter,
        message->type,
        message->sequence,
        &raw[CRYPTO_FRAME_CIPHERTEXT_OFFSET],
        payload_length,
        &raw[CRYPTO_FRAME_TAG_OFFSET(payload_length)],
        message->payload
    );
    if(crypto_status != SECURE_TRANSPORT_OK){
        crypto_wipe(message->payload, sizeof(message->payload));
        return SECURE_FRAME_AUTH_FAILED;
    }

    if(opening_session && (context->session_accept != NULL) &&
       !context->session_accept(
           context->session_accept_context,
           &raw[CRYPTO_FRAME_SESSION_ID_OFFSET]
       )){
        crypto_wipe(message->payload, sizeof(message->payload));
        return SECURE_FRAME_BAD_SESSION;
    }
    if(opening_session){
        memcpy(context->session_id,
               &raw[CRYPTO_FRAME_SESSION_ID_OFFSET],
               CRYPTO_SESSION_ID_SIZE);
        context->session_active = true;
        context->next_tx_counter = CRYPTO_DEVICE_TO_HOST_COUNTER_MIN;
    }
    context->last_rx_counter = counter;
    context->has_rx_counter = true;
    return SECURE_FRAME_OK;
}

SecureFrame_Status_t SecureFrame_Encode(
    SecureFrame_Context_t *context,
    uint8_t type,
    uint16_t sequence,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t *raw,
    size_t raw_capacity,
    size_t *raw_length
){
    size_t required_size;
    uint32_t crc;
    SecureTransport_Status_t crypto_status;

    if((context == NULL) || (raw == NULL) || (raw_length == NULL) ||
       ((payload_length > 0U) && (payload == NULL))){
        return SECURE_FRAME_INVALID_ARGUMENT;
    }
    if(!context->session_active){
        return SECURE_FRAME_BAD_SESSION;
    }
    if(payload_length > CRYPTO_FRAME_MAX_PAYLOAD_SIZE){
        return SECURE_FRAME_BAD_LENGTH;
    }
    required_size = CRYPTO_FRAME_RAW_SIZE(payload_length);
    if(raw_capacity < required_size){
        return SECURE_FRAME_BAD_LENGTH;
    }
    if(context->next_tx_counter == CRYPTO_DEVICE_TO_HOST_COUNTER_MAX){
        return SECURE_FRAME_COUNTER_EXHAUSTED;
    }

    raw[0] = CRYPTO_FRAME_SYNC_BYTE_0;
    raw[1] = CRYPTO_FRAME_SYNC_BYTE_1;
    memcpy(&raw[CRYPTO_FRAME_SESSION_ID_OFFSET],
           context->session_id, CRYPTO_SESSION_ID_SIZE);
    SecureFrame_WriteU32(&raw[CRYPTO_FRAME_COUNTER_OFFSET],
                         context->next_tx_counter);
    raw[CRYPTO_FRAME_TYPE_OFFSET] = type;
    SecureFrame_WriteU16(&raw[CRYPTO_FRAME_SEQUENCE_OFFSET], sequence);
    SecureFrame_WriteU16(&raw[CRYPTO_FRAME_LENGTH_OFFSET], payload_length);

    crypto_status = SecureTransport_Encrypt(
        context->key,
        context->session_id,
        context->next_tx_counter,
        type,
        sequence,
        payload,
        payload_length,
        &raw[CRYPTO_FRAME_CIPHERTEXT_OFFSET],
        &raw[CRYPTO_FRAME_TAG_OFFSET(payload_length)]
    );
    if(crypto_status != SECURE_TRANSPORT_OK){
        return SECURE_FRAME_AUTH_FAILED;
    }
    crc = Crc32_Compute(
        &raw[CRYPTO_FRAME_CRC_COVERAGE_OFFSET],
        CRYPTO_FRAME_CRC_COVERAGE_SIZE(payload_length)
    );
    SecureFrame_WriteU32(&raw[CRYPTO_FRAME_CRC32_OFFSET(payload_length)], crc);
    ++context->next_tx_counter;
    *raw_length = required_size;
    return SECURE_FRAME_OK;
}
