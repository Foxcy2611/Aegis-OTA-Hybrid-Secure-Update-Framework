#include "boot_metadata.h"

#include "flash_program.h"
#include "memory_map.h" 
#include "metadata_record.h"

#include <string.h>
#include <stddef.h>

/* ==================== Helper private ==================== */

// Kiểm tra page đã được xóa hay chưa (trắng)
static bool BootMetadata_IsPageErased(uint32_t page_address){
    const volatile uint8_t *page = (const volatile uint8_t *)(uintptr_t)page_address;

    for(uint32_t i = 0U; i < METADATA_PAGE_SIZE; ++i){
        if(page[i] != 0xFFU){
            return false;
        }
    }

    return true;
}

// Hàm ghi 1 bản Record vào 1 page METADATA
static BootMetadata_Status_t BootMetadata_CommitPage(
    uint32_t page_address,
    const Metadata_Record_t *record
){
    Flash_Status_t flash_status;

    /*
     * Bước 1:
     * Ghi body [0, 124), bao gồm metadata_crc32 nhưng chưa ghi marker.
     */
    flash_status = FlashProgram_WriteBuffer(
        page_address,
        (const uint8_t *)record,
        METADATA_COMMIT_MARKER_OFFSET
    );

    if(flash_status != FLASH_PROGRAM_OK){
        if(flash_status == FLASH_PROGRAM_VERIFY_FAILED){
            return BOOT_METADATA_VERIFY_ERROR;
        }

        return BOOT_METADATA_WRITE_ERROR;
    }

    /*
     * Bước 2:
     * Verify body trước khi publish commit marker.
     */
    flash_status = FlashProgram_Verify(
        page_address,
        (const uint8_t *)record,
        METADATA_COMMIT_MARKER_OFFSET
    );

    if(flash_status != FLASH_PROGRAM_OK){
        return BOOT_METADATA_VERIFY_ERROR;
    }

    /*
     * Bước 3:
     * Chỉ khi body đúng mới ghi commit marker.
     *
     * Dùng biến riêng để không lấy địa chỉ trực tiếp của field
     * nằm trong packed struct.
     */
    const uint32_t commit_marker = METADATA_COMMIT_MARKER_VALUE;

    flash_status = FlashProgram_WriteBuffer(
        page_address + METADATA_COMMIT_MARKER_OFFSET,
        (const uint8_t *)&commit_marker,
        sizeof(commit_marker)
    );

    if(flash_status != FLASH_PROGRAM_OK){
        if(flash_status == FLASH_PROGRAM_VERIFY_FAILED){
            return BOOT_METADATA_VERIFY_ERROR;
        }

        return BOOT_METADATA_WRITE_ERROR;
    }

    /*
     * Bước 4:
     * Verify toàn bộ 128 byte sau khi marker đã được ghi.
     */
    flash_status = FlashProgram_Verify(
        page_address,
        (const uint8_t *)record,
        METADATA_RECORD_SIZE
    );

    if(flash_status != FLASH_PROGRAM_OK){
        return BOOT_METADATA_VERIFY_ERROR;
    }

    /*
     * Bước 5:
     * Copy record đã ghi về RAM rồi validate lại cấu trúc và CRC.
     */
    Metadata_Record_t stored_record;

    memcpy(
        &stored_record,
        (const void *)(uintptr_t)page_address,
        sizeof(stored_record)
    );

    if(!MetadataRecord_IsStructurallyValid(&stored_record)){
        return BOOT_METADATA_VERIFY_ERROR;
    }

    return BOOT_METADATA_OK;
}

/* ==================== API public ==================== */

