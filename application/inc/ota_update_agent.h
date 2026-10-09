#ifndef AEGIS_OTA_UPDATE_AGENT_H
#define AEGIS_OTA_UPDATE_AGENT_H

/* Application Update Agent: secure command -> storage/verify/backup/mailbox. */

#include "delta_format.h"
#include "ota_transport.h"
#include "secure_frame.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum{
    OTA_AGENT_OK = 0U,
    OTA_AGENT_INVALID_ARGUMENT,
    OTA_AGENT_NOT_READY,
    OTA_AGENT_FRAME_ERROR,
    OTA_AGENT_PROTOCOL_ERROR,
    OTA_AGENT_STORAGE_ERROR,
    OTA_AGENT_AUTH_ERROR,
    OTA_AGENT_POLICY_ERROR,
    OTA_AGENT_RESPONSE_ERROR
} OtaAgent_Status_t;

typedef struct{
    SecureFrame_Context_t secure_frame;
    OtaTransport_Begin_t transfer;
    uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE];
    uint8_t manifest_buffer[DELTA_MANIFEST_RECORD_SIZE];
    uint16_t manifest_received;
    OtaProtocol_ReceiveState_t receive_state;
    bool initialized;
    bool transfer_active;
    bool storage_prepared;
    bool resume_existing;
} OtaAgent_Context_t;

OtaAgent_Status_t OtaAgent_Init(
    OtaAgent_Context_t *context,
    const uint8_t transport_key[CRYPTO_TRANSPORT_KEY_SIZE],
    const uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE]
);

/*
 * Mỗi request hợp lệ sinh đúng một secure ACK/NACK/STATUS response.
 * reset_requested chỉ true sau khi Staging + Backup + mailbox đã commit.
 */
OtaAgent_Status_t OtaAgent_ProcessFrame(
    OtaAgent_Context_t *context,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length,
    bool *reset_requested
);

#endif /* AEGIS_OTA_UPDATE_AGENT_H */
