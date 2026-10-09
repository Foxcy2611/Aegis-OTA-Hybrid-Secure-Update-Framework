#include "metadata_record.h"

#include "crc32.h"

#include <stddef.h>
#include <string.h>

bool MetadataRecord_RefreshCrc32(Metadata_Record_t *record){
    if(record == NULL){
        return false;
    }

    record->metadata_crc32 = Crc32_Compute(
        (const void *)record,
        METADATA_CRC_REGION_SIZE
    );

    return true;
}

bool MetadataRecord_IsStructurallyValid(const Metadata_Record_t *record){
    if(record == NULL){
        return false;
    }

    if(record->magic != METADATA_MAGIC_VALUE){
        return false;
    }

    if(record->format_version !=
       METADATA_FORMAT_VERSION){
        return false;
    }

    if(record->commit_marker !=
       METADATA_COMMIT_MARKER_VALUE){
        return false;
    }

    if(record->reserved != 0U){
        return false;
    }

    if(record->trial_boot_flag >
       METADATA_TRIAL_BOOT_TRUE){
        return false;
    }

    if(record->operation_state >
       METADATA_OPERATION_ROLLBACK_IN_PROGRESS){
        return false;
    }

    /* image_size chỉ được kiểm tra range; nội dung header thuộc Phase 3. */
    if(record->active_header.image_size > APPLICATION_SIZE){
        return false;
    }

    /* Default record hợp lệ về cấu trúc nhưng không bootable. */
    if(record->active_header.image_size == 0U){
        if(record->operation_state !=
           METADATA_OPERATION_IDLE){
            return false;
        }

        if(record->trial_boot_flag !=
           METADATA_TRIAL_BOOT_FALSE){
            return false;
        }

        if(record->boot_attempt_count != 0U){
            return false;
        }

        if(record->operation_id != UINT64_C(0)){
            return false;
        }
    }

    /*
     * INSTALL hoặc ROLLBACK phải bind với một operation cụ thể.
     */
    if((record->operation_state !=
        METADATA_OPERATION_IDLE) &&
       (record->operation_id == UINT64_C(0))){
        return false;
    }

    /*
     * Trial Boot phải có active image và operation_id.
     */
    if(record->trial_boot_flag ==
       METADATA_TRIAL_BOOT_TRUE){
        if(record->operation_id == UINT64_C(0)){
            return false;
        }

        if(record->active_header.image_size == 0U){
            return false;
        }
    }

    /*
     * IDLE ổn định, không Trial Boot:
     * - Không còn transaction.
     * - Bộ đếm Trial Boot phải được reset.
     */
    if((record->operation_state ==
        METADATA_OPERATION_IDLE) &&
       (record->trial_boot_flag ==
        METADATA_TRIAL_BOOT_FALSE)){
        if(record->operation_id != UINT64_C(0)){
            return false;
        }

        if(record->boot_attempt_count != 0U){
            return false;
        }
    }

    /*
     * CRC được kiểm tra cuối cùng sau khi toàn bộ field đã hợp lệ.
     */
    return record->metadata_crc32 == Crc32_Compute(
        (const void *)record,
        METADATA_CRC_REGION_SIZE
    );
}

bool MetadataRecord_InitializeDefault(Metadata_Record_t *record){
    if(record == NULL){
        return false;
    }

    memset(record, 0, sizeof(*record));

    record->magic =
        METADATA_MAGIC_VALUE;

    record->format_version =
        METADATA_FORMAT_VERSION;

    record->generation = 0U;

    record->operation_state =
        METADATA_OPERATION_IDLE;

    record->trial_boot_flag =
        METADATA_TRIAL_BOOT_FALSE;

    record->boot_attempt_count = 0U;
    record->reserved = 0U;
    record->operation_id = UINT64_C(0);

    /*
     * Default record hợp lệ về cấu trúc nhưng chưa có active image.
     */
    record->commit_marker =
        METADATA_COMMIT_MARKER_VALUE;

    if(!MetadataRecord_RefreshCrc32(record)){
        return false;
    }

    return MetadataRecord_IsStructurallyValid(record);
}
