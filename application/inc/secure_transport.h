#ifndef AEGIS_SECURE_TRANSPORT_H
#define AEGIS_SECURE_TRANSPORT_H

/*
 * Adapter ChaCha20-Poly1305 IETF của STM32 Application.
 *
 * Module này chỉ dựng nonce/AAD và mã hóa hoặc xác thực một payload. Parser
 * Phase 4 vẫn chịu trách nhiệm kiểm session hiện tại, counter tăng đơn điệu,
 * duplicate và replay state trước/sau khi gọi service này.
 */

#include "crypto_config.h"

#include <stdint.h>

typedef enum{
    SECURE_TRANSPORT_OK = 0U,
    /* Phép mã hóa hoặc xác thực hoàn tất. */

    SECURE_TRANSPORT_INVALID_ARGUMENT,
    /* Có con trỏ bắt buộc bằng NULL. */

    SECURE_TRANSPORT_INVALID_LENGTH,
    /* Payload vượt CRYPTO_FRAME_MAX_PAYLOAD_SIZE. */

    SECURE_TRANSPORT_WRONG_COUNTER_DOMAIN,
    /* Counter không thuộc chiều ESP32->STM32 hoặc STM32->ESP32 tương ứng. */

    SECURE_TRANSPORT_AUTH_FAILED
    /* Poly1305 TAG không hợp lệ; plaintext đầu ra đã được xóa. */
} SecureTransport_Status_t;

/* Serialize nonce = session_id[8] || frame_counter_le[4]. */
SecureTransport_Status_t SecureTransport_BuildNonce(
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE],
    uint32_t frame_counter,
    Crypto_Nonce_t *nonce
);

/* Serialize AAD = type[1] || sequence_le[2] || length_le[2]. */
SecureTransport_Status_t SecureTransport_BuildAad(
    uint8_t type,
    uint16_t sequence,
    uint16_t length,
    Crypto_FrameAad_t *aad
);

/*
 * Mã hóa frame STM32 -> ESP32.
 * frame_counter phải thuộc miền DEVICE_TO_HOST.
 */
SecureTransport_Status_t SecureTransport_Encrypt(
    const uint8_t key[CRYPTO_TRANSPORT_KEY_SIZE],
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE],
    uint32_t frame_counter,
    uint8_t type,
    uint16_t sequence,
    const uint8_t *plaintext,
    uint16_t length,
    uint8_t *ciphertext,
    uint8_t tag[CRYPTO_AEAD_TAG_SIZE]
);

/*
 * Xác thực và giải mã frame ESP32 -> STM32.
 * frame_counter phải thuộc miền HOST_TO_DEVICE. Không xử lý plaintext khi
 * hàm trả SECURE_TRANSPORT_AUTH_FAILED.
 */
SecureTransport_Status_t SecureTransport_Decrypt(
    const uint8_t key[CRYPTO_TRANSPORT_KEY_SIZE],
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE],
    uint32_t frame_counter,
    uint8_t type,
    uint16_t sequence,
    const uint8_t *ciphertext,
    uint16_t length,
    const uint8_t tag[CRYPTO_AEAD_TAG_SIZE],
    uint8_t *plaintext
);

#endif /* AEGIS_SECURE_TRANSPORT_H */
