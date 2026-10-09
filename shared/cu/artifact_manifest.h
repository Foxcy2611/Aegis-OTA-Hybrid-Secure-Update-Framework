#ifndef AEGIS_ARTIFACT_MANIFEST_H
#define AEGIS_ARTIFACT_MANIFEST_H

/* Contract manifest Staging/Backup bền vững dùng chung cho hai firmware image. */

#include "crypto_config.h"
#include "memory_map.h"
#include "ota_protocol.h"

#include <stddef.h>
#include <stdint.h>

#define STAGING_MANIFEST_MAGIC_VALUE          UINT32_C(0x53544731)
#define STAGING_MANIFEST_FORMAT_VERSION       UINT32_C(1)
#define STAGING_HEADER_COMMIT_MARKER_VALUE    UINT32_C(0x48445231)
#define STAGING_ARTIFACT_VALID_MARKER_VALUE   UINT32_C(0x56414C31)
#define ARTIFACT_MANIFEST_ERASED_WORD         UINT32_C(0xFFFFFFFF)

#define STAGING_PROGRESS_BITMAP_SIZE          144U
#define STAGING_PROGRESS_FRAGMENT_MAX         (STAGING_PROGRESS_BITMAP_SIZE * 8U)
#define STAGING_RESERVED_ERASED_SIZE           108U

typedef struct __attribute__((packed)){
    uint32_t magic;                                      /* Vị trí 0: nhận diện record. */
    uint32_t format_version;                             /* Vị trí 4: phiên bản schema. */
    uint8_t artifact_type;                               /* Vị trí 8: phải là FULL. */
    uint8_t reserved0;                                   /* Vị trí 9: phải bằng 0. */
    uint16_t chunk_size;                                 /* Vị trí 10: kích thước data chunk. */
    uint64_t operation_id;                               /* Vị trí 12: nhận diện lần cài đặt. */
    uint64_t transfer_id;                                /* Vị trí 20: nhận diện lần truyền. */
    uint8_t artifact_id[OTA_ARTIFACT_ID_SIZE];           /* Vị trí 28: nhận diện target. */
    Crypto_SignedHeader_t image_header;                  /* Vị trí 36: header của target. */
    uint8_t image_signature[CRYPTO_ED25519_SIGNATURE_SIZE]; /* Vị trí 68: chữ ký target. */
    uint32_t data_size;                                  /* Vị trí 132: số byte image thô. */
    uint16_t fragment_count;                             /* Vị trí 136: số chunk dự kiến. */
    uint16_t reserved1;                                  /* Vị trí 138: phải bằng 0. */
    uint32_t header_crc32;                               /* Vị trí 140: CRC [0, 140). */
    uint32_t header_commit_marker;                       /* Vị trí 144: công bố header. */
    uint8_t reserved_erased[STAGING_RESERVED_ERASED_SIZE]; /* Vị trí 148..255: giữ erased. */
    uint8_t progress_bitmap[STAGING_PROGRESS_BITMAP_SIZE]; /* Vị trí 256..399: tiến độ nhận. */
    uint32_t artifact_valid_marker;                      /* Vị trí 400: công bố cuối cùng. */
} Staging_Manifest_t;

#define STAGING_MANIFEST_HEADER_CRC_SIZE \
    ((uint32_t)offsetof(Staging_Manifest_t, header_crc32))
#define STAGING_MANIFEST_HEADER_MARKER_OFFSET \
    ((uint32_t)offsetof(Staging_Manifest_t, header_commit_marker))
#define STAGING_MANIFEST_BITMAP_OFFSET \
    ((uint32_t)offsetof(Staging_Manifest_t, progress_bitmap))
#define STAGING_MANIFEST_VALID_MARKER_OFFSET \
    ((uint32_t)offsetof(Staging_Manifest_t, artifact_valid_marker))
#define STAGING_MANIFEST_RECORD_SIZE ((uint32_t)sizeof(Staging_Manifest_t))

#define BACKUP_MANIFEST_MAGIC_VALUE          UINT32_C(0x424B5550)
#define BACKUP_MANIFEST_FORMAT_VERSION       UINT32_C(2)
#define BACKUP_MANIFEST_COMMIT_MARKER_VALUE  UINT32_C(0x434D4954)

typedef struct __attribute__((packed)){
    uint32_t magic;                                      /* Vị trí 0: nhận diện record. */
    uint32_t format_version;                             /* Vị trí 4: phiên bản schema. */
    uint64_t operation_id;                               /* Vị trí 8: nhận diện lần cài đặt. */
    Crypto_SignedHeader_t image_header;                  /* Vị trí 16: header App đang chạy. */
    uint8_t image_signature[CRYPTO_ED25519_SIGNATURE_SIZE]; /* Vị trí 48: chữ ký App. */
    uint32_t manifest_crc32;                             /* Vị trí 112: CRC [0,112). */
    uint32_t commit_marker;                              /* Vị trí 116: công bố cuối cùng. */
} Backup_Manifest_t;

#define BACKUP_MANIFEST_CRC_SIZE \
    ((uint32_t)offsetof(Backup_Manifest_t, manifest_crc32))
#define BACKUP_MANIFEST_COMMIT_OFFSET \
    ((uint32_t)offsetof(Backup_Manifest_t, commit_marker))
#define BACKUP_MANIFEST_RECORD_SIZE ((uint32_t)sizeof(Backup_Manifest_t))

_Static_assert(offsetof(Staging_Manifest_t, operation_id) == 12U,
               "Sai vị trí operation ID trong Staging");
_Static_assert(offsetof(Staging_Manifest_t, image_header) == 36U,
               "Sai vị trí signed header trong Staging");
_Static_assert(offsetof(Staging_Manifest_t, image_signature) == 68U,
               "Sai vị trí chữ ký trong Staging");
_Static_assert(offsetof(Staging_Manifest_t, header_crc32) == 140U,
               "Sai vị trí CRC header trong Staging");
_Static_assert(offsetof(Staging_Manifest_t, progress_bitmap) == 256U,
               "Sai vị trí bitmap trong Staging");
_Static_assert(offsetof(Staging_Manifest_t, artifact_valid_marker) == 400U,
               "Sai vị trí valid marker trong Staging");
_Static_assert(sizeof(Staging_Manifest_t) == 404U,
               "Record Staging manifest phải giữ đúng 404 byte");
_Static_assert(sizeof(Staging_Manifest_t) <= W25Q32_STAGING_MANIFEST_SIZE,
               "Staging manifest phải nằm gọn trong sector dành riêng");

_Static_assert(offsetof(Backup_Manifest_t, operation_id) == 8U,
               "Sai vị trí operation ID trong Backup");
_Static_assert(offsetof(Backup_Manifest_t, image_header) == 16U,
               "Sai vị trí signed header trong Backup");
_Static_assert(offsetof(Backup_Manifest_t, image_signature) == 48U,
               "Sai vị trí chữ ký trong Backup");
_Static_assert(offsetof(Backup_Manifest_t, manifest_crc32) == 112U,
               "Sai vị trí CRC trong Backup");
_Static_assert(offsetof(Backup_Manifest_t, commit_marker) == 116U,
               "Sai vị trí commit marker trong Backup");
_Static_assert(sizeof(Backup_Manifest_t) == 120U,
               "Record Backup manifest phải giữ đúng 120 byte");
_Static_assert(sizeof(Backup_Manifest_t) <= W25Q32_BACKUP_MANIFEST_SIZE,
               "Backup manifest phải nằm gọn trong sector dành riêng");

#endif /* AEGIS_ARTIFACT_MANIFEST_H */
