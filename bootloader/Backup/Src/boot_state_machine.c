#include "boot_state_machine.h"

#include <stddef.h>

Boot_Action_t BootState_Decide(const BootState_Input_t *input){
    if(input == NULL){
        return BOOT_ACTION_RECOVERY;
    }

    /* Không tin các field khác nếu metadata chưa được xác thực. */
    if(input->metadata_status != BOOT_INPUT_METADATA_VALID){
        return BOOT_ACTION_RECOVERY;
    }

    /*
     * Lần cài trước bị mất điện.
     * Nếu Staging còn tốt thì tiếp tục cài; nếu không thì thử rollback.
     */
    if(input->operation_state == METADATA_OPERATION_INSTALL_IN_PROGRESS){
        if(input->staging_valid && input->backup_valid){
            return BOOT_ACTION_INSTALL_UPDATE;
        }

        if(input->backup_valid){
            return BOOT_ACTION_ROLLBACK;
        }

        return BOOT_ACTION_RECOVERY;
    }

    /*
     * Rollback trước đó bị mất điện.
     * Chỉ có thể tiếp tục nếu Backup vẫn hợp lệ.
     */
    if(input->operation_state == METADATA_OPERATION_ROLLBACK_IN_PROGRESS){
        return input->backup_valid ? BOOT_ACTION_ROLLBACK : BOOT_ACTION_RECOVERY;
    }

    /* Unknown persistent operation state is not trusted. */
    if(input->operation_state != METADATA_OPERATION_IDLE){
        return BOOT_ACTION_RECOVERY;
    }

    /*
     * Application hỏng: ưu tiên khôi phục Backup.
     * Không có Backup thì vào Recovery.
     */
    if(!input->app_valid){
        return input->backup_valid ?
                   BOOT_ACTION_ROLLBACK :
                   BOOT_ACTION_RECOVERY;
    }

    /*
     * Firmware trial đã thất bại quá số lần cho phép.
     * Không được boot lại Application đó.
     */
    if(input->trial_boot && (input->boot_attempt_count >= BOOT_MAX_TRIAL_ATTEMPTS)){
        return input->backup_valid ? BOOT_ACTION_ROLLBACK : BOOT_ACTION_RECOVERY;
    }

    /* A trial candidate must be resolved before accepting another update. */
    if(input->trial_boot){
        return BOOT_ACTION_START_APP;
    }

    /* A fresh request never authorizes install without both committed artifacts. */
    if(input->update_requested && input->staging_valid && input->backup_valid){
        return BOOT_ACTION_INSTALL_UPDATE;
    }

    /* Application hợp lệ và không có transaction cần khôi phục. */
    return BOOT_ACTION_START_APP;
}
