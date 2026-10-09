#ifndef AEGIS_FIRMWARE_AUTH_H
#define AEGIS_FIRMWARE_AUTH_H

/*
 * Verifier firmware dùng chung cho Application và Bootloader.
 *
 * Module chỉ đọc image qua callback, tính CRC32/SHA-512 và kiểm Ed25519ph.
 * Nó không biết W25Q32, Internal Flash, private key hoặc ChaCha transport.
 */

#include "crypto_config.h"

#include <stdbool.h>
#include <stdint.h>

typedef bool (*FirmwareAuth_ReadCallback_t)(
    void *context,
    uint32_t offset,
    uint8_t *buffer,
    uint32_t length
);

typedef enum{
    FIRMWARE_AUTH_OK = 0U,
    /* Header, CRC32 và Ed25519ph đều hợp lệ. */

    FIRMWARE_AUTH_INVALID_ARGUMENT,
    /* Callback hoặc con trỏ input bắt buộc bằng NULL. */

    FIRMWARE_AUTH_INVALID_HEADER,
    /* Domain, format hoặc image size không đúng contract. */

    FIRMWARE_AUTH_READ_ERROR,
    /* Callback không đọc đủ block image được yêu cầu. */

    FIRMWARE_AUTH_CRC_MISMATCH,
    /* CRC32 image khác image_crc32 trong signed header. */

    FIRMWARE_AUTH_SIGNATURE_FAILED
    /* Ed25519ph signature không khớp header, image hoặc public key. */
} FirmwareAuth_Status_t;

/* Chỉ kiểm tra cấu trúc/range; chưa đọc image và chưa xác thực chữ ký. */
bool FirmwareAuth_HeaderIsValid(const Firmware_SignedHeader_t *header);

/* Serialize SignedHeader thành đúng 32 byte little-endian của artifact. */
bool FirmwareAuth_SerializeHeader(
    const Firmware_SignedHeader_t *header,
    uint8_t bytes[FIRMWARE_SIGNED_HEADER_SIZE]
);

/*
 * Verify SHA-512(header || image) bằng Ed25519ph.
 * reader phải trả true chỉ khi đã ghi đủ length byte vào buffer.
 */
FirmwareAuth_Status_t FirmwareAuth_Verify(
    const Firmware_SignedHeader_t *header,
    const uint8_t signature[CRYPTO_ED25519_SIGNATURE_SIZE],
    const uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE],
    FirmwareAuth_ReadCallback_t reader,
    void *reader_context
);

#endif /* AEGIS_FIRMWARE_AUTH_H */
