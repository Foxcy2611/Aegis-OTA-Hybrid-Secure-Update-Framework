#ifndef AEGIS_ARTIFACT_VALIDATOR_H
#define AEGIS_ARTIFACT_VALIDATOR_H

/* Bootloader-owned read-only validation access to committed W25Q32 artifacts. */

#include "artifact_manifest.h"

#include <stdint.h>

typedef enum {
    ARTIFACT_VALIDATOR_OK = 0U,             /* Manifest and image CRC are valid. */
    ARTIFACT_VALIDATOR_INVALID_ARGUMENT,    /* Required pointer is NULL. */
    ARTIFACT_VALIDATOR_OUT_OF_RANGE,        /* Relative read leaves its image area. */
    ARTIFACT_VALIDATOR_EXT_FLASH_ERROR,     /* W25Q32 read failed. */
    ARTIFACT_VALIDATOR_MANIFEST_INVALID,    /* Format, CRC or marker is invalid. */
    ARTIFACT_VALIDATOR_CRC_MISMATCH         /* Image CRC differs from SignedHeader. */
} ArtifactValidator_Status_t;

ArtifactValidator_Status_t ArtifactValidator_StagingReadManifest(
    Staging_Manifest_t *manifest
);
ArtifactValidator_Status_t ArtifactValidator_StagingValidate(
    Staging_Manifest_t *manifest
);
ArtifactValidator_Status_t ArtifactValidator_StagingRead(uint32_t offset,
                                                         uint8_t *data,
                                                         uint32_t length);

ArtifactValidator_Status_t ArtifactValidator_BackupReadManifest(
    Backup_Manifest_t *manifest
);
ArtifactValidator_Status_t ArtifactValidator_BackupValidate(
    Backup_Manifest_t *manifest
);
ArtifactValidator_Status_t ArtifactValidator_BackupRead(uint32_t offset,
                                                        uint8_t *data,
                                                        uint32_t length);

#endif /* AEGIS_ARTIFACT_VALIDATOR_H */
