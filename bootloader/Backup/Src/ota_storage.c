#include "ota_storage.h"

#include "crc32.h"
#include "ext_flash.h"
#include "memory_map.h"

#include <stddef.h>

#define OTA_STORAGE_IO_CHUNK_SIZE W25Q32_PAGE_SIZE

// Check vùng
static bool OtaStorage_IsRangeValid(uint32_t offset, uint32_t length, uint32_t region_size){
    if(offset > region_size){
        return false;
    }

    return length <= (region_size - offset);
}

// Ánh xạ trạng thái
// Chuyển đổi mã lỗi tầng thấp (Driver) lên cao (Module)
static OtaStorage_Status_t OtaStorage_MapExtFlashStatus(ExtFlash_Status_t status){
    if(status == EXT_FLASH_OK){
        return OTA_STORAGE_OK;
    }

    if(status == EXT_FLASH_VERIFY_FAILED){
        return OTA_STORAGE_VERIFY_FAILED;
    }

    return OTA_STORAGE_EXT_FLASH_ERROR;
}

// Xóa 1 khoảng sector cần thiết
static OtaStorage_Status_t OtaStorage_EraseRegion(uint32_t base, uint32_t length){
    // Đếm số sector cần xóa
    const uint32_t sector_count = (length + W25Q32_SECTOR_SIZE - 1U) / W25Q32_SECTOR_SIZE;

    for(uint32_t sector = 0U; sector < sector_count; ++sector){
        const uint32_t address = base + (sector * W25Q32_SECTOR_SIZE);
        const ExtFlash_Status_t status = W25Q32_SectorErase(address);

        if(status != EXT_FLASH_OK){
            return OtaStorage_MapExtFlashStatus(status);
        }
    }

    return OTA_STORAGE_OK;
}

// Tính toán mã kiểm CRC32 toàn bộ Firm đang nằm ngoài chip flash W25Q32
static OtaStorage_Status_t OtaStorage_CalculateExternalCrc(uint32_t address, uint32_t length, uint32_t *crc32){
    /*
    - Quy trình diễn ra lặp lại
    + Đọc 64 bytes
    + Nhồi vào bộ tính CRC
    + Đọc 64 bytes tiếp
    */
    uint8_t buffer[OTA_STORAGE_IO_CHUNK_SIZE];
    uint32_t remaining = length;
    uint32_t current_address = address;
    uint32_t running_crc = 0U;

    if(crc32 == NULL){
        return OTA_STORAGE_INVALID_ARGUMENT;
    }

    while(remaining > 0U){
        const uint32_t chunk = (remaining < sizeof(buffer)) ? remaining : (uint32_t)sizeof(buffer);
        const ExtFlash_Status_t status = W25Q32_ReadBlock(current_address, buffer, chunk);

        if(status != EXT_FLASH_OK){
            return OtaStorage_MapExtFlashStatus(status);
        }

        running_crc = Crc32_Update(running_crc, buffer, chunk);
        current_address += chunk;
        remaining -= chunk;
    }

    *crc32 = running_crc;
    return OTA_STORAGE_OK;
}

// ======================== API chính ======================== //

OtaStorage_Status_t OtaStorage_StagingPrepare(uint32_t image_size){
    if(image_size == 0U){
        return OTA_STORAGE_INVALID_ARGUMENT;
    }

    if((image_size > APPLICATION_SIZE) ||
       (image_size > W25Q32_STAGING_IMAGE_SIZE)){
        return OTA_STORAGE_OUT_OF_RANGE;
    }

    return OtaStorage_EraseRegion(W25Q32_STAGING_IMAGE_BASE, image_size);
}

