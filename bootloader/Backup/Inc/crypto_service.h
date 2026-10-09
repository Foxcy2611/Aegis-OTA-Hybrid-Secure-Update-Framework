#ifndef AEGIS_CRYPTO_SERVICE_H
#define AEGIS_CRYPTO_SERVICE_H

#include <stdint.h>

#include "crypto_config.h"

typedef enum {
    CRYPTO_STATUS_OK = 0U,
    CRYPTO_STATUS_INVALID_ARGUMENT,
    CRYPTO_STATUS_INVALID_LENGTH,
    CRYPTO_STATUS_AUTH_FAILED,
    CRYPTO_STATUS_INVALID_STATE,
    CRYPTO_STATUS_INVALID_HEADER,
    CRYPTO_STATUS_INCOMPLETE_IMAGE,
    CRYPTO_STATUS_SIGNATURE_FAILED
} Crypto_Status_t;

/*
 * Giải mã ciphertext và xác thực TAG.
 * Nếu TAG sai, plaintext output bị xóa.
 */
Crypto_Status_t CryptoService_DecryptFrame(
    const uint8_t transport_key[CRYPTO_TRANSPORT_KEY_SIZE],
    const Crypto_Nonce_t *nonce_info,
    const Crypto_FrameAad_t *aad_info,
    const uint8_t *ciphertext,
    uint8_t *plaintext,
    const uint8_t tag[CRYPTO_AEAD_TAG_SIZE]
);

/*
 * Mã hóa plaintext thành CIPHERTEXT và sinh ra TAG 16-Byte
*/
Crypto_Status_t CryptoService_EncryptFrame(
    const uint8_t transport_key[CRYPTO_TRANSPORT_KEY_SIZE],
    const Crypto_Nonce_t *nonce_info,
    const Crypto_FrameAad_t *aad_info,
    const uint8_t *plaintext,
    uint8_t *out_ciphertext,
    uint8_t generated_tag[CRYPTO_AEAD_TAG_SIZE]
);

// ========================= PHÂN LUỒNG CHO SHA và ED Check ========================= //
/*
 * Bắt đầu phiên xác thực
 * Băm Header và thiết lập trạng thái
*/
Crypto_Status_t CryptoService_VerifyBegin(const Crypto_SignedHeader_t *header);

/*
 * Bơm dữ liệu Firmware vào máy băm SHA-512.
 * Có thể gọi nhiều lần với các chunk size khác nhau.
 */
Crypto_Status_t CryptoService_VerifyUpdate(const uint8_t *firmware_data, uint32_t length);

/*
 * Chốt mã băm và đối chiếu chữ ký Ed25519ph.
 * Tự động xóa dọn context sau khi hoàn thành.
 */
Crypto_Status_t CryptoService_VerifyFinal(
    const uint8_t signature[CRYPTO_ED25519_SIGNATURE_SIZE],
    const uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE]
);

/*
 * Hủy bỏ phiên xác thực giữa chừng và dọn dẹp RAM.
 */
void CryptoService_VerifyAbort(void);

#endif /* AEGIS_CRYPTO_SERVICE_H */
