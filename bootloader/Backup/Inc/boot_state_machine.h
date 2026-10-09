#ifndef AEGIS_BOOT_STATE_MACHINE_H
#define AEGIS_BOOT_STATE_MACHINE_H

/*
=> Dựa vào trạng thái đó, Bootloader phải làm gì tiếp theo?
Nó chỉ nhận kết quả kiểm tra từ các module khác và đưa ra quyết định
*/

#include "stdint.h"
#include "stdbool.h"

#include "metadata_struct.h"

#define BOOT_MAX_TRIAL_ATTEMPTS 3U

typedef enum {
    BOOT_ACTION_START_APP = 0,  /* Application hợp lệ, cho phép boot. */
    BOOT_ACTION_INSTALL_UPDATE, /* Tiếp tục cài firmware từ Staging Area. */
    BOOT_ACTION_ROLLBACK,       /* Khôi phục Application từ Backup Slot. */
    BOOT_ACTION_RECOVERY        /* Không có phương án boot an toàn. */
} Boot_Action_t;

/* Result of loading the persistent Metadata A/B store. */
typedef enum {
    BOOT_INPUT_METADATA_VALID = 0U, /* A committed record was selected. */
    BOOT_INPUT_METADATA_FIRST_BOOT, /* Both Metadata pages are erased. */
    BOOT_INPUT_METADATA_CORRUPT      /* No trustworthy committed record exists. */
} Boot_MetadataStatus_t;

/* Kết hợp các điều kiện này để đưa ra quyết định cho Bootloader. */
typedef struct {
    /* Distinguishes provisioned metadata from FIRST_BOOT and corruption. */
    Boot_MetadataStatus_t metadata_status;

    /* Application trong Internal Flash đã được xác thực. */
    bool app_valid;

    /* Firmware trong W25Q32 Staging Area hợp lệ. */
    bool staging_valid;

    /* Firmware trong W25Q32 Backup Slot hợp lệ. */
    bool backup_valid;

    /* UPDATE_READY mailbox request is valid and bound to the candidate. */
    bool update_requested;

    /* Application hiện tại có đang ở chế độ trial không. */
    bool trial_boot;

    /* Số lần Application trial đã được thử boot. */
    uint8_t boot_attempt_count;

    /* Destructive Internal Flash operation persisted in metadata. */
    Metadata_OperationState_t operation_state;
} BootState_Input_t;

/**
 * @brief Chọn hành động tiếp theo của Bootloader.
 *
 * @param input Trạng thái metadata, Application, Staging và Backup.
 * @return Hành động cần thực hiện; NULL hoặc trạng thái sai trả Recovery.
 *
 * @note Hàm chỉ quyết định, không erase/program Flash và không jump vào App.
 */
Boot_Action_t BootState_Decide(const BootState_Input_t *input);


#endif
