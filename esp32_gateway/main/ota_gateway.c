#include "ota_gateway.h"

#include "as32_adapter.h"

#include "esp_timer.h"

#include <stdbool.h>
#include <stddef.h>

static uint16_t OtaGateway_ReadU16(const uint8_t *input){
    return (uint16_t)input[0] | (uint16_t)((uint16_t)input[1] << 8U);
}

static uint32_t OtaGateway_RemainingMs(
    int64_t started_us,
    uint32_t timeout_ms
){
    const int64_t elapsed_us = esp_timer_get_time() - started_us;
    const uint64_t timeout_us = (uint64_t)timeout_ms * UINT64_C(1000);

    if((elapsed_us < 0) || ((uint64_t)elapsed_us >= timeout_us)){
        return 0U;
    }
    const uint64_t remaining_us = timeout_us - (uint64_t)elapsed_us;
    return (uint32_t)((remaining_us + UINT64_C(999)) / UINT64_C(1000));
}

static bool OtaGateway_ReadExact(
    uint8_t *data,
    size_t length,
    int64_t started_us,
    uint32_t timeout_ms
){
    const uint32_t remaining_ms = OtaGateway_RemainingMs(
        started_us, timeout_ms
    );
    size_t received = 0U;

    return (remaining_ms > 0U) &&
           (As32Adapter_Receive(
                data, length, &received, remaining_ms
            ) == AS32_ADAPTER_OK) &&
           (received == length);
}

/* Đọc một secure frame đầy đủ; byte nhiễu trước SYNC bị bỏ qua có giới hạn. */
static bool OtaGateway_ReadFrame(
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length,
    uint32_t timeout_ms
){
    const int64_t started_us = esp_timer_get_time();
    bool first_sync_seen = false;
    bool sync_found = false;
    uint8_t byte;
    uint16_t payload_length;
    size_t remaining;

    if((response == NULL) || (response_length == NULL) ||
       (response_capacity < CRYPTO_FRAME_FIXED_OVERHEAD_SIZE)){
        return false;
    }
    while(OtaGateway_RemainingMs(started_us, timeout_ms) > 0U){
        if(!OtaGateway_ReadExact(
               &byte, 1U, started_us, timeout_ms
           )){
            return false;
        }
        if(!first_sync_seen){
            first_sync_seen = byte == CRYPTO_FRAME_SYNC_BYTE_0;
            continue;
        }
        if(byte == CRYPTO_FRAME_SYNC_BYTE_1){
            response[0] = CRYPTO_FRAME_SYNC_BYTE_0;
            response[1] = CRYPTO_FRAME_SYNC_BYTE_1;
            sync_found = true;
            break;
        }
        first_sync_seen = byte == CRYPTO_FRAME_SYNC_BYTE_0;
    }
    if(!sync_found ||
       !OtaGateway_ReadExact(
           &response[CRYPTO_FRAME_SESSION_ID_OFFSET],
           CRYPTO_FRAME_CIPHERTEXT_OFFSET -
               CRYPTO_FRAME_SESSION_ID_OFFSET,
           started_us,
           timeout_ms
       )){
        return false;
    }
    payload_length = OtaGateway_ReadU16(
        &response[CRYPTO_FRAME_LENGTH_OFFSET]
    );
    if((payload_length > CRYPTO_FRAME_MAX_PAYLOAD_SIZE) ||
       (response_capacity < CRYPTO_FRAME_RAW_SIZE(payload_length))){
        return false;
    }
    remaining = (size_t)payload_length + CRYPTO_AEAD_TAG_SIZE +
        CRYPTO_FRAME_CRC32_SIZE;
    if(!OtaGateway_ReadExact(
           &response[CRYPTO_FRAME_CIPHERTEXT_OFFSET],
           remaining,
           started_us,
           timeout_ms
       )){
        return false;
    }
    *response_length = CRYPTO_FRAME_RAW_SIZE(payload_length);
    return true;
}

static bool OtaGateway_Exchange(
    void *opaque,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length,
    uint32_t timeout_ms
){
    OtaGateway_Context_t *context = opaque;

    if((context == NULL) || !context->initialized ||
       (timeout_ms == 0U)){
        return false;
    }
    As32Adapter_FlushRx();
    if(As32Adapter_Send(request, request_length, timeout_ms) !=
       AS32_ADAPTER_OK){
        return false;
    }
    return OtaGateway_ReadFrame(
        response,
        response_capacity,
        response_length,
        timeout_ms
    );
}

OtaSender_Status_t OtaGateway_Init(
    OtaGateway_Context_t *context,
    const uint8_t key[CRYPTO_TRANSPORT_KEY_SIZE],
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE],
    uint8_t max_retries,
    uint32_t exchange_timeout_ms
){
    OtaSender_Status_t status;

    if(context == NULL){
        return OTA_SENDER_INVALID_ARGUMENT;
    }
    context->initialized = true;
    status = OtaSender_Init(
        &context->sender,
        key,
        session_id,
        max_retries,
        exchange_timeout_ms,
        OtaGateway_Exchange,
        context
    );
    if(status != OTA_SENDER_OK){
        context->initialized = false;
    }
    return status;
}

OtaSender_Status_t OtaGateway_SendArtifact(
    OtaGateway_Context_t *context,
    const OtaSender_Artifact_t *artifact
){
    if((context == NULL) || !context->initialized){
        return OTA_SENDER_INVALID_ARGUMENT;
    }
    return OtaSender_Run(&context->sender, artifact);
}
