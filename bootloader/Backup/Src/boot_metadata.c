#include "boot_metadata.h"
#include "flash_program.h"
#include "memory_map.h" /* Bắt buộc phải có METADATA_PAGE_A_ADDRESS và METADATA_PAGE_B_ADDRESS */
#include <string.h>

/* Địa chỉ page A/B đang chứa record hợp lệ mới nhất. */
static uint32_t s_active_addr = 0U;

/* Generation của record active, dùng sinh generation cho lần commit kế. */
static uint32_t s_active_gen = 0U;

/* true sau khi Init thành công hoặc xác nhận trạng thái first boot. */
static bool s_initialized = false;

/* false ở first boot khi cả A/B còn trắng và chưa có record active. */
static bool s_has_record = false;


/*
 * true khi A mới hơn B theo serial-number arithmetic 32-bit.
 * Hai generation bằng nhau cũng chọn A để kết quả luôn ổn định.
 * Caller phải loại trường hợp chênh đúng 0x80000000 trước khi gọi.
 */
static bool BootMetadata_IsGenANewer(uint32_t gen_a, uint32_t gen_b){
    const uint32_t difference = gen_a - gen_b;
    return (difference == 0U) || (difference < 0x80000000UL);
}

static bool BootMetadata_IsPageErased(uint32_t page_address){
    const volatile uint8_t *page = (const volatile uint8_t *)(uintptr_t)page_address;

    for(uint32_t i = 0U ; i < METADATA_PAGE_SIZE; ++i){
        if(page[i] != 0xFFU){
            return false;
        }
    }

    return true;
}

/*
 * Được gọi trước khi ghi Commit marker vào 1 page
 * Check xem phần cứng ghi đúng chưa
 * Check dữ liệu đưa cho phần cứng có hợp lệ không
*/
static BootMetadata_Status_t BootMetadata_AtomicWriteAndVerify(
    uint32_t target_slot,
    const Metadata_Struct *metadata
){
    Flash_Status_t status;

    /* PHA 1: Ghi toàn bộ dữ liệu (TRỪ Commit Marker) */
    status = FlashProgram_WriteBuffer(target_slot, (const uint8_t *)metadata, METADATA_COMMIT_MARKER_OFFSET);
    if(status != FLASH_PROGRAM_OK) return BOOT_METADATA_WRITE_ERROR;

    /* PHA 2: Chỉ commit khi toàn bộ body đã đọc lại đúng. */
    status = FlashProgram_Verify(target_slot, (const uint8_t *)metadata, METADATA_COMMIT_MARKER_OFFSET);
    if(status != FLASH_PROGRAM_OK) return BOOT_METADATA_VERIFY_ERROR;

    /* PHA 3: Chốt cửa bằng Commit Marker
       Dùng biến trung gian để trình biên dịch tự xử lý Little-Endian an toàn */
    uint32_t marker = METADATA_COMMIT_MARKER_VALUE;
    status = FlashProgram_WriteBuffer(target_slot + METADATA_COMMIT_MARKER_OFFSET,(const uint8_t *)&marker, sizeof(marker));
    if(status != FLASH_PROGRAM_OK) return BOOT_METADATA_WRITE_ERROR;

    /* PHA 4: So khớp toàn record rồi validate CRC, marker và format. */
    status = FlashProgram_Verify(target_slot,(const uint8_t *)metadata,METADATA_RECORD_SIZE);
    if(status != FLASH_PROGRAM_OK) return BOOT_METADATA_VERIFY_ERROR;

    const Metadata_Struct *flashed_record = (const Metadata_Struct *)(uintptr_t)target_slot;
    if(!Metadata_IsValid(flashed_record)){
        return BOOT_METADATA_VERIFY_ERROR;
    }

    return BOOT_METADATA_OK;
}

