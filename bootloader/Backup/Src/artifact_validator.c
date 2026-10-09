#include "artifact_validator.h"

#include "crc32.h"
#include "ext_flash.h"
#include "memory_map.h"

#include <stdbool.h>
#include <stddef.h>

#define ARTIFACT_VALIDATOR_IO_CHUNK_SIZE W25Q32_PAGE_SIZE

static bool ArtifactValidator_IsRangeValid(uint32_t offset,
                                           uint32_t length,
                                           uint32_t region_size){
    if(offset > region_size){
        return false;
    }

    return length <= (region_size - offset);
}

static ArtifactValidator_Status_t ArtifactValidator_MapFlashStatus(
    ExtFlash_Status_t status
){
    return (status == EXT_FLASH_OK) ?
               ARTIFACT_VALIDATOR_OK :
               ARTIFACT_VALIDATOR_EXT_FLASH_ERROR;
}

static ArtifactValidator_Status_t ArtifactValidator_CalculateExternalCrc(
    uint32_t address,
    uint32_t length,
    uint32_t *crc32
){
    if(crc32 == NULL){
        return ARTIFACT_VALIDATOR_INVALID_ARGUMENT;
    }

    uint8_t buffer[ARTIFACT_VALIDATOR_IO_CHUNK_SIZE];
    uint32_t remaining = length;
    uint32_t current_address = address;
    uint32_t running_crc = 0U;

    while(remaining > 0U){
        const uint32_t chunk = (remaining < sizeof(buffer)) ?
                                   remaining :
                                   (uint32_t)sizeof(buffer);
        const ExtFlash_Status_t status =
            W25Q32_ReadBlock(current_address, buffer, chunk);
        if(status != EXT_FLASH_OK){
            return ArtifactValidator_MapFlashStatus(status);
        }

        running_crc = Crc32_Update(running_crc, buffer, chunk);
        current_address += chunk;
        remaining -= chunk;
    }

    *crc32 = running_crc;
    return ARTIFACT_VALIDATOR_OK;
}

ArtifactValidator_Status_t ArtifactValidator_StagingReadManifest(
    Staging_Manifest_t *manifest
){
    if(manifest == NULL){
        return ARTIFACT_VALIDATOR_INVALID_ARGUMENT;
    }

    const ExtFlash_Status_t status = W25Q32_ReadBlock(
        W25Q32_STAGING_MANIFEST_BASE,
        (uint8_t *)manifest,
        STAGING_MANIFEST_RECORD_SIZE
    );
    if(status != EXT_FLASH_OK){
        return ArtifactValidator_MapFlashStatus(status);
    }

    return ArtifactManifest_StagingIsValid(manifest) ?
               ARTIFACT_VALIDATOR_OK :
               ARTIFACT_VALIDATOR_MANIFEST_INVALID;
}

ArtifactValidator_Status_t ArtifactValidator_StagingValidate(
    Staging_Manifest_t *manifest
){
    ArtifactValidator_Status_t status =
        ArtifactValidator_StagingReadManifest(manifest);
    if(status != ARTIFACT_VALIDATOR_OK){
        return status;
    }

    uint32_t image_crc = 0U;
    status = ArtifactValidator_CalculateExternalCrc(
        W25Q32_STAGING_IMAGE_BASE,
        manifest->image_header.image_size,
        &image_crc
    );
    if(status != ARTIFACT_VALIDATOR_OK){
        return status;
    }

    return (image_crc == manifest->image_header.image_crc32) ?
               ARTIFACT_VALIDATOR_OK :
               ARTIFACT_VALIDATOR_CRC_MISMATCH;
}

ArtifactValidator_Status_t ArtifactValidator_StagingRead(uint32_t offset,
                                                         uint8_t *data,
                                                         uint32_t length){
    if(!ArtifactValidator_IsRangeValid(offset,
                                       length,
                                       W25Q32_STAGING_IMAGE_SIZE)){
        return ARTIFACT_VALIDATOR_OUT_OF_RANGE;
    }

    if(length == 0U){
        return ARTIFACT_VALIDATOR_OK;
    }

    if(data == NULL){
        return ARTIFACT_VALIDATOR_INVALID_ARGUMENT;
    }

    return ArtifactValidator_MapFlashStatus(W25Q32_ReadBlock(
        W25Q32_STAGING_IMAGE_BASE + offset,
        data,
        length
    ));
}

ArtifactValidator_Status_t ArtifactValidator_BackupReadManifest(
    Backup_Manifest_t *manifest
){
    if(manifest == NULL){
        return ARTIFACT_VALIDATOR_INVALID_ARGUMENT;
    }

    const ExtFlash_Status_t status = W25Q32_ReadBlock(
        W25Q32_BACKUP_MANIFEST_BASE,
        (uint8_t *)manifest,
        BACKUP_MANIFEST_RECORD_SIZE
    );
    if(status != EXT_FLASH_OK){
        return ArtifactValidator_MapFlashStatus(status);
    }

    return ArtifactManifest_BackupIsValid(manifest) ?
               ARTIFACT_VALIDATOR_OK :
               ARTIFACT_VALIDATOR_MANIFEST_INVALID;
}

ArtifactValidator_Status_t ArtifactValidator_BackupValidate(
    Backup_Manifest_t *manifest
){
    ArtifactValidator_Status_t status =
        ArtifactValidator_BackupReadManifest(manifest);
    if(status != ARTIFACT_VALIDATOR_OK){
        return status;
    }

    uint32_t image_crc = 0U;
    status = ArtifactValidator_CalculateExternalCrc(
        W25Q32_BACKUP_IMAGE_BASE,
        manifest->image_header.image_size,
        &image_crc
    );
    if(status != ARTIFACT_VALIDATOR_OK){
        return status;
    }

    return (image_crc == manifest->image_header.image_crc32) ?
               ARTIFACT_VALIDATOR_OK :
               ARTIFACT_VALIDATOR_CRC_MISMATCH;
}

ArtifactValidator_Status_t ArtifactValidator_BackupRead(uint32_t offset,
                                                        uint8_t *data,
                                                        uint32_t length){
    if(!ArtifactValidator_IsRangeValid(offset,
                                       length,
                                       W25Q32_BACKUP_IMAGE_SIZE)){
        return ARTIFACT_VALIDATOR_OUT_OF_RANGE;
    }

    if(length == 0U){
        return ARTIFACT_VALIDATOR_OK;
    }

    if(data == NULL){
        return ARTIFACT_VALIDATOR_INVALID_ARGUMENT;
    }

    return ArtifactValidator_MapFlashStatus(W25Q32_ReadBlock(
        W25Q32_BACKUP_IMAGE_BASE + offset,
        data,
        length
    ));
}
