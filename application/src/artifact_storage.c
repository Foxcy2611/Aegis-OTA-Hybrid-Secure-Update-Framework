#include "artifact_storage.h"

#include "crc32.h"
#include "ext_flash.h"
#include "memory_map.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* ==================== Helper private ==================== */

/*
 * Kiểm tra range tương đối [offset, offset + length) có nằm trọn trong region.
 * Dùng phép trừ để tránh overflow khi cộng offset + length.
 */
static bool ArtifactStorage_IsRangeValid(
    uint32_t offset, 
    uint32_t length,
    uint32_t region_size
){
    if(offset > region_size){
        return false;
    }

    return length <= (region_size - offset);
}

/*
 * Kiểm tra toàn bộ buffer còn ở trạng thái erased 0xFF.
 * Dùng để xác nhận các byte reserved của manifest chưa bị program.
 */
static bool ArtifactStorage_IsErased(
    const uint8_t *data,
    uint32_t length
){
    for(uint32_t i = 0U; i < length; ++i){
        if(data[i] != 0xFFU){
            return false;
        }
    }

    return true;
}

/*
 * Kiểm tra ID không phải chuỗi toàn byte 0.
 * Operation/transfer/artifact ID bằng 0 được xem là chưa được cấp hợp lệ.
 */
static bool ArtifactStorage_IdIsNonzero(
    const uint8_t *id,
    uint32_t length
){
    uint8_t result = 0U;

    for(uint32_t i = 0U; i < length; ++i){
        result |= id[i];
    }

    return result != 0U;
}

/*
 * Chuyển status tầng driver W25Q32 sang status của tầng artifact storage.
 * Nhờ đó caller không phải phụ thuộc trực tiếp vào lỗi SPI/Page Program.
 */
static ArtifactStorage_Status_t ArtifactStorage_MapFlashStatus(
    ExtFlash_Status_t status
){
    switch(status){
        case EXT_FLASH_OK:
            return ARTIFACT_STORAGE_OK;

        case EXT_FLASH_INVALID_ARGUMENT:
            return ARTIFACT_STORAGE_INVALID_ARGUMENT;

        case EXT_FLASH_OUT_OF_RANGE:
        case EXT_FLASH_UNALIGNED:
        case EXT_FLASH_PAGE_BOUNDARY:
            return ARTIFACT_STORAGE_OUT_OF_RANGE;

        case EXT_FLASH_VERIFY_FAILED:
            return ARTIFACT_STORAGE_VERIFY_FAILED;

        case EXT_FLASH_SPI_TIMEOUT:
        case EXT_FLASH_BUSY_TIMEOUT:
        case EXT_FLASH_WRITE_ENABLE_FAILED:
        case EXT_FLASH_JEDEC_ID_MISMATCH:
        default:
            return ARTIFACT_STORAGE_FLASH_ERROR;
    }
}

/*
 * Erase đủ số sector 4 KB bao phủ length byte tính từ base_address.
 * Dừng ngay và trả lỗi đầu tiên nếu bất kỳ sector nào erase thất bại.
 */
static ArtifactStorage_Status_t ArtifactStorage_EraseRegion(
    uint32_t base_address,
    uint32_t length
){
    if(length == 0U){
        return ARTIFACT_STORAGE_OK;
    }

    const uint32_t sector_count = (length + W25Q32_SECTOR_SIZE - 1U) / W25Q32_SECTOR_SIZE;

    for(uint32_t sector = 0U ; sector < sector_count ; sector++){
        const uint32_t sector_address = base_address + (sector * W25Q32_SECTOR_SIZE);

        const ExtFlash_Status_t flash_status = W25Q32_SectorErase(sector_address);
        if(flash_status != EXT_FLASH_OK){
            return ArtifactStorage_MapFlashStatus(flash_status);
        }
    }

    return ARTIFACT_STORAGE_OK;
}

/*
 * Ghi một block rồi đọc lại để verify byte-for-byte.
 * Chỉ trả OK khi cả WriteBlock và VerifyBlock đều thành công.
 */
