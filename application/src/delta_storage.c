#include "delta_storage.h"

#include "crc32.h"
#include "ext_flash.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static bool DeltaStorage_RangeIsValid(
    uint32_t offset,
    uint32_t length,
    uint32_t size
){
    return (offset <= size) && (length <= (size - offset));
}

static DeltaStorage_Status_t DeltaStorage_MapFlash(ExtFlash_Status_t status){
    if(status == EXT_FLASH_OK){
        return DELTA_STORAGE_OK;
    }
    if(status == EXT_FLASH_VERIFY_FAILED){
        return DELTA_STORAGE_VERIFY_FAILED;
    }
    if((status == EXT_FLASH_OUT_OF_RANGE) ||
       (status == EXT_FLASH_UNALIGNED) ||
       (status == EXT_FLASH_PAGE_BOUNDARY)){
        return DELTA_STORAGE_OUT_OF_RANGE;
    }
    if(status == EXT_FLASH_INVALID_ARGUMENT){
        return DELTA_STORAGE_INVALID_ARGUMENT;
    }
    return DELTA_STORAGE_FLASH_ERROR;
}

static DeltaStorage_Status_t DeltaStorage_WriteVerified(
    uint32_t address,
    const uint8_t *data,
    uint32_t length
){
    ExtFlash_Status_t status = W25Q32_WriteBlock(address, data, length);

    if(status == EXT_FLASH_OK){
        status = W25Q32_VerifyBlock(address, data, length);
    }
    return DeltaStorage_MapFlash(status);
}

static bool DeltaStorage_HeaderIsValid(const Delta_Manifest_t *manifest){
    uint32_t expected_count;
    uint8_t reserved_value = 0xFFU;

    if((manifest == NULL) ||
       (manifest->immutable_body.format_version != DELTA_FORMAT_VERSION) ||
       (manifest->immutable_body.algorithm_id != DELTA_ALGORITHM_COPY_ADD_V1) ||
       (memcmp(manifest->immutable_body.domain, DELTA_DOMAIN_STRING,
               DELTA_DOMAIN_FIELD_SIZE) != 0) ||
       (manifest->operation_id == UINT64_C(0)) ||
       (manifest->transfer_id == UINT64_C(0)) ||
       (manifest->chunk_size == 0U) ||
       (manifest->chunk_size > OTA_DATA_CHUNK_MAX_SIZE) ||
       (manifest->immutable_body.patch_size == 0U) ||
       (manifest->immutable_body.patch_size > W25Q32_DELTA_DATA_SIZE) ||
       (manifest->header_commit_marker != DELTA_HEADER_COMMIT_MARKER_VALUE) ||
       ((manifest->artifact_valid_marker != DELTA_ERASED_WORD) &&
        (manifest->artifact_valid_marker !=
            DELTA_ARTIFACT_VALID_MARKER_VALUE)) ||
       ((manifest->apply_started_marker != DELTA_ERASED_WORD) &&
        (manifest->apply_started_marker !=
            DELTA_APPLY_STARTED_MARKER_VALUE)) ||
       ((manifest->apply_started_marker == DELTA_APPLY_STARTED_MARKER_VALUE) &&
        (manifest->artifact_valid_marker !=
            DELTA_ARTIFACT_VALID_MARKER_VALUE))){
        return false;
    }
    for(size_t index = 0U;
        index < sizeof(manifest->reserved_erased);
        ++index){
        reserved_value &= manifest->reserved_erased[index];
    }
    if(reserved_value != 0xFFU){
        return false;
    }
    expected_count = (manifest->immutable_body.patch_size +
                      manifest->chunk_size - 1U) / manifest->chunk_size;
    if((expected_count == 0U) ||
       (expected_count > DELTA_PROGRESS_FRAGMENT_MAX) ||
       (manifest->fragment_count != expected_count)){
        return false;
    }
    return manifest->record_crc32 == Crc32_Compute(
        manifest,
        DELTA_MANIFEST_CRC_SIZE
    );
}