OtaStorage_Status_t OtaStorage_StagingWrite(uint32_t offset, const uint8_t *data, uint32_t length){
    if(!OtaStorage_IsRangeValid(offset, length, W25Q32_STAGING_IMAGE_SIZE)){
        return OTA_STORAGE_OUT_OF_RANGE;
    }

    if(length == 0U){
        return OTA_STORAGE_OK;
    }

    if(data == NULL){
        return OTA_STORAGE_INVALID_ARGUMENT;
    }

    ExtFlash_Status_t status = W25Q32_WriteBlock(W25Q32_STAGING_IMAGE_BASE + offset, data, length);
    if(status != EXT_FLASH_OK){
        return OtaStorage_MapExtFlashStatus(status);
    }

    status = W25Q32_VerifyBlock(W25Q32_STAGING_IMAGE_BASE + offset, data, length);
    return OtaStorage_MapExtFlashStatus(status);
}

OtaStorage_Status_t OtaStorage_StagingRead(uint32_t offset, uint8_t *data, uint32_t length){
    if(!OtaStorage_IsRangeValid(offset, length, W25Q32_STAGING_IMAGE_SIZE)){
        return OTA_STORAGE_OUT_OF_RANGE;
    }

    if(length == 0U){
        return OTA_STORAGE_OK;
    }

    if(data == NULL){
        return OTA_STORAGE_INVALID_ARGUMENT;
    }

    ExtFlash_Status_t status = W25Q32_ReadBlock(W25Q32_STAGING_IMAGE_BASE + offset, data, length);
    return OtaStorage_MapExtFlashStatus(status);
}

OtaStorage_Status_t OtaStorage_StagingVerify(uint32_t offset, const uint8_t *expected_data, uint32_t length){
    if(!OtaStorage_IsRangeValid(offset, length, W25Q32_STAGING_IMAGE_SIZE)){
        return OTA_STORAGE_OUT_OF_RANGE;
    }

    if(length == 0U){
        return OTA_STORAGE_OK;
    }

    if(expected_data == NULL){
        return OTA_STORAGE_INVALID_ARGUMENT;
    }

    ExtFlash_Status_t status = W25Q32_VerifyBlock(W25Q32_STAGING_IMAGE_BASE + offset, expected_data, length);
    return OtaStorage_MapExtFlashStatus(status);
}

// ====================== API cho MAINFEST ====================== //

uint32_t OtaStorage_BackupManifestCalculateCrc(
    const OtaStorage_BackupManifest_t *manifest){
    if(manifest == NULL){
        return 0U;
    }

    return Crc32_Compute(manifest, OTA_STORAGE_BACKUP_CRC_SIZE);
}

bool OtaStorage_BackupManifestRefreshCrc(OtaStorage_BackupManifest_t *manifest){
    if(manifest == NULL){
        return false;
    }

    manifest->manifest_crc32 = OtaStorage_BackupManifestCalculateCrc(manifest);
    return true;
}

bool OtaStorage_BackupManifestIsValid(const OtaStorage_BackupManifest_t *manifest){
    if(manifest == NULL){
        return false;
    }

    if((manifest->magic != OTA_STORAGE_BACKUP_MAGIC_VALUE) ||
       (manifest->format_version != OTA_STORAGE_BACKUP_FORMAT_VERSION) ||
       (manifest->commit_marker != OTA_STORAGE_BACKUP_COMMIT_MARKER_VALUE) ||
       (manifest->operation_id == UINT64_C(0))
    ){
        return false;
    }

    if((manifest->image_header.image_size == 0U) ||
       (manifest->image_header.image_size > APPLICATION_SIZE) ||
       (manifest->image_header.image_size > W25Q32_BACKUP_IMAGE_SIZE)
    ){
        return false;
    }

    return manifest->manifest_crc32 == OtaStorage_BackupManifestCalculateCrc(manifest);
}

OtaStorage_Status_t OtaStorage_BackupInvalidate(void){
    return OtaStorage_MapExtFlashStatus(W25Q32_SectorErase(W25Q32_BACKUP_MANIFEST_BASE));
}