static ArtifactStorage_Status_t ArtifactStorage_WriteVerified(
    uint32_t address,
    const uint8_t *data,
    uint32_t length
){
    ExtFlash_Status_t flash_status = W25Q32_WriteBlock(address, data, length);

    if(flash_status != EXT_FLASH_OK){
        return ArtifactStorage_MapFlashStatus(flash_status);
    }

    flash_status = W25Q32_VerifyBlock(address, data, length);

    return ArtifactStorage_MapFlashStatus(flash_status);
}

/*
 * Tính CRC32 phần header manifest từ byte 0 đến ngay trước header_crc32.
 * CRC phát hiện dữ liệu hỏng; nó không thay thế chữ ký firmware.
 */
static uint32_t ArtifactStorage_CalculateHeaderCrc(
    const Staging_Manifest_t *manifest
){
    return Crc32_Compute(
        manifest,
        STAGING_MANIFEST_HEADER_CRC_SIZE
    );
}

/*
 * Kiểm tra tính hợp lệ cấu trúc của Staging Manifest header:
 * magic/format/ID/range/chunk/count/reserved/commit marker và CRC.
 * Hàm không verify image bytes hoặc chữ ký Ed25519ph.
 */
static bool ArtifactStorage_ManifestHeaderIsValid(
    const Staging_Manifest_t *manifest
){
    if(manifest == NULL){
        return false;
    }

    if(manifest->magic != STAGING_MANIFEST_MAGIC_VALUE){
        return false;
    }

    if(manifest->format_version != STAGING_MANIFEST_FORMAT_VERSION){
        return false;
    }

    if(manifest->artifact_type != STAGING_ARTIFACT_TYPE_FULL){
        return false;
    }

    if(manifest->reserved0 != 0U || manifest->reserved1 != 0U){
        return false;
    }

    if((manifest->operation_id == UINT64_C(0)) ||
       (manifest->transfer_id == UINT64_C(0))){
        return false;
    }

    if(!ArtifactStorage_IdIsNonzero(
        manifest->artifact_id,
        sizeof(manifest->artifact_id)
    )){
        return false;
    }

    if((manifest->chunk_size == 0U) ||
       (manifest->chunk_size > STAGING_DATA_CHUNK_MAX_SIZE)){
        return false;
    }

    if((manifest->data_size == 0U) ||
       (manifest->data_size > W25Q32_STAGING_IMAGE_SIZE)){
        return false;
    }

    if(manifest->data_size != manifest->image_header.image_size){
        return false;
    }

    /*
     * Firmware cuối cùng phải vừa Application slot 46 KB.
     */
    if(manifest->image_header.image_size > APPLICATION_SIZE){
        return false;
    }

    const uint32_t expected_fragment_count =
    (manifest->data_size + manifest->chunk_size - 1U) / manifest->chunk_size;

    if((expected_fragment_count == 0U) ||
       (expected_fragment_count > STAGING_PROGRESS_FRAGMENT_MAX)){
        return false;
    }

    if(manifest->fragment_count != expected_fragment_count){
        return false;
    }

    if(manifest->header_commit_marker != STAGING_HEADER_COMMIT_MARKER_VALUE){
        return false;
    }

    if((manifest->artifact_valid_marker != ARTIFACT_MANIFEST_ERASED_WORD) &&
       (manifest->artifact_valid_marker !=
            STAGING_ARTIFACT_VALID_MARKER_VALUE)){
        return false;
    }

    if(!ArtifactStorage_IsErased(
        manifest->reserved_erased,
        sizeof(manifest->reserved_erased)
    )){
        return false;
    }

    return manifest->header_crc32 == ArtifactStorage_CalculateHeaderCrc(manifest);
}

/*
 * Kiểm tra header hợp lệ và mọi fragment bắt buộc đã được commit.
 * Complete chưa đồng nghĩa artifact trusted hoặc đã có valid marker.
 */
static bool ArtifactStorage_ManifestIsComplete(
    const Staging_Manifest_t *manifest
){
    if(!ArtifactStorage_ManifestHeaderIsValid(manifest)){
        return false;
    }

    for(uint16_t fragment = 0U; fragment < manifest->fragment_count; ++fragment){

        const uint8_t mask = (uint8_t)(1U << (fragment & 7U));

        if((manifest->progress_bitmap[fragment >> 3U] & mask) != 0U){
            return false;
        }
    }

    return true;
}

