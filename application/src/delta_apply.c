#include "delta_apply.h"

#include <stddef.h>

#define DELTA_APPLY_BUFFER_SIZE 128U

static bool DeltaApply_RangeIsValid(
    uint32_t offset,
    uint32_t length,
    uint32_t size
){
    return (offset <= size) && (length <= (size - offset));
}

static uint16_t DeltaApply_ReadU16(const uint8_t *input){
    return (uint16_t)input[0] | (uint16_t)((uint16_t)input[1] << 8U);
}

static uint32_t DeltaApply_ReadU32(const uint8_t *input){
    return (uint32_t)input[0] |
           ((uint32_t)input[1] << 8U) |
           ((uint32_t)input[2] << 16U) |
           ((uint32_t)input[3] << 24U);
}

static DeltaApply_Status_t DeltaApply_CopyRange(
    DeltaApply_ReadCallback_t reader,
    void *reader_context,
    uint32_t reader_offset,
    DeltaApply_WriteCallback_t writer,
    void *writer_context,
    uint32_t writer_offset,
    uint32_t length
){
    uint8_t buffer[DELTA_APPLY_BUFFER_SIZE];
    uint32_t copied = 0U;

    while(copied < length){
        const uint32_t remaining = length - copied;
        const uint32_t chunk = remaining < sizeof(buffer) ?
            remaining : (uint32_t)sizeof(buffer);

        if(!reader(reader_context, reader_offset + copied, buffer, chunk)){
            return DELTA_APPLY_READ_ERROR;
        }
        if(!writer(writer_context, writer_offset + copied, buffer, chunk)){
            return DELTA_APPLY_WRITE_ERROR;
        }
        copied += chunk;
    }
    return DELTA_APPLY_OK;
}

DeltaApply_Status_t DeltaApply_Run(
    const Delta_ImmutableBody_t *body,
    DeltaApply_ReadCallback_t base_reader,
    void *base_context,
    DeltaApply_ReadCallback_t patch_reader,
    void *patch_context,
    DeltaApply_WriteCallback_t target_writer,
    void *target_context
){
    uint8_t header[DELTA_PATCH_COPY_RECORD_SIZE];
    uint32_t patch_offset = 0U;
    uint32_t target_offset = 0U;

    if((body == NULL) || (base_reader == NULL) ||
       (patch_reader == NULL) || (target_writer == NULL)){
        return DELTA_APPLY_INVALID_ARGUMENT;
    }
    if(body->algorithm_id != DELTA_ALGORITHM_COPY_ADD_V1){
        return DELTA_APPLY_UNSUPPORTED_ALGORITHM;
    }
    if((body->base_size == 0U) ||
       (body->target_header.image_size == 0U) ||
       (body->patch_size == 0U)){
        return DELTA_APPLY_INVALID_ARGUMENT;
    }

    while(patch_offset < body->patch_size){
        if(!patch_reader(patch_context, patch_offset, header, 1U)){
            return DELTA_APPLY_READ_ERROR;
        }
        if(header[0] == DELTA_PATCH_OPCODE_END){
            ++patch_offset;
            if((patch_offset != body->patch_size) ||
               (target_offset != body->target_header.image_size)){
                return DELTA_APPLY_SIZE_MISMATCH;
            }
            return DELTA_APPLY_OK;
        }

        if(header[0] == DELTA_PATCH_OPCODE_COPY){
            if(!DeltaApply_RangeIsValid(
                patch_offset,
                DELTA_PATCH_COPY_RECORD_SIZE,
                body->patch_size
            ) || !patch_reader(
                patch_context,
                patch_offset,
                header,
                DELTA_PATCH_COPY_RECORD_SIZE
            )){
                return DELTA_APPLY_READ_ERROR;
            }
            const uint32_t source_offset = DeltaApply_ReadU32(&header[1]);
            const uint16_t length = DeltaApply_ReadU16(&header[5]);

            if((length == 0U) ||
               !DeltaApply_RangeIsValid(source_offset, length, body->base_size) ||
               !DeltaApply_RangeIsValid(
                   target_offset,
                   length,
                   body->target_header.image_size
               )){
                return DELTA_APPLY_OUT_OF_RANGE;
            }
            const DeltaApply_Status_t status = DeltaApply_CopyRange(
                base_reader, base_context, source_offset,
                target_writer, target_context, target_offset, length
            );
            if(status != DELTA_APPLY_OK){
                return status;
            }
            patch_offset += DELTA_PATCH_COPY_RECORD_SIZE;
            target_offset += length;
            continue;
        }

        if(header[0] == DELTA_PATCH_OPCODE_ADD){
            if(!DeltaApply_RangeIsValid(
                patch_offset,
                DELTA_PATCH_ADD_HEADER_SIZE,
                body->patch_size
            ) || !patch_reader(
                patch_context,
                patch_offset,
                header,
                DELTA_PATCH_ADD_HEADER_SIZE
            )){
                return DELTA_APPLY_READ_ERROR;
            }
            const uint16_t length = DeltaApply_ReadU16(&header[1]);
            const uint32_t data_offset =
                patch_offset + DELTA_PATCH_ADD_HEADER_SIZE;

            if((length == 0U) ||
               !DeltaApply_RangeIsValid(data_offset, length, body->patch_size) ||
               !DeltaApply_RangeIsValid(
                   target_offset,
                   length,
                   body->target_header.image_size
               )){
                return DELTA_APPLY_OUT_OF_RANGE;
            }
            const DeltaApply_Status_t status = DeltaApply_CopyRange(
                patch_reader, patch_context, data_offset,
                target_writer, target_context, target_offset, length
            );
            if(status != DELTA_APPLY_OK){
                return status;
            }
            patch_offset = data_offset + length;
            target_offset += length;
            continue;
        }

        return DELTA_APPLY_MALFORMED_PATCH;
    }
    return DELTA_APPLY_MALFORMED_PATCH;
}
