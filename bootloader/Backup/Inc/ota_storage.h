#ifndef AEGIS_OTA_STORAGE_H
#define AEGIS_OTA_STORAGE_H

/* Storage cấp cao cho Staging Area và Backup Slot trên W25Q32. */

#include "metadata_struct.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OTA_STORAGE_BACKUP_MAGIC_VALUE          0x424B5550UL
#define OTA_STORAGE_BACKUP_FORMAT_VERSION       2UL
#define OTA_STORAGE_BACKUP_COMMIT_MARKER_VALUE  0x434D4954UL
#define OTA_STORAGE_ERASED_WORD                 0xFFFFFFFFUL

typedef enum {
    OTA_STORAGE_OK = 0,                 /* Thao tác storage hoàn tất thành công. */
    OTA_STORAGE_INVALID_ARGUMENT,       /* Con trỏ NULL hoặc image size bằng 0. */
    OTA_STORAGE_OUT_OF_RANGE,           /* Offset/length vượt vùng storage sở hữu. */
    OTA_STORAGE_EXT_FLASH_ERROR,        /* Driver W25Q32 trả lỗi giao tiếp/thao tác. */
    OTA_STORAGE_VERIFY_FAILED,          /* Dữ liệu đọc lại khác dữ liệu đã ghi. */
    OTA_STORAGE_CRC_MISMATCH,           /* CRC image không khớp manifest. */
    OTA_STORAGE_MANIFEST_INVALID        /* Manifest sai format, CRC hoặc commit marker. */
} OtaStorage_Status_t;

/*
 * Manifest nằm đầu Backup Slot và chỉ hợp lệ sau khi commit_marker được ghi.
 * CRC coverage là [magic, manifest_crc32); commit marker không thuộc CRC.
 */
typedef struct __attribute__((packed)){
    uint32_t magic;                              /* Mã nhận diện backup manifest. */
    uint32_t format_version;                     /* Phiên bản layout manifest. */
    uint64_t operation_id;                       /* Bind Backup với đúng update transaction. */
    Crypto_SignedHeader_t image_header;          /* SignedHeader chính xác của App cũ. */
    uint8_t image_signature[CRYPTO_ED25519_SIGNATURE_SIZE]; /* Chữ ký App cũ. */
    uint32_t manifest_crc32;                     /* CRC32 bảo vệ body manifest. */
    uint32_t commit_marker;                      /* Ghi cuối cùng để publish backup. */
} OtaStorage_BackupManifest_t;

#define OTA_STORAGE_BACKUP_MANIFEST_SIZE ((uint32_t)sizeof(OtaStorage_BackupManifest_t))
#define OTA_STORAGE_BACKUP_CRC_SIZE      ((uint32_t)offsetof(OtaStorage_BackupManifest_t, manifest_crc32))
#define OTA_STORAGE_BACKUP_COMMIT_OFFSET ((uint32_t)offsetof(OtaStorage_BackupManifest_t, commit_marker))

_Static_assert(offsetof(OtaStorage_BackupManifest_t, operation_id) == 8U,
               "Unexpected backup operation ID offset");
_Static_assert(offsetof(OtaStorage_BackupManifest_t, image_header) == 16U,
               "Unexpected backup image header offset");
_Static_assert(offsetof(OtaStorage_BackupManifest_t, image_signature) == 48U,
               "Unexpected backup signature offset");
_Static_assert(offsetof(OtaStorage_BackupManifest_t, manifest_crc32) == 112U,
               "Unexpected backup manifest CRC offset");
_Static_assert(offsetof(OtaStorage_BackupManifest_t, commit_marker) == 116U,
               "Unexpected backup commit marker offset");
_Static_assert(sizeof(OtaStorage_BackupManifest_t) == 120U,
               "Backup manifest layout must remain exactly 120 bytes");
_Static_assert(sizeof(OtaStorage_BackupManifest_t) <= W25Q32_BACKUP_MANIFEST_SIZE,
               "Backup manifest must fit in its dedicated sector");

// 2.3
/* Erase đúng số sector staging cần để chứa image_size. */
OtaStorage_Status_t OtaStorage_StagingPrepare(uint32_t image_size);

/* Các offset staging là tương đối; API không thể đi sang Backup Slot. */
// Cộng offset vs staging address rồi nhảy vào write block
OtaStorage_Status_t OtaStorage_StagingWrite(uint32_t offset, const uint8_t *data, uint32_t length);
// Đọc data ra RAM để sau này tính toán SHA-512
OtaStorage_Status_t OtaStorage_StagingRead(uint32_t offset, uint8_t *data, uint32_t length);
// Đối chiếu dữ liệu
OtaStorage_Status_t OtaStorage_StagingVerify(uint32_t offset, const uint8_t *expected_data, uint32_t length);

// 2.4
/* Tính và kiểm tra CRC/format/marker của một manifest đã nằm trong RAM. */
uint32_t OtaStorage_BackupManifestCalculateCrc(const OtaStorage_BackupManifest_t *manifest);
bool OtaStorage_BackupManifestRefreshCrc(OtaStorage_BackupManifest_t *manifest);
bool OtaStorage_BackupManifestIsValid(const OtaStorage_BackupManifest_t *manifest);

/* Erase riêng sector manifest để backup hiện tại lập tức mất hiệu lực. */
OtaStorage_Status_t OtaStorage_BackupInvalidate(void);

/*
 * Tạo backup từ image_data: 
 * 1. Dọn Ram, ép lại magic, version, tính CRC cho pending
 * 2. Kiểm nguồn: Tự tính CRC của image_data (file app gốc) khớp k
 * 3. Xóa manifest cũ (OtaStorage_BackupInvalidate)
 * 4. Xóa vùng backup image (OtaStorage_EraseRegion)
 * 5. Chuyển đồ: Chia file App thành các chunk, vừa ghi vừa verify block
 * 6. Kiểm: Đọc lại toàn bộ để tính CRC
 * 7. Chốt: Ghi body -> commit marker cuối -> verify -> soi lại bằng IsValid
 */
OtaStorage_Status_t OtaStorage_BackupCreate(
    const OtaStorage_BackupManifest_t *manifest,
    const uint8_t *image_data
);

/* Đọc manifest đã commit và kiểm tra cả CRC của Backup Image. */
// Đọc và check đúng manifest struct, nhanh và xem có bản nào tồn tại không.
OtaStorage_Status_t OtaStorage_BackupReadManifest(OtaStorage_BackupManifest_t *manifest);
// Gọi lại hàm trên, nếu ok sẽ quét chục KB tính CRC, dùng trc khi quyết định Rollback
// Đảm bảo bản backup này ko bị lỗi dù thời gian tồn tại đủ lâu
OtaStorage_Status_t OtaStorage_BackupValidate(OtaStorage_BackupManifest_t *manifest);

/* Các offset backup là tương đối với W25Q32_BACKUP_IMAGE_BASE. */
// Cung cấp API để Rollback bốc data từ W25Q32 vào nạp ngược lại App
OtaStorage_Status_t OtaStorage_BackupRead(uint32_t offset, uint8_t *data, uint32_t length);
OtaStorage_Status_t OtaStorage_BackupVerify(uint32_t offset, const uint8_t *expected_data, uint32_t length);

#endif /* AEGIS_OTA_STORAGE_H */