/*
 * Kiểm tra hình dạng một DATA fragment theo manifest:
 * offset phải thẳng hàng chunk, length đúng và fragment cuối được phép ngắn.
 * Nếu hợp lệ, trả fragment index qua con trỏ fragment_index.
 */
static bool ArtifactStorage_FragmentIsValid(
    const Staging_Manifest_t *manifest,
    uint32_t offset,
    uint16_t length,
    uint16_t *fragment_index
){
    if((manifest == NULL) ||
       (fragment_index == NULL) ||
       (length == 0U)){
        return false;
    }

    if((offset % manifest->chunk_size) != 0U){
        return false;
    }

    if(!ArtifactStorage_IsRangeValid(
        offset,
        length,
        manifest->data_size
    )){
        return false;
    }

    const uint32_t remaining =  manifest->data_size - offset;

    const uint16_t expected_length =
    (remaining < manifest->chunk_size) ? (uint16_t)remaining : manifest->chunk_size;

    if(length != expected_length){
        return false;
    }

    const uint32_t index = offset / manifest->chunk_size;

    if((index >= manifest->fragment_count) || (index > UINT16_MAX)){
        return false;
    }

    *fragment_index = (uint16_t)index;
    return true;
}

/*
 * Clear progress bit của fragment từ 1 xuống 0 sau khi image đã read-back đúng.
 * Chỉ program đúng một byte bitmap và verify lại byte vừa cập nhật.
 */
static ArtifactStorage_Status_t ArtifactStorage_CommitProgressBit(
    uint16_t fragment_index,
    uint8_t current_bitmap_byte
){
    const uint8_t mask = (uint8_t)(1U << (fragment_index & 7U));

    const uint8_t new_bitmap_byte = current_bitmap_byte & (uint8_t)~mask;

    const uint32_t bitmap_address =
        W25Q32_STAGING_MANIFEST_BASE +
        STAGING_MANIFEST_BITMAP_OFFSET +
        (fragment_index >> 3U);

    return ArtifactStorage_WriteVerified(
        bitmap_address,
        &new_bitmap_byte,
        sizeof(new_bitmap_byte)
    );
}

/*
 * Kiểm tra phần SignedHeader cần thiết trước khi dùng nó để giới hạn copy.
 * Đây chỉ là kiểm tra format/range; chưa phải phép verify Ed25519ph.
 */
static bool ArtifactStorage_FirmwareHeaderIsStructurallyValid(
    const Firmware_SignedHeader_t *header
){
    if(header == NULL){
        return false;
    }

    if(memcmp(
        header->domain,
        FIRMWARE_DOMAIN_STRING,
        FIRMWARE_DOMAIN_SIZE
    ) != 0){
        return false;
    }

    if(header->format_version != FIRMWARE_FORMAT_VERSION){
        return false;
    }

    if((header->image_size == 0U) ||
       (header->image_size > APPLICATION_SIZE) ||
       (header->image_size > W25Q32_BACKUP_IMAGE_SIZE)){
        return false;
    }

    return true;
}

/*
 * Tính CRC32 streaming của một vùng W25Q32 mà không cấp buffer bằng image size.
 * Kết quả chỉ được trả khi mọi block đã đọc thành công.
 */
static ArtifactStorage_Status_t ArtifactStorage_CalculateFlashCrc(
    uint32_t address,
    uint32_t length,
    uint32_t *crc_result
){
    if(crc_result == NULL){
        return ARTIFACT_STORAGE_INVALID_ARGUMENT;
    }

    uint8_t buffer[W25Q32_PAGE_SIZE];
    uint32_t crc = CRC32_INITIAL_VALUE;
    uint32_t offset = 0U;

    while(offset < length){
        const uint32_t remaining = length - offset;
        const uint32_t chunk_size =
            (remaining < sizeof(buffer)) ? remaining : (uint32_t)sizeof(buffer);

        const ExtFlash_Status_t flash_status = W25Q32_ReadBlock(
            address + offset,
            buffer,
            chunk_size
        );

        if(flash_status != EXT_FLASH_OK){
            return ArtifactStorage_MapFlashStatus(flash_status);
        }

        crc = Crc32_Update(crc, buffer, chunk_size);
        offset += chunk_size;
    }

    *crc_result = crc;
    return ARTIFACT_STORAGE_OK;
}

