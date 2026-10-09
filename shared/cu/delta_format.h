#ifndef AEGIS_DELTA_FORMAT_H
#define AEGIS_DELTA_FORMAT_H

/* Contract byte chuẩn của Delta manifest; việc apply Delta thuộc Phase 7. */

#include "crypto_config.h"
#include "memory_map.h"
#include "ota_protocol.h"

#include <stddef.h>
#include <stdint.h>

#define DELTA_DOMAIN_STRING                    "AEGIS-F103-DELTA-V1"
#define DELTA_DOMAIN_FIELD_SIZE                20U
#define DELTA_FORMAT_VERSION                   UINT32_C(1)

#define DELTA_ALGORITHM_COPY_ADD_V1            UINT32_C(1)

#define DELTA_IMMUTABLE_BODY_SIZE              336U
#define DELTA_PROGRESS_BITMAP_SIZE             144U
#define DELTA_PROGRESS_FRAGMENT_MAX            (DELTA_PROGRESS_BITMAP_SIZE * 8U)
#define DELTA_RESERVED_ERASED_SIZE             76U

#define DELTA_HEADER_COMMIT_MARKER_VALUE       UINT32_C(0x44484431)
#define DELTA_ARTIFACT_VALID_MARKER_VALUE      UINT32_C(0x44564C31)
#define DELTA_APPLY_STARTED_MARKER_VALUE       UINT32_C(0x44415031)
#define DELTA_ERASED_WORD                      UINT32_C(0xFFFFFFFF)

typedef struct __attribute__((packed)){
    uint8_t domain[DELTA_DOMAIN_FIELD_SIZE];       /* Vị trí 0: domain kèm byte NUL cuối. */
    uint32_t format_version;                       /* Vị trí 20: phiên bản schema. */
    uint32_t algorithm_id;                         /* Vị trí 24: ID profile thuật toán patch. */
    uint8_t artifact_id[OTA_ARTIFACT_ID_SIZE];     /* Vị trí 28: nhận diện Delta. */
    uint32_t base_version;                         /* Vị trí 36: phiên bản base bắt buộc. */
    uint32_t base_size;                            /* Vị trí 40: số byte base bắt buộc. */
    uint8_t base_sha512[CRYPTO_SHA512_HASH_SIZE];  /* Vị trí 44: digest chính xác của base. */
    Crypto_SignedHeader_t target_header;           /* Vị trí 108: target được reconstruct. */
    uint8_t target_sha512[CRYPTO_SHA512_HASH_SIZE];/* Vị trí 140: digest của target. */
    uint8_t target_signature[CRYPTO_ED25519_SIGNATURE_SIZE]; /* Vị trí 204: chữ ký target. */
    uint32_t patch_size;                           /* Vị trí 268: số byte DELTA_DATA. */
    uint8_t patch_sha512[CRYPTO_SHA512_HASH_SIZE]; /* Vị trí 272: digest của patch. */
} Delta_ImmutableBody_t;

typedef struct __attribute__((packed)){
    Delta_ImmutableBody_t immutable_body;          /* Vị trí 0: phần thân được ký. */
    uint8_t delta_signature[CRYPTO_ED25519_SIGNATURE_SIZE]; /* Vị trí 336: chữ ký Delta. */
    uint64_t operation_id;                         /* Vị trí 400: nhận diện lần cài đặt. */
    uint64_t transfer_id;                          /* Vị trí 408: nhận diện lần nhận. */
    uint16_t chunk_size;                           /* Vị trí 416: mức chuẩn 224 byte. */
    uint16_t fragment_count;                       /* Vị trí 418: số chunk dự kiến. */
    uint32_t record_crc32;                         /* Vị trí 420: CRC [0, 420). */
    uint32_t header_commit_marker;                 /* Vị trí 424: công bố header. */
    uint32_t artifact_valid_marker;                /* Vị trí 428: công bố patch. */
    uint32_t apply_started_marker;                 /* Vị trí 432: đánh dấu bắt đầu apply. */
    uint8_t reserved_erased[DELTA_RESERVED_ERASED_SIZE]; /* Vị trí 436..511: giữ erased. */
    uint8_t progress_bitmap[DELTA_PROGRESS_BITMAP_SIZE]; /* Vị trí 512..655: tiến độ nhận. */
} Delta_Manifest_t;

#define DELTA_MANIFEST_CRC_SIZE \
    ((uint32_t)offsetof(Delta_Manifest_t, record_crc32))
#define DELTA_MANIFEST_HEADER_MARKER_OFFSET \
    ((uint32_t)offsetof(Delta_Manifest_t, header_commit_marker))
#define DELTA_MANIFEST_VALID_MARKER_OFFSET \
    ((uint32_t)offsetof(Delta_Manifest_t, artifact_valid_marker))
#define DELTA_MANIFEST_APPLY_MARKER_OFFSET \
    ((uint32_t)offsetof(Delta_Manifest_t, apply_started_marker))
#define DELTA_MANIFEST_BITMAP_OFFSET \
    ((uint32_t)offsetof(Delta_Manifest_t, progress_bitmap))
#define DELTA_MANIFEST_RECORD_SIZE ((uint32_t)sizeof(Delta_Manifest_t))

_Static_assert(sizeof(DELTA_DOMAIN_STRING) == DELTA_DOMAIN_FIELD_SIZE,
               "Field domain Delta phải gồm một byte NUL ở cuối");
_Static_assert(sizeof(Delta_ImmutableBody_t) == DELTA_IMMUTABLE_BODY_SIZE,
               "Phần thân bất biến Delta phải giữ đúng 336 byte");
_Static_assert(offsetof(Delta_Manifest_t, delta_signature) == 336U,
               "Sai vị trí chữ ký Delta");
_Static_assert(offsetof(Delta_Manifest_t, operation_id) == 400U,
               "Sai vị trí operation ID Delta");
_Static_assert(offsetof(Delta_Manifest_t, transfer_id) == 408U,
               "Sai vị trí transfer ID Delta");
_Static_assert(offsetof(Delta_Manifest_t, chunk_size) == 416U,
               "Sai vị trí kích thước chunk Delta");
_Static_assert(offsetof(Delta_Manifest_t, record_crc32) == 420U,
               "Sai vị trí CRC record Delta");
_Static_assert(offsetof(Delta_Manifest_t, header_commit_marker) == 424U,
               "Sai vị trí header marker Delta");
_Static_assert(offsetof(Delta_Manifest_t, artifact_valid_marker) == 428U,
               "Sai vị trí valid marker Delta");
_Static_assert(offsetof(Delta_Manifest_t, apply_started_marker) == 432U,
               "Sai vị trí apply marker Delta");
_Static_assert(offsetof(Delta_Manifest_t, progress_bitmap) == 512U,
               "Sai vị trí bitmap Delta");
_Static_assert(sizeof(Delta_Manifest_t) == 656U,
               "Record Delta manifest phải giữ đúng 656 byte");
_Static_assert(sizeof(Delta_Manifest_t) <= W25Q32_DELTA_MANIFEST_SIZE,
               "Delta manifest phải nằm gọn trong sector dành riêng");
_Static_assert(DELTA_PROGRESS_FRAGMENT_MAX == 1152U,
               "Bitmap tiến độ Delta phải theo dõi đúng 1152 fragment");

#endif /* AEGIS_DELTA_FORMAT_H */