static bool DeltaStorage_IsComplete(const Delta_Manifest_t *manifest){
    if(!DeltaStorage_HeaderIsValid(manifest)){
        return false;
    }
    for(uint16_t index = 0U; index < manifest->fragment_count; ++index){
        if((manifest->progress_bitmap[index >> 3U] &
            (uint8_t)(1U << (index & 7U))) != 0U){
            return false;
        }
    }
    return true;
}

DeltaStorage_Status_t DeltaStorage_Begin(const Delta_Manifest_t *manifest){
    Delta_Manifest_t pending;
    uint32_t sector_count;
    DeltaStorage_Status_t status;

    if(manifest == NULL){
        return DELTA_STORAGE_INVALID_ARGUMENT;
    }
    pending = *manifest;
    pending.record_crc32 = Crc32_Compute(&pending, DELTA_MANIFEST_CRC_SIZE);
    pending.header_commit_marker = DELTA_HEADER_COMMIT_MARKER_VALUE;
    pending.artifact_valid_marker = DELTA_ERASED_WORD;
    pending.apply_started_marker = DELTA_ERASED_WORD;
    memset(pending.reserved_erased, 0xFF, sizeof(pending.reserved_erased));
    memset(pending.progress_bitmap, 0xFF, sizeof(pending.progress_bitmap));
    if(!DeltaStorage_HeaderIsValid(&pending)){
        return DELTA_STORAGE_INVALID_ARGUMENT;
    }

    status = DeltaStorage_MapFlash(
        W25Q32_SectorErase(W25Q32_DELTA_MANIFEST_BASE)
    );
    if(status != DELTA_STORAGE_OK){
        return status;
    }
    sector_count = (pending.immutable_body.patch_size +
                    W25Q32_SECTOR_SIZE - 1U) / W25Q32_SECTOR_SIZE;
    for(uint32_t sector = 0U; sector < sector_count; ++sector){
        status = DeltaStorage_MapFlash(W25Q32_SectorErase(
            W25Q32_DELTA_DATA_BASE + sector * W25Q32_SECTOR_SIZE
        ));
        if(status != DELTA_STORAGE_OK){
            return status;
        }
    }

    status = DeltaStorage_WriteVerified(
        W25Q32_DELTA_MANIFEST_BASE,
        (const uint8_t *)&pending,
        DELTA_MANIFEST_HEADER_MARKER_OFFSET
    );
    if(status != DELTA_STORAGE_OK){
        return status;
    }
    return DeltaStorage_WriteVerified(
        W25Q32_DELTA_MANIFEST_BASE + DELTA_MANIFEST_HEADER_MARKER_OFFSET,
        (const uint8_t *)&pending.header_commit_marker,
        sizeof(pending.header_commit_marker)
    );
}

DeltaStorage_Status_t DeltaStorage_ReadManifest(Delta_Manifest_t *manifest){
    if(manifest == NULL){
        return DELTA_STORAGE_INVALID_ARGUMENT;
    }
    const ExtFlash_Status_t status = W25Q32_ReadBlock(
        W25Q32_DELTA_MANIFEST_BASE,
        (uint8_t *)manifest,
        sizeof(*manifest)
    );
    if(status != EXT_FLASH_OK){
        return DeltaStorage_MapFlash(status);
    }
    return DeltaStorage_HeaderIsValid(manifest) ?
        DELTA_STORAGE_OK : DELTA_STORAGE_BAD_STATE;
}

