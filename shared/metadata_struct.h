#ifndef AEGIS_METADATA_STRUCT_H
#define AEGIS_METADATA_STRUCT_H

/*
 * Quy ước byte Metadata v2 dùng chung giữa các thành phần.
 *
 * File này chỉ chứa:
 * - Hằng số lưu bền vững.
 * - Enum lưu bền vững.
 * - Layout record.
 * - Kiểm tra offset/kích thước lúc biên dịch.
 *
 * File này không chứa API xử lý metadata.
 */

#include "firmware_format.h"
#include "memory_map.h"

#include <stddef.h>
#include <stdint.h>

#define METADATA_MAGIC_VALUE              UINT32_C(0x41454749)
#define METADATA_FORMAT_VERSION           UINT32_C(2)
#define METADATA_COMMIT_MARKER_VALUE      UINT32_C(0x434D4954)
#define METADATA_ERASED_WORD              UINT32_C(0xFFFFFFFF)

#define METADATA_TRIAL_BOOT_FALSE         UINT8_C(0)
#define METADATA_TRIAL_BOOT_TRUE          UINT8_C(1)

typedef enum{
    METADATA_OPERATION_IDLE = 0U,
    /* Không có thao tác Internal Flash đang dở. */

    METADATA_OPERATION_INSTALL_IN_PROGRESS = 1U,
    /* Bootloader cần tiếp tục cài firmware từ Staging. */

    METADATA_OPERATION_ROLLBACK_IN_PROGRESS = 2U
    /* Bootloader cần tiếp tục khôi phục firmware từ Backup. */
} Metadata_OperationState_t;

typedef struct __attribute__((packed)){
    uint32_t magic;
    /* Byte [0..3]: nhận diện Metadata Aegis. */

    uint32_t format_version;
    /* Byte [4..7]: phiên bản layout metadata. */

    uint32_t generation;
    /* Byte [8..11]: dùng chọn record A/B mới hơn. */

    uint8_t operation_state;
    /* Byte [12]: giá trị thuộc Metadata_OperationState_t. */

    uint8_t trial_boot_flag;
    /* Byte [13]: App hiện tại là candidate chưa confirm. */

    uint8_t boot_attempt_count;
    /* Byte [14]: số lần đã cho candidate Trial Boot. */

    uint8_t reserved;
    /* Byte [15]: phải bằng 0. */

    uint64_t operation_id;
    /* Byte [16..23]: liên kết transaction với W25 manifest. */

    Firmware_SignedHeader_t active_header;
    /* Byte [24..55]: mô tả App trong Internal Flash. */

    uint8_t active_signature[FIRMWARE_SIGNATURE_SIZE];
    /* Byte [56..119]: bằng chứng chữ ký; được xác thực ở Phase 3. */

    uint32_t metadata_crc32;
    /* Byte [120..123]: CRC32 của byte [0..119]. */

    uint32_t commit_marker;
    /* Byte [124..127]: được ghi cuối để publish record. */
} Metadata_Record_t;

#define METADATA_RECORD_SIZE            ((uint32_t)sizeof(Metadata_Record_t))

#define METADATA_CRC_REGION_OFFSET      ((uint32_t)offsetof(Metadata_Record_t, magic))

#define METADATA_CRC_REGION_SIZE        ((uint32_t)offsetof(Metadata_Record_t, metadata_crc32))

#define METADATA_COMMIT_MARKER_OFFSET   ((uint32_t)offsetof(Metadata_Record_t, commit_marker))

_Static_assert(sizeof(uint8_t) == 1U,
               "uint8_t phải có đúng 1 byte");

_Static_assert(sizeof(uint32_t) == 4U,
               "uint32_t phải có đúng 4 byte");

_Static_assert(sizeof(uint64_t) == 8U,
               "uint64_t phải có đúng 8 byte");

_Static_assert(
    offsetof(Metadata_Record_t, operation_state) == 12U,
    "Sai vị trí operation_state"
);

_Static_assert(
    offsetof(Metadata_Record_t, operation_id) == 16U,
    "Sai vị trí operation_id"
);

_Static_assert(
    offsetof(Metadata_Record_t, active_header) == 24U,
    "Sai vị trí active_header"
);

_Static_assert(
    offsetof(Metadata_Record_t, active_signature) == 56U,
    "Sai vị trí active_signature"
);

_Static_assert(
    offsetof(Metadata_Record_t, metadata_crc32) == 120U,
    "Sai vị trí metadata_crc32"
);

_Static_assert(
    offsetof(Metadata_Record_t, commit_marker) == 124U,
    "Sai vị trí commit_marker"
);

_Static_assert(
    sizeof(Metadata_Record_t) == 128U,
    "Metadata record phải giữ đúng 128 byte"
);

_Static_assert(
    (sizeof(Metadata_Record_t) % sizeof(uint16_t)) == 0U,
    "Metadata phải hỗ trợ program half-word"
);

_Static_assert(
    sizeof(Metadata_Record_t) <= METADATA_PAGE_SIZE,
    "Metadata phải nằm gọn trong một Flash page"
);

#endif /* AEGIS_METADATA_STRUCT_H */
