#ifndef AEGIS_ARTIFACT_MANIFEST_H
#define AEGIS_ARTIFACT_MANIFEST_H

/*
 * Quy ước byte bền vững của Staging Manifest và Backup Manifest.
 *
 * File này chỉ chứa:
 * - Hằng số lưu bền vững.
 * - Layout Staging/Backup manifest.
 * - Kiểm tra offset và kích thước khi biên dịch.
 *
 * File này không chứa hàm xử lý lưu trữ.
 */

#include "firmware_format.h"
#include "memory_map.h"

#include <stddef.h>
#include <stdint.h>

#define STAGING_MANIFEST_MAGIC_VALUE            UINT32_C(0x53544731)
#define STAGING_MANIFEST_FORMAT_VERSION         UINT32_C(1)

#define STAGING_HEADER_COMMIT_MARKER_VALUE      UINT32_C(0x48445231)
#define STAGING_ARTIFACT_VALID_MARKER_VALUE     UINT32_C(0x56414C31)
#define ARTIFACT_MANIFEST_ERASED_WORD           UINT32_C(0xFFFFFFFF)

#define STAGING_ARTIFACT_TYPE_FULL              UINT8_C(1)
#define STAGING_DATA_CHUNK_MAX_SIZE             UINT16_C(224)

#define STAGING_ARTIFACT_ID_SIZE                8U
#define STAGING_PROGRESS_BITMAP_SIZE            144U
#define STAGING_PROGRESS_FRAGMENT_MAX           (STAGING_PROGRESS_BITMAP_SIZE * 8U)

#define STAGING_RESERVED_ERASED_SIZE            108U

typedef struct __attribute__((packed)){
    uint32_t magic;
    /* Byte [0..3]: nhận diện Staging Manifest. */

    uint32_t format_version;
    /* Byte [4..7]: phiên bản layout manifest. */

    uint8_t artifact_type;
    /* Byte [8]: Staging chỉ chứa FULL target image. */

    uint8_t reserved0;
    /* Byte [9]: phải bằng 0. */

    uint16_t chunk_size;
    /* Byte [10..11]: số byte tối đa của mỗi fragment. */

    uint64_t operation_id;
    /* Byte [12..19]: nhận diện transaction cài đặt. */

    uint64_t transfer_id;
    /* Byte [20..27]: nhận diện phiên truyền. */

    uint8_t artifact_id[STAGING_ARTIFACT_ID_SIZE];
    /* Byte [28..35]: nhận diện candidate firmware. */

    Firmware_SignedHeader_t image_header;
    /* Byte [36..67]: signed header của candidate. */

    uint8_t image_signature[FIRMWARE_SIGNATURE_SIZE];
    /* Byte [68..131]: chữ ký Ed25519ph của candidate. */

    uint32_t data_size;
    /* Byte [132..135]: số byte raw firmware. */

    uint16_t fragment_count;
    /* Byte [136..137]: số fragment cần nhận. */

    uint16_t reserved1;
    /* Byte [138..139]: phải bằng 0. */

    uint32_t header_crc32;
    /* Byte [140..143]: CRC32 của byte [0..139]. */

    uint32_t header_commit_marker;
    /* Byte [144..147]: ghi cuối khi publish header. */

    uint8_t reserved_erased[STAGING_RESERVED_ERASED_SIZE];
    /* Byte [148..255]: phải giữ erased 0xFF. */

    uint8_t progress_bitmap[STAGING_PROGRESS_BITMAP_SIZE];
    /* Byte [256..399]: bit 1 chưa nhận, bit 0 đã ghi và kiểm tra lại. */

    uint32_t artifact_valid_marker;
    /* Byte [400..403]: ghi sau khi toàn artifact đã được xác thực. */
} Staging_Manifest_t;

#define STAGING_MANIFEST_HEADER_CRC_SIZE         \
    ((uint32_t)offsetof(Staging_Manifest_t, header_crc32))

#define STAGING_MANIFEST_HEADER_MARKER_OFFSET    \
    ((uint32_t)offsetof(Staging_Manifest_t, header_commit_marker))

#define STAGING_MANIFEST_BITMAP_OFFSET           \
    ((uint32_t)offsetof(Staging_Manifest_t, progress_bitmap))

#define STAGING_MANIFEST_VALID_MARKER_OFFSET     \
    ((uint32_t)offsetof(Staging_Manifest_t, artifact_valid_marker))

#define STAGING_MANIFEST_RECORD_SIZE             \
    ((uint32_t)sizeof(Staging_Manifest_t))

