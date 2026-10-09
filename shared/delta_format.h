#ifndef AEGIS_DELTA_FORMAT_H
#define AEGIS_DELTA_FORMAT_H

/* Quy ước byte bền vững của gói Delta và tiến độ nhận. */

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
#define DELTA_PROGRESS_FRAGMENT_MAX            1152U
#define DELTA_RESERVED_ERASED_SIZE             76U

#define DELTA_HEADER_COMMIT_MARKER_VALUE       UINT32_C(0x44484431)
#define DELTA_ARTIFACT_VALID_MARKER_VALUE      UINT32_C(0x44564C31)
#define DELTA_APPLY_STARTED_MARKER_VALUE       UINT32_C(0x44415031)
#define DELTA_ERASED_WORD                      UINT32_C(0xFFFFFFFF)

/* Patch streaming tối giản: COPY(offset nguồn, độ dài), ADD(bytes), END. */
#define DELTA_PATCH_OPCODE_COPY                UINT8_C(0x01)
#define DELTA_PATCH_OPCODE_ADD                 UINT8_C(0x02)
#define DELTA_PATCH_OPCODE_END                 UINT8_C(0xFF)
#define DELTA_PATCH_COPY_RECORD_SIZE           7U
#define DELTA_PATCH_ADD_HEADER_SIZE            3U

typedef struct __attribute__((packed)){
    uint8_t domain[DELTA_DOMAIN_FIELD_SIZE];
    uint32_t format_version;
    uint32_t algorithm_id;
    uint8_t artifact_id[OTA_ARTIFACT_ID_SIZE];
    uint32_t base_version;
    uint32_t base_size;
    uint8_t base_sha512[CRYPTO_SHA512_HASH_SIZE];
    Firmware_SignedHeader_t target_header;
    uint8_t target_sha512[CRYPTO_SHA512_HASH_SIZE];
    uint8_t target_signature[CRYPTO_ED25519_SIGNATURE_SIZE];
    uint32_t patch_size;
    uint8_t patch_sha512[CRYPTO_SHA512_HASH_SIZE];
} Delta_ImmutableBody_t;

typedef struct __attribute__((packed)){
    Delta_ImmutableBody_t immutable_body;
    uint8_t delta_signature[CRYPTO_ED25519_SIGNATURE_SIZE];
    uint64_t operation_id;
    uint64_t transfer_id;
    uint16_t chunk_size;
    uint16_t fragment_count;
    uint32_t record_crc32;
    uint32_t header_commit_marker;
    uint32_t artifact_valid_marker;
    uint32_t apply_started_marker;
    uint8_t reserved_erased[DELTA_RESERVED_ERASED_SIZE];
    uint8_t progress_bitmap[DELTA_PROGRESS_BITMAP_SIZE];
} Delta_Manifest_t;

#define DELTA_MANIFEST_CRC_SIZE                \
    ((uint32_t)offsetof(Delta_Manifest_t, record_crc32))
#define DELTA_MANIFEST_HEADER_MARKER_OFFSET    \
    ((uint32_t)offsetof(Delta_Manifest_t, header_commit_marker))
#define DELTA_MANIFEST_VALID_MARKER_OFFSET     \
    ((uint32_t)offsetof(Delta_Manifest_t, artifact_valid_marker))
#define DELTA_MANIFEST_APPLY_MARKER_OFFSET     \
    ((uint32_t)offsetof(Delta_Manifest_t, apply_started_marker))
#define DELTA_MANIFEST_BITMAP_OFFSET           \
    ((uint32_t)offsetof(Delta_Manifest_t, progress_bitmap))
#define DELTA_MANIFEST_RECORD_SIZE             \
    ((uint32_t)sizeof(Delta_Manifest_t))

_Static_assert(sizeof(DELTA_DOMAIN_STRING) == DELTA_DOMAIN_FIELD_SIZE,
               "Delta domain phải có NUL cuối");
_Static_assert(sizeof(Delta_ImmutableBody_t) == DELTA_IMMUTABLE_BODY_SIZE,
               "Delta immutable body phải đúng 336 byte");
_Static_assert(offsetof(Delta_Manifest_t, delta_signature) == 336U,
               "Sai offset delta_signature");
_Static_assert(offsetof(Delta_Manifest_t, operation_id) == 400U,
               "Sai offset operation_id Delta");
_Static_assert(offsetof(Delta_Manifest_t, record_crc32) == 420U,
               "Sai offset CRC Delta");
_Static_assert(offsetof(Delta_Manifest_t, progress_bitmap) == 512U,
               "Sai offset bitmap Delta");
_Static_assert(sizeof(Delta_Manifest_t) == 656U,
               "Delta manifest phải đúng 656 byte");
_Static_assert(sizeof(Delta_Manifest_t) <= W25Q32_DELTA_MANIFEST_SIZE,
               "Delta manifest phải nằm trong một sector");

#endif /* AEGIS_DELTA_FORMAT_H */
