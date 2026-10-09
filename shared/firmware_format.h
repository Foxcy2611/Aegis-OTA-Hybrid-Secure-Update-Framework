#ifndef AEGIS_FIRMWARE_FORMAT_H
#define AEGIS_FIRMWARE_FORMAT_H

/*
 * Quy ước byte của gói firmware đã ký.
 *
 * File này chỉ khóa layout và kích thước dữ liệu dùng chung giữa Release tool,
 * Application và Bootloader. Việc tính SHA-512 và xác thực Ed25519ph thuộc Phase 3.
 */

#include <stddef.h>
#include <stdint.h>

/* Chuỗi ASCII thô đúng 16 byte; byte NUL của chuỗi C không thuộc header. */
#define FIRMWARE_DOMAIN_STRING                 "AEGIS-F103-FW-V1"
#define FIRMWARE_DOMAIN_SIZE                   16U

#define FIRMWARE_FORMAT_VERSION                UINT32_C(1)
#define FIRMWARE_SIGNED_HEADER_SIZE            32U
#define FIRMWARE_SIGNATURE_SIZE                64U

#define FIRMWARE_HEADER_DOMAIN_OFFSET          0U
#define FIRMWARE_HEADER_FORMAT_OFFSET          16U
#define FIRMWARE_HEADER_IMAGE_SIZE_OFFSET      20U
#define FIRMWARE_HEADER_IMAGE_VERSION_OFFSET   24U
#define FIRMWARE_HEADER_IMAGE_CRC32_OFFSET     28U

/*
 * Header 32 byte đứng trước raw firmware trong FULL artifact.
 * Được làm active header
 * Phase 1 chỉ lưu/sao chép quy ước này; Phase 3 mới xác thực nội dung của nó.
 */
typedef struct __attribute__((packed)){
    uint8_t domain[FIRMWARE_DOMAIN_SIZE];
    /* Byte [0..15]: domain tách chữ ký theo project/target. */

    uint32_t format_version;
    /* Byte [16..19]: phiên bản layout signed firmware. */

    uint32_t image_size;
    /* Byte [20..23]: số byte raw firmware thực tế. */

    uint32_t image_version;
    /* Byte [24..27]: phiên bản firmware dùng cho anti-downgrade. */

    uint32_t image_crc32;
    /* Byte [28..31]: CRC32 của raw firmware; không thay thế chữ ký. */
} Firmware_SignedHeader_t;

_Static_assert(
    (sizeof(FIRMWARE_DOMAIN_STRING) - 1U) == FIRMWARE_DOMAIN_SIZE,
    "Firmware domain phải chứa đúng 16 byte thô"
);

_Static_assert(
    offsetof(Firmware_SignedHeader_t, domain) ==
        FIRMWARE_HEADER_DOMAIN_OFFSET,
    "Sai vị trí domain trong signed header"
);

_Static_assert(
    offsetof(Firmware_SignedHeader_t, format_version) ==
        FIRMWARE_HEADER_FORMAT_OFFSET,
    "Sai vị trí format_version trong signed header"
);

_Static_assert(
    offsetof(Firmware_SignedHeader_t, image_size) ==
        FIRMWARE_HEADER_IMAGE_SIZE_OFFSET,
    "Sai vị trí image_size trong signed header"
);

_Static_assert(
    offsetof(Firmware_SignedHeader_t, image_version) ==
        FIRMWARE_HEADER_IMAGE_VERSION_OFFSET,
    "Sai vị trí image_version trong signed header"
);

_Static_assert(
    offsetof(Firmware_SignedHeader_t, image_crc32) ==
        FIRMWARE_HEADER_IMAGE_CRC32_OFFSET,
    "Sai vị trí image_crc32 trong signed header"
);

_Static_assert(
    sizeof(Firmware_SignedHeader_t) == FIRMWARE_SIGNED_HEADER_SIZE,
    "Signed firmware header phải giữ đúng 32 byte"
);

#endif /* AEGIS_FIRMWARE_FORMAT_H */