OtaStorage_Status_t OtaStorage_BackupCreate(
    const OtaStorage_BackupManifest_t *manifest,
    const uint8_t *image_data
){
    if((manifest == NULL) || (image_data == NULL) ||
       (manifest->image_header.image_size == 0U)){
        return OTA_STORAGE_INVALID_ARGUMENT;
    }

    if((manifest->image_header.image_size > APPLICATION_SIZE) ||
       (manifest->image_header.image_size > W25Q32_BACKUP_IMAGE_SIZE)){
        return OTA_STORAGE_OUT_OF_RANGE;
    }

    // 1. Tạo bản sao (pending) để không thao tác trực tiếp trên manifest
    // Tránh thao tác trực tiếp gặp lỗi hoặc mất điện gây thất thoát
    OtaStorage_BackupManifest_t pending = *manifest;
    pending.magic = OTA_STORAGE_BACKUP_MAGIC_VALUE;
    pending.format_version = OTA_STORAGE_BACKUP_FORMAT_VERSION;
    pending.commit_marker = OTA_STORAGE_BACKUP_COMMIT_MARKER_VALUE;
    (void)OtaStorage_BackupManifestRefreshCrc(&pending);

    // 2.
    if(Crc32_Compute(image_data, pending.image_header.image_size) !=
       pending.image_header.image_crc32){
        return OTA_STORAGE_CRC_MISMATCH;
    }

    // 3.
    OtaStorage_Status_t storage_status = OtaStorage_BackupInvalidate();
    if(storage_status != OTA_STORAGE_OK){
        return storage_status;
    }

    // 4.
    storage_status = OtaStorage_EraseRegion(W25Q32_BACKUP_IMAGE_BASE,
                                            pending.image_header.image_size);
    if(storage_status != OTA_STORAGE_OK){
        return storage_status;
    }

    // 5.
    uint32_t offset = 0U;
    while(offset < pending.image_header.image_size){
        const uint32_t remaining = pending.image_header.image_size - offset;
        const uint32_t chunk = (remaining < OTA_STORAGE_IO_CHUNK_SIZE) ? remaining : OTA_STORAGE_IO_CHUNK_SIZE;
        ExtFlash_Status_t status = 
        W25Q32_WriteBlock(W25Q32_BACKUP_IMAGE_BASE + offset, &image_data[offset], chunk);

        if(status != EXT_FLASH_OK){
            return OtaStorage_MapExtFlashStatus(status);
        }

        status = W25Q32_VerifyBlock(W25Q32_BACKUP_IMAGE_BASE + offset, &image_data[offset], chunk);
        if(status != EXT_FLASH_OK){
            return OtaStorage_MapExtFlashStatus(status);
        }

        offset += chunk;
    }

    // 6.
    uint32_t copied_crc = 0U;
    storage_status = OtaStorage_CalculateExternalCrc(W25Q32_BACKUP_IMAGE_BASE,
                                                     pending.image_header.image_size,
                                                     &copied_crc);

    if(storage_status != OTA_STORAGE_OK){
        return storage_status;
    }

    if(copied_crc != pending.image_header.image_crc32){
        return OTA_STORAGE_CRC_MISMATCH;
    }

    // 7.
    // 7.1 Ghi manifest body (trừ commit marker)
    ExtFlash_Status_t status = W25Q32_WriteBlock(
        W25Q32_BACKUP_MANIFEST_BASE, 
        (const uint8_t *)&pending, 
        OTA_STORAGE_BACKUP_COMMIT_OFFSET
    );
    if(status != EXT_FLASH_OK){
        return OtaStorage_MapExtFlashStatus(status);
    }

    // 7.2 Kiểm lại
    status = W25Q32_VerifyBlock(
        W25Q32_BACKUP_MANIFEST_BASE,
        (const uint8_t *)&pending,
        OTA_STORAGE_BACKUP_COMMIT_OFFSET
    );
    if(status != EXT_FLASH_OK){
        return OtaStorage_MapExtFlashStatus(status);
    }

    // 7.3 Ghi 4 byte commit marker
    status = W25Q32_WriteBlock(
        W25Q32_BACKUP_MANIFEST_BASE + OTA_STORAGE_BACKUP_COMMIT_OFFSET,
        (const uint8_t *)&pending.commit_marker,
        (uint32_t)sizeof(pending.commit_marker)
    );
    if(status != EXT_FLASH_OK){
        return OtaStorage_MapExtFlashStatus(status);
    }

    // 7.4 Soi lại toàn bộ manifest
    status = W25Q32_VerifyBlock(
        W25Q32_BACKUP_MANIFEST_BASE,
        (const uint8_t *)&pending,
        OTA_STORAGE_BACKUP_MANIFEST_SIZE
    );
    if(status != EXT_FLASH_OK){
        return OtaStorage_MapExtFlashStatus(status);
    }

    // 7.5 Dùng lại hàm ReadManifest để thẩm định lại lần cuối
    OtaStorage_BackupManifest_t stored;
    storage_status = OtaStorage_BackupReadManifest(&stored);
    if(storage_status != OTA_STORAGE_OK){
        return storage_status;
    }

    return OTA_STORAGE_OK;
}