/*
 * Kiểm tra byte contract của Backup Manifest đã commit.
 * Hàm chưa đọc Backup Image và chưa verify chữ ký Ed25519ph.
 */
static bool ArtifactStorage_BackupManifestIsValid(
    const Backup_Manifest_t *manifest
){
    if(manifest == NULL){
        return false;
    }

    if(manifest->magic != BACKUP_MANIFEST_MAGIC_VALUE){
        return false;
    }

    if(manifest->format_version != BACKUP_MANIFEST_FORMAT_VERSION){
        return false;
    }

    if(manifest->operation_id == UINT64_C(0)){
        return false;
    }

    if(!ArtifactStorage_FirmwareHeaderIsStructurallyValid(
        &manifest->image_header
    )){
        return false;
    }

    if(manifest->commit_marker != BACKUP_MANIFEST_COMMIT_MARKER_VALUE){
        return false;
    }

    return manifest->manifest_crc32 == Crc32_Compute(
        manifest,
        BACKUP_MANIFEST_CRC_SIZE
    );
}

/*
 * Copy raw App hiện tại sang Backup Image theo từng page-sized block.
 * Mỗi block phải program + read-back thành công trước khi chuyển sang block sau.
 */
static ArtifactStorage_Status_t ArtifactStorage_CopyApplicationToBackup(
    const Firmware_SignedHeader_t *active_header
){
    uint32_t offset = 0U;

    while(offset < active_header->image_size){
        const uint32_t remaining = active_header->image_size - offset;
        const uint32_t chunk_size =
            (remaining < W25Q32_PAGE_SIZE) ? remaining : W25Q32_PAGE_SIZE;

        const uint8_t *source = (const uint8_t *)(uintptr_t)(
            APPLICATION_BASE_ADDRESS + offset
        );

        const ArtifactStorage_Status_t status = ArtifactStorage_WriteVerified(
            W25Q32_BACKUP_IMAGE_BASE + offset,
            source,
            chunk_size
        );

        if(status != ARTIFACT_STORAGE_OK){
            return status;
        }

        offset += chunk_size;
    }

    return ARTIFACT_STORAGE_OK;
}

/* ==================== API public ==================== */

ArtifactStorage_Status_t ArtifactStorage_StagingInvalidate(void){
    const ExtFlash_Status_t flash_status =
        W25Q32_SectorErase(W25Q32_STAGING_MANIFEST_BASE);

    return ArtifactStorage_MapFlashStatus(flash_status);
}

