#include "boot_state_machine.h"

#include <stddef.h>

Boot_Action_t BootState_Decide(const BootState_Input_t *input){
    if(input == NULL){
        return BOOT_ACTION_RECOVERY;
    }   

    /*
     * FIRST_BOOT chưa được provision, Metadata corrupt hoặc status lạ
     * Đều không được tin các field còn lại
     */
    if(input->metadata_status != BOOT_INPUT_METADATA_VALID){
        return BOOT_ACTION_RECOVERY;
    }

    /*
     * Rollback đã được commit nhưng bị reset giữa chừng.
     * Chỉ được tiếp tục nếu Backup vẫn hợp lệ.
     */
    if(input->operation_state == METADATA_OPERATION_ROLLBACK_IN_PROGRESS){
        if(input->backup_valid){
            return BOOT_ACTION_ROLLBACK;
        }

        return BOOT_ACTION_RECOVERY;
    }

    /*
     * Install đã được commit nhưng bị reset giữa chừng.
     *
     * - Staging và Backup tốt: tiếp tục install.
     * - Staging hỏng nhưng Backup tốt: quay về firmware cũ.
     * - Backup hỏng: không còn đường khôi phục an toàn.
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
     * Enum state ngoài contract persistent phải fail-closed.
     */
    if(input->operation_state != METADATA_OPERATION_IDLE){
        return BOOT_ACTION_RECOVERY;
    }

    /*
     * App hiện tại hỏng thì ưu tiên khôi phục Backup.
     */
    if(!input->app_valid){
        if(input->backup_valid){
            return BOOT_ACTION_ROLLBACK;
        }

        return BOOT_ACTION_RECOVERY;
    }

    /*
     * Candidate đã dùng hết số lần Trial Boot.
     */
    if(input->trial_boot && (input->boot_attempt_count >= BOOT_MAX_TRIAL_ATTEMPTS)){
        if(input->backup_valid){
            return BOOT_ACTION_ROLLBACK;
        }

        return BOOT_ACTION_RECOVERY;
    }

    /*
     * Candidate còn lượt Trial Boot.
     * Không nhận update mới khi trial hiện tại chưa được giải quyết.
     */
    if(input->trial_boot){
        return BOOT_ACTION_START_APP;
    }

    /*
     * Update mới chỉ được cài khi:
     * - Có request hợp lệ.
     * - Staging hợp lệ.
     * - Backup hợp lệ.
     */
    if(input->update_requested &&
       input->staging_valid &&
       input->backup_valid
    ){
        return BOOT_ACTION_INSTALL_UPDATE;
    }

    /*
     * App hợp lệ và không có công việc ưu tiên nào khác.
     */
    return BOOT_ACTION_START_APP;

}
