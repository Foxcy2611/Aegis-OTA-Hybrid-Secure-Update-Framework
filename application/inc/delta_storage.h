#ifndef AEGIS_DELTA_STORAGE_H
#define AEGIS_DELTA_STORAGE_H

/* Persistent receive store của DELTA_MANIFEST và DELTA_DATA trên W25Q32. */

#include "delta_format.h"

#include <stdint.h>

typedef enum{
    DELTA_STORAGE_OK = 0U,
    DELTA_STORAGE_DUPLICATE,
    DELTA_STORAGE_INVALID_ARGUMENT,
    DELTA_STORAGE_OUT_OF_RANGE,
    DELTA_STORAGE_BAD_STATE,
    DELTA_STORAGE_CONFLICTING_DUPLICATE,
    DELTA_STORAGE_INCOMPLETE,
    DELTA_STORAGE_FLASH_ERROR,
    DELTA_STORAGE_VERIFY_FAILED
} DeltaStorage_Status_t;

DeltaStorage_Status_t DeltaStorage_Begin(const Delta_Manifest_t *manifest);
DeltaStorage_Status_t DeltaStorage_ReadManifest(Delta_Manifest_t *manifest);
DeltaStorage_Status_t DeltaStorage_WriteFragment(
    uint32_t offset,
    const uint8_t *data,
    uint16_t length
);
DeltaStorage_Status_t DeltaStorage_Read(
    uint32_t offset,
    uint8_t *data,
    uint32_t length
);
DeltaStorage_Status_t DeltaStorage_PublishValid(void);
DeltaStorage_Status_t DeltaStorage_MarkApplyStarted(void);
DeltaStorage_Status_t DeltaStorage_Invalidate(void);

#endif /* AEGIS_DELTA_STORAGE_H */