ArtifactStorage_Status_t ArtifactStorage_StagingBegin(
    const Staging_Manifest_t *manifest
){
    if(manifest == NULL){
        return ARTIFACT_STORAGE_INVALID_ARGUMENT;
    }

    if((manifest->operation_id == UINT64_C(0)) ||
       (manifest->transfer_id == UINT64_C(0)) ||
       (manifest->chunk_size == 0U) ||
       (manifest->chunk_size > STAGING_DATA_CHUNK_MAX_SIZE) ||
       (manifest->image_header.image_size == 0U) ||
       (manifest->image_header.image_size >  APPLICATION_SIZE) ||
       !ArtifactStorage_IdIsNonzero(
            manifest->artifact_id,
            sizeof(manifest->artifact_id)
       )){
        return ARTIFACT_STORAGE_INVALID_ARGUMENT;
    }

    Staging_Manifest_t pending = *manifest;

    pending.magic = STAGING_MANIFEST_MAGIC_VALUE;

    pending.format_version = STAGING_MANIFEST_FORMAT_VERSION;

    pending.artifact_type = STAGING_ARTIFACT_TYPE_FULL;

    pending.reserved0 = 0U;
    pending.reserved1 = 0U;

    pending.data_size = pending.image_header.image_size;

    const uint32_t fragment_count =
    (pending.data_size + pending.chunk_size - 1U) / pending.chunk_size;

    if((fragment_count == 0U) ||
       (fragment_count > STAGING_PROGRESS_FRAGMENT_MAX)){
        
        return ARTIFACT_STORAGE_OUT_OF_RANGE;
    }

    pending.fragment_count = (uint16_t)fragment_count;

    pending.header_commit_marker = STAGING_HEADER_COMMIT_MARKER_VALUE;

    pending.artifact_valid_marker =ARTIFACT_MANIFEST_ERASED_WORD;

    memset(
        pending.reserved_erased,
        0xFF,
        sizeof(pending.reserved_erased)
    );

    memset(
        pending.progress_bitmap,
        0xFF,
        sizeof(pending.progress_bitmap)
    );

    pending.header_crc32 = ArtifactStorage_CalculateHeaderCrc(&pending);

    /*
     * Erase manifest trước để transaction mới chưa thể bị hiểu nhầm
     * là artifact cũ đã hoàn thành.
     */
    ArtifactStorage_Status_t status = ArtifactStorage_StagingInvalidate();

    if(status != ARTIFACT_STORAGE_OK){
        return status;
    }

    /*
     * Chỉ erase đúng số sector image cần dùng.
     */
    status = ArtifactStorage_EraseRegion(
        W25Q32_STAGING_IMAGE_BASE,
        pending.data_size
    );

    if(status != ARTIFACT_STORAGE_OK){
        return status;
    }

    /*
     * Ghi header body [0, header_commit_marker).
     */
    status = ArtifactStorage_WriteVerified(
        W25Q32_STAGING_MANIFEST_BASE,
        (const uint8_t *)&pending,
        STAGING_MANIFEST_HEADER_MARKER_OFFSET
    );

    if(status != ARTIFACT_STORAGE_OK){
        return status;
    }

    /*
     * Commit marker được ghi cuối để publish header.
     */
    const uint32_t commit_marker = STAGING_HEADER_COMMIT_MARKER_VALUE;

    return ArtifactStorage_WriteVerified(
        W25Q32_STAGING_MANIFEST_BASE + STAGING_MANIFEST_HEADER_MARKER_OFFSET,
        (const uint8_t *)&commit_marker,
        sizeof(commit_marker)
    );
}

ArtifactStorage_Status_t ArtifactStorage_StagingReadManifest(
    Staging_Manifest_t *manifest
){
    if(manifest == NULL){
        return ARTIFACT_STORAGE_INVALID_ARGUMENT;
    }

    const ExtFlash_Status_t flash_status = W25Q32_ReadBlock(
        W25Q32_STAGING_MANIFEST_BASE,
        (uint8_t *)manifest,
        sizeof(*manifest)
    );

    if(flash_status != EXT_FLASH_OK){
        return ArtifactStorage_MapFlashStatus(flash_status);
    }
    return ArtifactStorage_ManifestHeaderIsValid(manifest) ?
        ARTIFACT_STORAGE_OK : ARTIFACT_STORAGE_BAD_STATE;
}

ArtifactStorage_Status_t ArtifactStorage_StagingRead(
    uint32_t offset,
    uint8_t *data,
    uint32_t length
){
    if(length == 0U){
        return ARTIFACT_STORAGE_OK;
    }

    if(data == NULL){
        return ARTIFACT_STORAGE_INVALID_ARGUMENT;
    }

    if(!ArtifactStorage_IsRangeValid(
        offset,
        length,
        W25Q32_STAGING_IMAGE_SIZE
    )){
        return ARTIFACT_STORAGE_OUT_OF_RANGE;
    }

    const ExtFlash_Status_t flash_status = W25Q32_ReadBlock(
        W25Q32_STAGING_IMAGE_BASE + offset,
        data,
        length
    );

    return ArtifactStorage_MapFlashStatus(flash_status);
}