BootMetadata_Status_t BootMetadata_Init(
    BootMetadata_Context_t *context,
    Metadata_Record_t *out_record
){
    if((context == NULL) || (out_record == NULL)){
        return BOOT_METADATA_INVALID_ARGUMENT;
    }

    memset(context, 0, sizeof(*context));

    Metadata_Record_t record_a;
    Metadata_Record_t record_b;

    memcpy(
        &record_a,
        (const void *)(uintptr_t)METADATA_PAGE_A_ADDRESS,
        sizeof(record_a)
    );

    memcpy(
        &record_b,
        (const void *)(uintptr_t)METADATA_PAGE_B_ADDRESS,
        sizeof(record_b)
    );

    const bool record_a_valid = MetadataRecord_IsStructurallyValid(&record_a);

    const bool record_b_valid = MetadataRecord_IsStructurallyValid(&record_b);

    if(record_a_valid && record_b_valid){
        /*
         * Serial-number arithmetic 32-bit:
         * - difference == 0: hai generation bằng nhau, chọn A ổn định.
         * - difference < 2^31: A mới hơn B, kể cả khi counter đã wrap.
         * - difference > 2^31: B mới hơn A.
         * - difference == 2^31: không thể xác định thứ tự an toàn.
         */
        const uint32_t difference = record_a.generation - record_b.generation;

        if(difference == UINT32_C(0x80000000)){
            return BOOT_METADATA_GENERATION_AMBIGUOUS;
        }

        if((difference == 0U) || (difference < UINT32_C(0x80000000))){
            *out_record = record_a;
            context->active_page_address = METADATA_PAGE_A_ADDRESS;
        } else {
            *out_record = record_b;
            context->active_page_address = METADATA_PAGE_B_ADDRESS;
        }

    } else if(record_a_valid){
        *out_record = record_a;
        context->active_page_address = METADATA_PAGE_A_ADDRESS;

    } else if(record_b_valid){
        *out_record = record_b;
        context->active_page_address = METADATA_PAGE_B_ADDRESS;

    } else {
        /*
        * Cả hai record đều invalid.
        *
        * Chỉ được gọi FIRST_BOOT khi toàn bộ hai page đều còn 0xFF.
        * Nếu có bất kỳ byte nào đã bị program thì fail-closed là CORRUPT.
        */

        const bool page_a_erased = BootMetadata_IsPageErased(METADATA_PAGE_A_ADDRESS);
        const bool page_b_erased = BootMetadata_IsPageErased(METADATA_PAGE_B_ADDRESS);

        if(!page_a_erased || !page_b_erased){
            return BOOT_METADATA_CORRUPT;
        }

        if(!MetadataRecord_InitializeDefault(out_record)){
            return BOOT_METADATA_INVALID_ARGUMENT;
        }

        /* Lần commit đầu tiên sẽ ghi page A với generation bằng 1. */
        context->initialized = true;
        return BOOT_METADATA_FIRST_BOOT;
    }

    /*
    * Chỉ đến đây mới publish record active vào caller/context.
    */

    context->active_generation = out_record->generation;
    context->has_active_record = true;
    context->initialized = true;

    return BOOT_METADATA_OK;
}

BootMetadata_Status_t BootMetadata_Commit(
    BootMetadata_Context_t *context,
    Metadata_Record_t *in_out_record
){
    if((context == NULL) || (in_out_record == NULL)){
        return BOOT_METADATA_INVALID_ARGUMENT;
    }

    if(!context->initialized){
        return BOOT_METADATA_NOT_INITIALIZED;
    }

    /*
     * FIRST_BOOT ghi page A.
     * A active thì ghi page B.
     * B active thì ghi page A.
     */
    
    uint32_t target_page_address;

    if(!context->has_active_record){
        if((context->active_page_address != 0U) ||
           (context->active_generation != 0U)){
            return BOOT_METADATA_INVALID_ARGUMENT;
        }

        target_page_address = METADATA_PAGE_A_ADDRESS;
    } else if(context->active_page_address == METADATA_PAGE_A_ADDRESS){
        target_page_address = METADATA_PAGE_B_ADDRESS;
    } else if(context->active_page_address == METADATA_PAGE_B_ADDRESS){
        target_page_address = METADATA_PAGE_A_ADDRESS;
    } else {
        return BOOT_METADATA_INVALID_ARGUMENT;
    }

    /*
     * Chỉ làm việc trên bản pending. Record của caller và context vẫn giữ
     * nguyên nếu mất điện hoặc một bước erase/program/verify thất bại.
     */
    Metadata_Record_t pending = *in_out_record;

    pending.magic = METADATA_MAGIC_VALUE;
    pending.format_version = METADATA_FORMAT_VERSION;
    pending.generation = context->has_active_record ?
        (context->active_generation + 1U) : 1U;
    pending.reserved = 0U;
    pending.commit_marker = METADATA_COMMIT_MARKER_VALUE;

    if(!MetadataRecord_RefreshCrc32(&pending) ||
       !MetadataRecord_IsStructurallyValid(&pending)){
        return BOOT_METADATA_INVALID_ARGUMENT;
    }

    const Flash_Status_t erase_status =
        FlashProgram_ErasePage(target_page_address);

    if(erase_status != FLASH_PROGRAM_OK){
        return BOOT_METADATA_ERASE_ERROR;
    }

    const BootMetadata_Status_t commit_status =
        BootMetadata_CommitPage(target_page_address, &pending);

    if(commit_status != BOOT_METADATA_OK){
        /*
         * Page target có thể chứa body ghi dở nhưng chưa có marker.
         * Page active cũ vẫn còn nguyên và sẽ được chọn ở lần reset sau.
         */
         
        return commit_status;
    }

    /* Chỉ publish state RAM sau khi page mới đã commit và validate đầy đủ. */
    context->active_page_address = target_page_address;
    context->active_generation = pending.generation;
    context->has_active_record = true;
    *in_out_record = pending;

    return BOOT_METADATA_OK;
}