DeltaStorage_Status_t DeltaStorage_WriteFragment(
    uint32_t offset,
    const uint8_t *data,
    uint16_t length
){
    Delta_Manifest_t manifest;
    uint16_t index;
    uint8_t mask;
    uint8_t bitmap_byte;
    DeltaStorage_Status_t status;

    if((data == NULL) || (length == 0U)){
        return DELTA_STORAGE_INVALID_ARGUMENT;
    }
    status = DeltaStorage_ReadManifest(&manifest);
    if(status != DELTA_STORAGE_OK){
        return status;
    }
    if((manifest.artifact_valid_marker != DELTA_ERASED_WORD) ||
       ((offset % manifest.chunk_size) != 0U) ||
       !DeltaStorage_RangeIsValid(
           offset, length, manifest.immutable_body.patch_size
       )){
        return DELTA_STORAGE_BAD_STATE;
    }
    const uint32_t remaining = manifest.immutable_body.patch_size - offset;
    const uint16_t expected = remaining < manifest.chunk_size ?
        (uint16_t)remaining : manifest.chunk_size;
    if(length != expected){
        return DELTA_STORAGE_OUT_OF_RANGE;
    }
    index = (uint16_t)(offset / manifest.chunk_size);
    mask = (uint8_t)(1U << (index & 7U));
    bitmap_byte = manifest.progress_bitmap[index >> 3U];
    if((bitmap_byte & mask) == 0U){
        const ExtFlash_Status_t verify_status = W25Q32_VerifyBlock(
            W25Q32_DELTA_DATA_BASE + offset, data, length
        );
        return verify_status == EXT_FLASH_OK ?
            DELTA_STORAGE_DUPLICATE : DELTA_STORAGE_CONFLICTING_DUPLICATE;
    }

    status = DeltaStorage_WriteVerified(
        W25Q32_DELTA_DATA_BASE + offset, data, length
    );
    if(status != DELTA_STORAGE_OK){
        return status;
    }
    bitmap_byte &= (uint8_t)~mask;
    return DeltaStorage_WriteVerified(
        W25Q32_DELTA_MANIFEST_BASE + DELTA_MANIFEST_BITMAP_OFFSET +
            (index >> 3U),
        &bitmap_byte,
        sizeof(bitmap_byte)
    );
}

DeltaStorage_Status_t DeltaStorage_Read(
    uint32_t offset,
    uint8_t *data,
    uint32_t length
){
    if((data == NULL) || (length == 0U)){
        return DELTA_STORAGE_INVALID_ARGUMENT;
    }
    if(!DeltaStorage_RangeIsValid(offset, length, W25Q32_DELTA_DATA_SIZE)){
        return DELTA_STORAGE_OUT_OF_RANGE;
    }
    return DeltaStorage_MapFlash(W25Q32_ReadBlock(
        W25Q32_DELTA_DATA_BASE + offset, data, length
    ));
}

DeltaStorage_Status_t DeltaStorage_PublishValid(void){
    Delta_Manifest_t manifest;
    const uint32_t marker = DELTA_ARTIFACT_VALID_MARKER_VALUE;
    DeltaStorage_Status_t status = DeltaStorage_ReadManifest(&manifest);

    if(status != DELTA_STORAGE_OK){
        return status;
    }
    if(!DeltaStorage_IsComplete(&manifest)){
        return DELTA_STORAGE_INCOMPLETE;
    }
    if(manifest.artifact_valid_marker == marker){
        return DELTA_STORAGE_OK;
    }
    if(manifest.artifact_valid_marker != DELTA_ERASED_WORD){
        return DELTA_STORAGE_BAD_STATE;
    }
    return DeltaStorage_WriteVerified(
        W25Q32_DELTA_MANIFEST_BASE + DELTA_MANIFEST_VALID_MARKER_OFFSET,
        (const uint8_t *)&marker,
        sizeof(marker)
    );
}

DeltaStorage_Status_t DeltaStorage_MarkApplyStarted(void){
    Delta_Manifest_t manifest;
    const uint32_t marker = DELTA_APPLY_STARTED_MARKER_VALUE;
    DeltaStorage_Status_t status = DeltaStorage_ReadManifest(&manifest);

    if(status != DELTA_STORAGE_OK){
        return status;
    }
    if(manifest.artifact_valid_marker != DELTA_ARTIFACT_VALID_MARKER_VALUE){
        return DELTA_STORAGE_BAD_STATE;
    }
    if(manifest.apply_started_marker == marker){
        return DELTA_STORAGE_OK;
    }
    if(manifest.apply_started_marker != DELTA_ERASED_WORD){
        return DELTA_STORAGE_BAD_STATE;
    }
    return DeltaStorage_WriteVerified(
        W25Q32_DELTA_MANIFEST_BASE + DELTA_MANIFEST_APPLY_MARKER_OFFSET,
        (const uint8_t *)&marker,
        sizeof(marker)
    );
}

DeltaStorage_Status_t DeltaStorage_Invalidate(void){
    return DeltaStorage_MapFlash(
        W25Q32_SectorErase(W25Q32_DELTA_MANIFEST_BASE)
    );
}