OtaStorage_Status_t OtaStorage_BackupReadManifest(
    OtaStorage_BackupManifest_t *manifest){
    if(manifest == NULL){
        return OTA_STORAGE_INVALID_ARGUMENT;
    }

    const ExtFlash_Status_t status = W25Q32_ReadBlock(
        W25Q32_BACKUP_MANIFEST_BASE,
        (uint8_t *)manifest,
        OTA_STORAGE_BACKUP_MANIFEST_SIZE
    );
    if(status != EXT_FLASH_OK){
        return OtaStorage_MapExtFlashStatus(status);
    }

    if(!OtaStorage_BackupManifestIsValid(manifest)){
        return OTA_STORAGE_MANIFEST_INVALID;
    }

    return OTA_STORAGE_OK;
}

OtaStorage_Status_t OtaStorage_BackupValidate(
    OtaStorage_BackupManifest_t *manifest){
    OtaStorage_Status_t status = OtaStorage_BackupReadManifest(manifest);
    if(status != OTA_STORAGE_OK){
        return status;
    }

    uint32_t image_crc = 0U;
    status = OtaStorage_CalculateExternalCrc(W25Q32_BACKUP_IMAGE_BASE,
                                             manifest->image_header.image_size,
                                             &image_crc);
    if(status != OTA_STORAGE_OK){
        return status;
    }

    if(image_crc != manifest->image_header.image_crc32){
        return OTA_STORAGE_CRC_MISMATCH;
    }

    return OTA_STORAGE_OK;
}

OtaStorage_Status_t OtaStorage_BackupRead(uint32_t offset, uint8_t *data, uint32_t length){
    if(!OtaStorage_IsRangeValid(offset, length, W25Q32_BACKUP_IMAGE_SIZE)){
        return OTA_STORAGE_OUT_OF_RANGE;
    }

    if(length == 0U){
        return OTA_STORAGE_OK;
    }

    if(data == NULL){
        return OTA_STORAGE_INVALID_ARGUMENT;
    }

    ExtFlash_Status_t status = W25Q32_ReadBlock(W25Q32_BACKUP_IMAGE_BASE + offset, data, length);
    return OtaStorage_MapExtFlashStatus(status);
}

OtaStorage_Status_t OtaStorage_BackupVerify(uint32_t offset, const uint8_t *expected_data, uint32_t length){
    if(!OtaStorage_IsRangeValid(offset, length, W25Q32_BACKUP_IMAGE_SIZE)){
        return OTA_STORAGE_OUT_OF_RANGE;
    }

    if(length == 0U){
        return OTA_STORAGE_OK;
    }

    if(expected_data == NULL){
        return OTA_STORAGE_INVALID_ARGUMENT;
    }

    ExtFlash_Status_t status = W25Q32_VerifyBlock(W25Q32_BACKUP_IMAGE_BASE + offset, expected_data, length);
    return OtaStorage_MapExtFlashStatus(status);
}