ArtifactStorage_Status_t ArtifactStorage_StagingVerify(
    uint32_t offset,
    const uint8_t *expected,
    uint32_t length
){
    if(length == 0U){
        return ARTIFACT_STORAGE_OK;
    }

    if(expected == NULL){
        return ARTIFACT_STORAGE_INVALID_ARGUMENT;
    }

    if(!ArtifactStorage_IsRangeValid(
        offset, length,
        W25Q32_STAGING_IMAGE_SIZE
    )){
        return ARTIFACT_STORAGE_OUT_OF_RANGE;
    }

    const ExtFlash_Status_t flash_status = W25Q32_VerifyBlock(
        W25Q32_STAGING_IMAGE_BASE + offset,
        expected,
        length
    );

    return ArtifactStorage_MapFlashStatus(flash_status);
}

ArtifactStorage_Status_t ArtifactStorage_StagingWriteFragment(
    uint32_t offset,
    const uint8_t *data,
    uint16_t length
){
    if(data == NULL){
        return ARTIFACT_STORAGE_INVALID_ARGUMENT;
    }

    Staging_Manifest_t manifest;

    ArtifactStorage_Status_t status = ArtifactStorage_StagingReadManifest(&manifest);

    if(status != ARTIFACT_STORAGE_OK){
        return status;
    }

    if(!ArtifactStorage_ManifestHeaderIsValid(&manifest)){
        return ARTIFACT_STORAGE_BAD_STATE;
    }

    /*
     * Không nhận thêm fragment sau khi artifact đã publish.
     */
    if(manifest.artifact_valid_marker != ARTIFACT_MANIFEST_ERASED_WORD){
        return ARTIFACT_STORAGE_BAD_STATE;
    }

    uint16_t fragment_index = 0U;

    if(!ArtifactStorage_FragmentIsValid(
        &manifest,
        offset, length,
        &fragment_index
    )){
        return ARTIFACT_STORAGE_OUT_OF_RANGE;
    }

    const uint32_t bitmap_index = fragment_index >> 3U;

    const uint8_t bitmap_mask = (uint8_t)(1U << (fragment_index & 7U));

    const uint8_t bitmap_value = manifest.progress_bitmap[bitmap_index];

    /*
     * Bit đã bằng 0 nghĩa là fragment đã được program và verify.
     */
    if((bitmap_value & bitmap_mask) == 0U){
        const ExtFlash_Status_t verify_status = W25Q32_VerifyBlock(
            W25Q32_STAGING_IMAGE_BASE + offset,
            data,
            length
        );

        if(verify_status == EXT_FLASH_OK){
            return ARTIFACT_STORAGE_DUPLICATE;
        }

        if(verify_status == EXT_FLASH_VERIFY_FAILED){
            return ARTIFACT_STORAGE_CONFLICTING_DUPLICATE;
        }

        return ArtifactStorage_MapFlashStatus(
            verify_status
        );
    }

    /*
     * Program fragment rồi read-back trước khi commit progress.
     */
    status = ArtifactStorage_WriteVerified(
        W25Q32_STAGING_IMAGE_BASE + offset,
        data, length
    );

    if(status != ARTIFACT_STORAGE_OK){
        return status;
    }

    /*
     * Chỉ sau khi dữ liệu đúng mới clear progress bit 1 -> 0.
     */
    return ArtifactStorage_CommitProgressBit(
        fragment_index,
        bitmap_value
    );
}

ArtifactStorage_Status_t ArtifactStorage_StagingPublishValid(void){
    Staging_Manifest_t manifest;

    ArtifactStorage_Status_t status = ArtifactStorage_StagingReadManifest(&manifest);

    if(status != ARTIFACT_STORAGE_OK){
        return status;
    }

    if(!ArtifactStorage_ManifestIsComplete(&manifest)){
        return ARTIFACT_STORAGE_INCOMPLETE;
    }

    if(manifest.artifact_valid_marker == STAGING_ARTIFACT_VALID_MARKER_VALUE){
        return ARTIFACT_STORAGE_DUPLICATE;
    }

    if(manifest.artifact_valid_marker != ARTIFACT_MANIFEST_ERASED_WORD){
        return ARTIFACT_STORAGE_BAD_STATE;
    }

    /*
     * Hàm storage không tự xác thực chữ ký.
     * Caller phải verify CRC/chữ ký trước khi gọi API này.
     */
    const uint32_t valid_marker = STAGING_ARTIFACT_VALID_MARKER_VALUE;

    return ArtifactStorage_WriteVerified(
        W25Q32_STAGING_MANIFEST_BASE + STAGING_MANIFEST_VALID_MARKER_OFFSET,
        (const uint8_t *)&valid_marker,
        sizeof(valid_marker)
    );
}

