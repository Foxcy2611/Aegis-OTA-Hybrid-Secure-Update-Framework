#ifndef AEGIS_DELTA_APPLY_H
#define AEGIS_DELTA_APPLY_H

/* Streaming applier COPY/ADD: đọc base + patch, ghi full target sang Staging. */

#include "delta_format.h"

#include <stdbool.h>
#include <stdint.h>

typedef bool (*DeltaApply_ReadCallback_t)(
    void *context,
    uint32_t offset,
    uint8_t *buffer,
    uint32_t length
);

typedef bool (*DeltaApply_WriteCallback_t)(
    void *context,
    uint32_t offset,
    const uint8_t *buffer,
    uint32_t length
);

typedef enum{
    DELTA_APPLY_OK = 0U,
    DELTA_APPLY_INVALID_ARGUMENT,
    DELTA_APPLY_UNSUPPORTED_ALGORITHM,
    DELTA_APPLY_MALFORMED_PATCH,
    DELTA_APPLY_OUT_OF_RANGE,
    DELTA_APPLY_READ_ERROR,
    DELTA_APPLY_WRITE_ERROR,
    DELTA_APPLY_SIZE_MISMATCH
} DeltaApply_Status_t;

DeltaApply_Status_t DeltaApply_Run(
    const Delta_ImmutableBody_t *body,
    DeltaApply_ReadCallback_t base_reader,
    void *base_context,
    DeltaApply_ReadCallback_t patch_reader,
    void *patch_context,
    DeltaApply_WriteCallback_t target_writer,
    void *target_context
);

#endif /* AEGIS_DELTA_APPLY_H */
