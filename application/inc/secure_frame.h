#ifndef AEGIS_SECURE_FRAME_H
#define AEGIS_SECURE_FRAME_H

/* Stateful secure-frame codec: framing, CRC, AEAD, session và anti-replay. */

#include "crypto_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum{
    SECURE_FRAME_OK = 0U,
    SECURE_FRAME_INVALID_ARGUMENT,
    SECURE_FRAME_BAD_KEY,
    SECURE_FRAME_BAD_SYNC,
    SECURE_FRAME_BAD_LENGTH,
    SECURE_FRAME_BAD_CRC,
    SECURE_FRAME_BAD_SESSION,
    SECURE_FRAME_REPLAY,
    SECURE_FRAME_AUTH_FAILED,
    SECURE_FRAME_COUNTER_EXHAUSTED
} SecureFrame_Status_t;

typedef bool (*SecureFrame_SessionAcceptCallback_t)(
    void *context,
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE]
);

typedef struct{
    uint8_t key[CRYPTO_TRANSPORT_KEY_SIZE];
    uint8_t session_id[CRYPTO_SESSION_ID_SIZE];
    uint32_t last_rx_counter;
    uint32_t next_tx_counter;
    SecureFrame_SessionAcceptCallback_t session_accept;
    /* Gate persistent được gọi trước khi publish session mới. */

    void *session_accept_context;
    /* Context opaque truyền lại cho session_accept. */
    bool session_active;
    bool has_rx_counter;
} SecureFrame_Context_t;

typedef struct{
    uint8_t type;
    uint16_t sequence;
    uint16_t payload_length;
    uint8_t payload[CRYPTO_FRAME_MAX_PAYLOAD_SIZE];
} SecureFrame_Message_t;

SecureFrame_Status_t SecureFrame_Init(
    SecureFrame_Context_t *context,
    const uint8_t key[CRYPTO_TRANSPORT_KEY_SIZE]
);

void SecureFrame_SetSessionAcceptor(
    SecureFrame_Context_t *context,
    SecureFrame_SessionAcceptCallback_t session_accept,
    void *session_accept_context
);

void SecureFrame_ResetSession(SecureFrame_Context_t *context);

SecureFrame_Status_t SecureFrame_Decode(
    SecureFrame_Context_t *context,
    const uint8_t *raw,
    size_t raw_length,
    SecureFrame_Message_t *message
);

SecureFrame_Status_t SecureFrame_Encode(
    SecureFrame_Context_t *context,
    uint8_t type,
    uint16_t sequence,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t *raw,
    size_t raw_capacity,
    size_t *raw_length
);

#endif /* AEGIS_SECURE_FRAME_H */