/* ==================== Backup Manifest ==================== */

#define BACKUP_MANIFEST_MAGIC_VALUE              UINT32_C(0x424B5550)
#define BACKUP_MANIFEST_FORMAT_VERSION           UINT32_C(2)
#define BACKUP_MANIFEST_COMMIT_MARKER_VALUE      UINT32_C(0x434D4954)

/*
 * Manifest mô tả bản App cũ đã được copy sang Backup Image.
 * Commit marker được ghi cuối cùng sau khi image và thân manifest đã kiểm tra.
 */
typedef struct __attribute__((packed)){
    uint32_t magic;
    /* Byte [0..3]: nhận diện Backup Manifest. */

    uint32_t format_version;
    /* Byte [4..7]: phiên bản layout manifest, không phải version firmware. */

    uint64_t operation_id;
    /* Byte [8..15]: liên kết Backup với đúng transaction cài đặt. */

    Firmware_SignedHeader_t image_header;
    /* Byte [16..47]: exact SignedHeader của App cũ. */

    uint8_t image_signature[FIRMWARE_SIGNATURE_SIZE];
    /* Byte [48..111]: exact Ed25519ph signature của App cũ. */

    uint32_t manifest_crc32;
    /* Byte [112..115]: CRC32 của byte [0..111]. */

    uint32_t commit_marker;
    /* Byte [116..119]: ghi cuối để công bố Backup hoàn chỉnh. */
} Backup_Manifest_t;

#define BACKUP_MANIFEST_CRC_SIZE                  \
    ((uint32_t)offsetof(Backup_Manifest_t, manifest_crc32))

#define BACKUP_MANIFEST_COMMIT_OFFSET             \
    ((uint32_t)offsetof(Backup_Manifest_t, commit_marker))

#define BACKUP_MANIFEST_RECORD_SIZE               \
    ((uint32_t)sizeof(Backup_Manifest_t))

_Static_assert(
    offsetof(Staging_Manifest_t, operation_id) == 12U,
    "Sai vị trí operation_id"
);

_Static_assert(
    offsetof(Staging_Manifest_t, image_header) == 36U,
    "Sai vị trí image_header"
);

_Static_assert(
    offsetof(Staging_Manifest_t, image_signature) == 68U,
    "Sai vị trí image_signature"
);

_Static_assert(
    offsetof(Staging_Manifest_t, header_crc32) == 140U,
    "Sai vị trí header_crc32"
);

_Static_assert(
    offsetof(Staging_Manifest_t, header_commit_marker) == 144U,
    "Sai vị trí header_commit_marker"
);

_Static_assert(
    offsetof(Staging_Manifest_t, progress_bitmap) == 256U,
    "Sai vị trí progress_bitmap"
);

_Static_assert(
    offsetof(Staging_Manifest_t, artifact_valid_marker) == 400U,
    "Sai vị trí artifact_valid_marker"
);

_Static_assert(
    sizeof(Staging_Manifest_t) == 404U,
    "Staging Manifest phải giữ đúng 404 byte"
);

_Static_assert(
    sizeof(Staging_Manifest_t) <= W25Q32_STAGING_MANIFEST_SIZE,
    "Staging Manifest phải nằm gọn trong một sector"
);

_Static_assert(
    offsetof(Backup_Manifest_t, operation_id) == 8U,
    "Sai vị trí operation_id trong Backup Manifest"
);

_Static_assert(
    offsetof(Backup_Manifest_t, image_header) == 16U,
    "Sai vị trí image_header trong Backup Manifest"
);

_Static_assert(
    offsetof(Backup_Manifest_t, image_signature) == 48U,
    "Sai vị trí image_signature trong Backup Manifest"
);

_Static_assert(
    offsetof(Backup_Manifest_t, manifest_crc32) == 112U,
    "Sai vị trí manifest_crc32 trong Backup Manifest"
);

_Static_assert(
    offsetof(Backup_Manifest_t, commit_marker) == 116U,
    "Sai vị trí commit_marker trong Backup Manifest"
);

_Static_assert(
    sizeof(Backup_Manifest_t) == 120U,
    "Backup Manifest phải giữ đúng 120 byte"
);

_Static_assert(
    sizeof(Backup_Manifest_t) <= W25Q32_BACKUP_MANIFEST_SIZE,
    "Backup Manifest phải nằm gọn trong một sector"
);

#endif /* AEGIS_ARTIFACT_MANIFEST_H */