BootMetadata_Status_t BootMetadata_Init(Metadata_Struct *out_metadata){
    s_active_addr = 0U;
    s_active_gen = 0U;
    s_initialized = false;
    s_has_record = false;

    if(out_metadata == NULL) return BOOT_METADATA_INVALID_ARGUMENT;

    const Metadata_Struct *slot_a = (const Metadata_Struct *)(uintptr_t)METADATA_PAGE_A_ADDRESS;
    const Metadata_Struct *slot_b = (const Metadata_Struct *)(uintptr_t)METADATA_PAGE_B_ADDRESS;

    bool a_valid = Metadata_IsValid(slot_a);
    bool b_valid = Metadata_IsValid(slot_b);

    if(a_valid && b_valid){
        /* 
         * Giải quyết bài toán tràn biến (Wrap-around) bằng Serial Number Arithmetic (RFC 1982):
         * - Vì generation tự ++ sau mỗi lần lưu, nó sẽ đạt max uint32_t và tràn về 0.
         * - Để so sánh 2 số khi có hiện tượng tràn, ta đặt chúng lên một "vòng tròn số".
         * - Nếu khoảng cách (generation_difference) bằng ĐÚNG 0x80000000 (tức 2^31, chính xác nửa vòng tròn),
         *   thì khoảng cách từ A đến B và từ B đến A là ĐỐI XỨNG TUYỆT ĐỐI. 
         */
        const uint32_t generation_difference = slot_a->generation - slot_b->generation;

        if(generation_difference == 0x80000000UL){
            return BOOT_METADATA_GENERATION_AMBIGUOUS;
        }

        if(BootMetadata_IsGenANewer(slot_a->generation, slot_b->generation)){
            s_active_addr = METADATA_PAGE_A_ADDRESS;
        } else {
            s_active_addr = METADATA_PAGE_B_ADDRESS;
        }

    } else if(a_valid){
        s_active_addr = METADATA_PAGE_A_ADDRESS;

    } else if(b_valid){
        s_active_addr = METADATA_PAGE_B_ADDRESS;
        
    } else {
        const bool a_erased = BootMetadata_IsPageErased(METADATA_PAGE_A_ADDRESS);
        const bool b_erased = BootMetadata_IsPageErased(METADATA_PAGE_B_ADDRESS);

        if(!a_erased || !b_erased){
            return BOOT_METADATA_CORRUPT;
        }

        if(!Metadata_InitializeDefault(out_metadata)){
            return BOOT_METADATA_INVALID_ARGUMENT;
        }

        /* Chưa có active record; lần commit đầu tiên sẽ ghi page A, generation 1. */
        s_initialized = true;
        return BOOT_METADATA_FIRST_BOOT;
    }

    /* Tải bản hợp lệ lên RAM */
    const Metadata_Struct *active_slot = (const Metadata_Struct *)(uintptr_t)s_active_addr;
    memcpy(out_metadata, active_slot, METADATA_RECORD_SIZE);

    s_active_gen = out_metadata->generation;
    s_has_record = true;
    s_initialized = true;
    return BOOT_METADATA_OK;
}

BootMetadata_Status_t BootMetadata_Write(Metadata_Struct *metadata){
    if(!s_initialized) return BOOT_METADATA_NOT_INITIALIZED;
    if(metadata == NULL) return BOOT_METADATA_INVALID_ARGUMENT;

    /* Nếu mạch mới thì chọn A, nếu mạch đang dùng page nào thì page kia đc chọn. */
    const uint32_t target_slot = !s_has_record ? METADATA_PAGE_A_ADDRESS :
        ((s_active_addr == METADATA_PAGE_A_ADDRESS) ? METADATA_PAGE_B_ADDRESS : METADATA_PAGE_A_ADDRESS);

    /*
     * Tạo ra 1 bản copy pending để ghi
     * Nếu lỡ mất điện sẽ không ảnh hưởng đến *metadata gốc
    */
    Metadata_Struct pending = *metadata;
    pending.magic = METADATA_MAGIC_VALUE;
    pending.format_version = METADATA_FORMAT_VERSION;
    pending.generation = s_has_record ? (s_active_gen + 1U) : 1U;
    pending.reserved = 0U;
    pending.commit_marker = METADATA_COMMIT_MARKER_VALUE;

    if(!Metadata_RefreshCrc32(&pending) || !Metadata_IsValid(&pending)){
        return BOOT_METADATA_INVALID_ARGUMENT;
    }

    /* Xóa dọn dẹp Slot mục tiêu */
    if(FlashProgram_ErasePage(target_slot) != FLASH_PROGRAM_OK){
        return BOOT_METADATA_WRITE_ERROR;
    }

    /* Ghi và Xác thực tận cùng */
    BootMetadata_Status_t status = BootMetadata_AtomicWriteAndVerify(target_slot, &pending);
    if(status != BOOT_METADATA_OK){
        /* Bỏ lại Slot bị hỏng, lần sau Boot lên hệ thống vẫn sẽ lấy Slot Active cũ! */
        return status;
    }

    /* Chỉ publish state mới và cập nhật caller sau khi commit hoàn tất. */
    s_active_addr = target_slot;
    s_active_gen = pending.generation;
    s_has_record = true;
    *metadata = pending;

    return BOOT_METADATA_OK;
}