ArtifactStorage_Status_t ArtifactStorage_BackupInvalidate(void){
    const ExtFlash_Status_t flash_status = W25Q32_SectorErase(W25Q32_BACKUP_MANIFEST_BASE);

    return ArtifactStorage_MapFlashStatus(flash_status);
}

ArtifactStorage_Status_t ArtifactStorage_BackupReadManifest(
    Backup_Manifest_t *manifest
){
    if(manifest == NULL){
        return ARTIFACT_STORAGE_INVALID_ARGUMENT;
    }

    const ExtFlash_Status_t flash_status = W25Q32_ReadBlock(
        W25Q32_BACKUP_MANIFEST_BASE,
        (uint8_t *)manifest,
        sizeof(*manifest)
    );

    if(flash_status != EXT_FLASH_OK){
        return ArtifactStorage_MapFlashStatus(flash_status);
    }
    return ArtifactStorage_BackupManifestIsValid(manifest) ?
        ARTIFACT_STORAGE_OK : ARTIFACT_STORAGE_BAD_STATE;
}

ArtifactStorage_Status_t ArtifactStorage_BackupRead(
    uint32_t offset,
    uint8_t *data,
    uint32_t length
){
    if(length == 0U){
        return ARTIFACT_STORAGE_OK;
    }

    if(data == NULL){
        return ARTIFACT_STORAGE_INVALID_ARGUMENT;
    }

    if(!ArtifactStorage_IsRangeValid(
        offset, length,
        W25Q32_BACKUP_IMAGE_SIZE
    )){
        return ARTIFACT_STORAGE_OUT_OF_RANGE;
    }

    const ExtFlash_Status_t flash_status = W25Q32_ReadBlock(
        W25Q32_BACKUP_IMAGE_BASE + offset,
        data, length
    );

    return ArtifactStorage_MapFlashStatus(flash_status);
}

ArtifactStorage_Status_t ArtifactStorage_BackupVerify(
    uint32_t offset,
    const uint8_t *expected,
    uint32_t length
){
    if(length == 0U){
        return ARTIFACT_STORAGE_OK;
    }

    if(expected == NULL){
        return ARTIFACT_STORAGE_INVALID_ARGUMENT;
    }

    if(!ArtifactStorage_IsRangeValid(
        offset, length,
        W25Q32_BACKUP_IMAGE_SIZE
    )){
        return ARTIFACT_STORAGE_OUT_OF_RANGE;
    }

    const ExtFlash_Status_t flash_status = W25Q32_VerifyBlock(
        W25Q32_BACKUP_IMAGE_BASE + offset,
        expected, length
    );

    return ArtifactStorage_MapFlashStatus(flash_status);
}

ArtifactStorage_Status_t ArtifactStorage_BackupValidate(
    Backup_Manifest_t *manifest
){
    if(manifest == NULL){
        return ARTIFACT_STORAGE_INVALID_ARGUMENT;
    }

    ArtifactStorage_Status_t status = ArtifactStorage_BackupReadManifest(manifest);

    if(status != ARTIFACT_STORAGE_OK){
        return status;
    }

    if(!ArtifactStorage_BackupManifestIsValid(manifest)){
        return ARTIFACT_STORAGE_BAD_STATE;
    }

    uint32_t image_crc32 = CRC32_INITIAL_VALUE;

    status = ArtifactStorage_CalculateFlashCrc(
        W25Q32_BACKUP_IMAGE_BASE,
        manifest->image_header.image_size,
        &image_crc32
    );

    if(status != ARTIFACT_STORAGE_OK){
        return status;
    }

    if(image_crc32 != manifest->image_header.image_crc32){
        return ARTIFACT_STORAGE_VERIFY_FAILED;
    }

    return ARTIFACT_STORAGE_OK;
}

