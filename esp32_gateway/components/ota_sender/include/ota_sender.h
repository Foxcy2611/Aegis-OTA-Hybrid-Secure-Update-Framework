#ifndef AEGIS_ESP32_OTA_SENDER_H
#define AEGIS_ESP32_OTA_SENDER_H

/* Stop-and-wait OTA sender; radio/upstream cache được đưa vào bằng callback. */

#include "ota_protocol.h"
#include "ota_transport.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef bool (*OtaSender_ExchangeCallback_t)(
    void *context,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length,
    uint32_t timeout_ms
);

typedef struct{
    OtaProtocol_ArtifactType_t type;
    uint64_t transfer_id;
    uint64_t operation_id;
    uint8_t artifact_id[OTA_ARTIFACT_ID_SIZE];
    const uint8_t *manifest;
    uint16_t manifest_size;
    const uint8_t *data;
    uint32_t data_size;
    uint16_t chunk_size;
} OtaSender_Artifact_t;

typedef enum{
    OTA_SENDER_OK = 0U,
    OTA_SENDER_INVALID_ARGUMENT,
    OTA_SENDER_RADIO_ERROR,
    OTA_SENDER_BAD_RESPONSE,
    OTA_SENDER_REJECTED,
    OTA_SENDER_RETRY_EXHAUSTED,
    OTA_SENDER_COUNTER_EXHAUSTED
} OtaSender_Status_t;

typedef struct{
    uint8_t key[CRYPTO_TRANSPORT_KEY_SIZE];
    uint8_t session_id[CRYPTO_SESSION_ID_SIZE];
    uint32_t tx_counter;
    uint32_t last_rx_counter;
    uint16_t next_sequence;
    uint8_t max_retries;
    uint32_t timeout_ms;
    OtaSender_ExchangeCallback_t exchange;
    void *exchange_context;
    bool has_rx_counter;
} OtaSender_Context_t;

OtaSender_Status_t OtaSender_Init(
    OtaSender_Context_t *context,
    const uint8_t key[CRYPTO_TRANSPORT_KEY_SIZE],
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE],
    uint8_t max_retries,
    uint32_t timeout_ms,
    OtaSender_ExchangeCallback_t exchange,
    void *exchange_context
);

OtaSender_Status_t OtaSender_Run(
    OtaSender_Context_t *context,
    const OtaSender_Artifact_t *artifact
);

/* Query transfer cụ thể; transfer_id = 0 hỏi record đang active. */
OtaSender_Status_t OtaSender_QueryStatus(
    OtaSender_Context_t *context,
    uint64_t transfer_id,
    OtaTransport_StatusResponse_t *persistent_status
);

#endif /* AEGIS_ESP32_OTA_SENDER_H */
