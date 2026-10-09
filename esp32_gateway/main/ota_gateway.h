#ifndef AEGIS_ESP32_OTA_GATEWAY_H
#define AEGIS_ESP32_OTA_GATEWAY_H

/* Nối OTA sender với AS32; artifact/cache được caller cung cấp. */

#include "ota_sender.h"

#include <stdint.h>

typedef struct{
    OtaSender_Context_t sender;
    bool initialized;
} OtaGateway_Context_t;

OtaSender_Status_t OtaGateway_Init(
    OtaGateway_Context_t *context,
    const uint8_t key[CRYPTO_TRANSPORT_KEY_SIZE],
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE],
    uint8_t max_retries,
    uint32_t exchange_timeout_ms
);

/*
 * Caller giữ manifest/data ổn định trong suốt lời gọi này.
 * AS32 adapter phải được init và chuyển sang NORMAL trước đó.
 */
OtaSender_Status_t OtaGateway_SendArtifact(
    OtaGateway_Context_t *context,
    const OtaSender_Artifact_t *artifact
);

#endif /* AEGIS_ESP32_OTA_GATEWAY_H */
