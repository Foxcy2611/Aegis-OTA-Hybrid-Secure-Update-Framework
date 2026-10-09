#ifndef AEGIS_BOOT_STATE_MACHINE_H
#define AEGIS_BOOT_STATE_MACHINE_H

/*
 * Hàm quyết định thuần của Bootloader.
 *
 * Module này chịu trách nhiệm:
 * - Nhận các kết quả đã được module khác kiểm chứng.
 * - Áp dụng thứ tự ưu tiên cố định.
 * - Trả về đúng một Boot_Action_t.
 *
 * Module này không:
 * - Đọc hoặc ghi Flash.
 * - Kiểm tra CRC/chữ ký firmware.
 * - Đọc mailbox hoặc W25Q32.
 * - Install, rollback hoặc jump vào Application.
 */

#include "metadata_struct.h"

#include <stdbool.h>
#include <stdint.h>

#define BOOT_MAX_TRIAL_ATTEMPTS UINT8_C(3)

/*
 * Hành động mà tầng executor của Bootloader phải thực hiện.
 *
 * RECOVERY đặt bằng 0 để giá trị mặc định luôn fail-closed.
 */
typedef enum {
    BOOT_ACTION_RECOVERY = 0U,
    /* Không có phương án boot an toàn; đi vào recovery policy. */

    BOOT_ACTION_START_APP,
    /* Application hợp lệ và được phép chạy. */

    BOOT_ACTION_INSTALL_UPDATE,
    /* Cài hoặc tiếp tục cài firmware từ Staging. */

    BOOT_ACTION_ROLLBACK
    /* Khôi phục hoặc tiếp tục khôi phục firmware từ Backup. */
} Boot_Action_t;

/*
 * Kết quả đọc Metadata A/B đã được rút gọn cho state machine.
 */
typedef enum{
    BOOT_INPUT_METADATA_VALID = 0U,
    /* BootMetadata_Init đã chọn được một record hợp lệ. */

    BOOT_INPUT_METADATA_FIRST_BOOT,
    /* Cả hai Metadata page còn trắng và thiết bị chưa cung cấp. */

    BOOT_INPUT_METADATA_CORRUPT
    /* Không tìm được Metadata record đáng tin cậy. */
} Boot_InputMetadataStatus_t;

/*
 * Toàn bộ điều kiện cần thiết để đưa ra boot action.
 *
 * Struct này chỉ tồn tại trong RAM và không phải persistent contract.
 */
typedef struct{
    Boot_InputMetadataStatus_t metadata_status;
    /* Trạng thái kết quả đọc Metadata A/B. */

    Metadata_OperationState_t operation_state;
    /* Thao tác Internal Flash đang dở: IDLE, INSTALL hoặc ROLLBACK. */

    bool app_valid;
    /* Application hiện tại đã vượt qua toàn bộ bước xác thực. */

    bool staging_valid;
    /* Staging manifest và firmware đã được xác thực. */

    bool backup_valid;
    /* Backup manifest và firmware đã được xác thực. */

    bool update_requested;
    /* Có mailbox UPDATE_READY hợp lệ và bind đúng candidate. */

    bool trial_boot;
    /* Application hiện tại là candidate chưa được confirm. */

    uint8_t boot_attempt_count;
    /* Số lần candidate đã được cho phép Trial Boot. */
} BootState_Input_t;

/*
 * Áp dụng boot policy để chọn hành động tiếp theo.
 *
 * Hàm không thay đổi input và không truy cập hardware.
 * NULL hoặc trạng thái không đáng tin cậy luôn trả RECOVERY.
 */
Boot_Action_t BootState_Decide(const BootState_Input_t *input);

#endif /* AEGIS_BOOT_STATE_MACHINE_H */