#ifndef AEGIS_ARTIFACT_STORAGE_H
#define AEGIS_ARTIFACT_STORAGE_H

/*
 * Lớp quản lý vùng Staging và Backup trên W25Q32.
 *
 * Module chịu trách nhiệm:
 * - Ánh xạ offset tương đối vào Staging Image.
 * - Chuẩn bị manifest và erase image cần thiết.
 * - Ghi fragment, read-back và commit progress.
 * - Phát hiện duplicate/conflicting duplicate.
 * - Publish artifact-valid marker sau khi caller đã xác thực.
 * - Copy App hiện tại sang Backup, read-back và commit manifest cuối cùng.
 *
 * Module không:
 * - Nhận UART/LoRa frame.
 * - Tính hoặc verify Ed25519ph.
 * - Quyết định install firmware.
 */
#include "artifact_manifest.h"

#include <stdint.h>

typedef enum{
    ARTIFACT_STORAGE_OK = 0U,
    /* Thao tác hoàn thành thành công. */

    ARTIFACT_STORAGE_DUPLICATE,
    /* Fragment đã tồn tại và dữ liệu hoàn toàn giống nhau. */

    ARTIFACT_STORAGE_INVALID_ARGUMENT,
    /* Con trỏ NULL hoặc field đầu vào sai. */

    ARTIFACT_STORAGE_OUT_OF_RANGE,
    /* Offset/length vượt khỏi Staging Image. */

    ARTIFACT_STORAGE_BAD_STATE,
    /* Manifest chưa commit, bị hỏng hoặc artifact đã publish. */

    ARTIFACT_STORAGE_CONFLICTING_DUPLICATE,
    /* Fragment đã commit nhưng dữ liệu gửi lại không khớp. */

    ARTIFACT_STORAGE_INCOMPLETE,
    /* Vẫn còn fragment chưa được ghi. */

    ARTIFACT_STORAGE_FLASH_ERROR,
    /* Driver W25Q32 báo lỗi SPI/BUSY/program/erase. */

    ARTIFACT_STORAGE_VERIFY_FAILED
    /* Dữ liệu read-back không khớp dữ liệu đã ghi. */
} ArtifactStorage_Status_t;

/*
 * Bắt đầu một transaction Staging mới.
 *
 * Hàm:
 * - Erase sector manifest.
 * - Erase đủ số sector image.
 * - Chuẩn hóa manifest.
 * - Ghi header body.
 * - Ghi header commit marker cuối cùng.
 */
ArtifactStorage_Status_t ArtifactStorage_StagingBegin(
    const Staging_Manifest_t *manifest
);

/*
 * Ghi đúng một fragment.
 *
 * Fragment chỉ được đánh dấu hoàn thành sau program + read-back thành công.
 */
ArtifactStorage_Status_t ArtifactStorage_StagingWriteFragment(
    uint32_t offset,
    const uint8_t *data,
    uint16_t length
);

/*
 * Đọc Staging Manifest hiện tại.
 */
ArtifactStorage_Status_t ArtifactStorage_StagingReadManifest(
    Staging_Manifest_t *manifest
);

/*
 * Đọc image bằng offset tương đối trong Staging Image.
 */
ArtifactStorage_Status_t ArtifactStorage_StagingRead(
    uint32_t offset,
    uint8_t *data,
    uint32_t length
);

/*
 * So sánh dữ liệu trong Staging với buffer mong đợi.
 */
ArtifactStorage_Status_t ArtifactStorage_StagingVerify(
    uint32_t offset,
    const uint8_t *expected,
    uint32_t length
);

/*
 * Ghi artifact-valid marker.
 *
 * Caller chỉ được gọi sau khi CRC/chữ ký đã được xác thực.
 */
ArtifactStorage_Status_t ArtifactStorage_StagingPublishValid(void);

/*
 * Invalidate Staging bằng cách erase sector manifest.
 */
ArtifactStorage_Status_t ArtifactStorage_StagingInvalidate(void);

// =================== API BACKUP ===================== //

/*
 * Tạo Backup của App đang chạy.
 *
 * Caller cung cấp exact SignedHeader/signature đang mô tả App hiện tại.
 * Hàm invalidate manifest trước, copy image, kiểm tra CRC, ghi manifest body
 * rồi mới program commit marker cuối cùng.
 */
ArtifactStorage_Status_t ArtifactStorage_BackupCreate(
    uint64_t operation_id,
    const Firmware_SignedHeader_t *active_header,
    const uint8_t active_signature[FIRMWARE_SIGNATURE_SIZE]
);

/* Đọc Backup Manifest hiện tại. */
ArtifactStorage_Status_t ArtifactStorage_BackupReadManifest(
    Backup_Manifest_t *manifest
);

/* Đọc Backup Image bằng offset tương đối. */
ArtifactStorage_Status_t ArtifactStorage_BackupRead(
    uint32_t offset,
    uint8_t *data,
    uint32_t length
);

/* So sánh một vùng Backup Image với buffer mong đợi. */
ArtifactStorage_Status_t ArtifactStorage_BackupVerify(
    uint32_t offset,
    const uint8_t *expected,
    uint32_t length
);

/*
 * Kiểm tra manifest đã commit và CRC32 của toàn Backup Image.
 * Hàm chưa verify Ed25519ph; bước crypto độc lập được nối ở Phase 3.
 */
ArtifactStorage_Status_t ArtifactStorage_BackupValidate(
    Backup_Manifest_t *manifest
);

/* Invalidate Backup bằng cách erase sector manifest. */
ArtifactStorage_Status_t ArtifactStorage_BackupInvalidate(void);

#endif /* AEGIS_ARTIFACT_STORAGE_H */