ArtifactStorage_Status_t ArtifactStorage_BackupCreate(
    uint64_t operation_id,
    const Firmware_SignedHeader_t *active_header,
    const uint8_t active_signature[FIRMWARE_SIGNATURE_SIZE]
){
    if((operation_id == UINT64_C(0)) ||
       (active_signature == NULL) ||
       !ArtifactStorage_FirmwareHeaderIsStructurallyValid(active_header)
    ){
        return ARTIFACT_STORAGE_INVALID_ARGUMENT;
    }

    /*
     * Kiểm tra App nguồn trước khi phá Backup cũ. Nếu header không mô tả đúng
     * App đang chạy thì dừng ngay và giữ nguyên Backup hiện có.
     */
    const uint8_t *application = (const uint8_t *)(uintptr_t)APPLICATION_BASE_ADDRESS;

    if(Crc32_Compute(application, active_header->image_size) !=
       active_header->image_crc32
    ){
        return ARTIFACT_STORAGE_VERIFY_FAILED;
    }

    /*
     * Xóa manifest trước: từ thời điểm này mọi reset đều nhìn thấy Backup invalid
     * cho đến khi commit marker mới được program ở cuối transaction.
     */
    ArtifactStorage_Status_t status = ArtifactStorage_BackupInvalidate();

    if(status != ARTIFACT_STORAGE_OK){
        return status;
    }

    status = ArtifactStorage_EraseRegion(
        W25Q32_BACKUP_IMAGE_BASE,
        active_header->image_size
    );

    if(status != ARTIFACT_STORAGE_OK){
        return status;
    }

    status = ArtifactStorage_CopyApplicationToBackup(active_header);

    if(status != ARTIFACT_STORAGE_OK){
        return status;
    }

    /*
     * Quét lại chính Backup Image trước khi công bố manifest. Read-back từng
     * block lúc copy bắt lỗi cục bộ; CRC toàn image khóa lại kết quả cuối cùng.
     */
    uint32_t copied_crc32 = CRC32_INITIAL_VALUE;

    status = ArtifactStorage_CalculateFlashCrc(
        W25Q32_BACKUP_IMAGE_BASE,
        active_header->image_size,
        &copied_crc32
    );

    if(status != ARTIFACT_STORAGE_OK){
        return status;
    }

    if(copied_crc32 != active_header->image_crc32){
        return ARTIFACT_STORAGE_VERIFY_FAILED;
    }

    Backup_Manifest_t pending;
    memset(&pending, 0, sizeof(pending));

    pending.magic = BACKUP_MANIFEST_MAGIC_VALUE;
    pending.format_version = BACKUP_MANIFEST_FORMAT_VERSION;
    pending.operation_id = operation_id;
    pending.image_header = *active_header;

    memcpy(
        pending.image_signature,
        active_signature,
        sizeof(pending.image_signature)
    );

    pending.manifest_crc32 = Crc32_Compute(
        &pending,
        BACKUP_MANIFEST_CRC_SIZE
    );

    pending.commit_marker = BACKUP_MANIFEST_COMMIT_MARKER_VALUE;

    /*
     * Ghi và verify body trước; commit marker chưa được program nên record vẫn
     * invalid nếu mất nguồn hoặc gặp lỗi tại đây.
     */
    status = ArtifactStorage_WriteVerified(
        W25Q32_BACKUP_MANIFEST_BASE,
        (const uint8_t *)&pending,
        BACKUP_MANIFEST_COMMIT_OFFSET
    );

    if(status != ARTIFACT_STORAGE_OK){
        return status;
    }

    const uint32_t commit_marker = BACKUP_MANIFEST_COMMIT_MARKER_VALUE;

    status = ArtifactStorage_WriteVerified(
        W25Q32_BACKUP_MANIFEST_BASE + BACKUP_MANIFEST_COMMIT_OFFSET,
        (const uint8_t *)&commit_marker,
        sizeof(commit_marker)
    );

    if(status != ARTIFACT_STORAGE_OK){
        return status;
    }

    Backup_Manifest_t stored;
    return ArtifactStorage_BackupValidate(&stored);
}
