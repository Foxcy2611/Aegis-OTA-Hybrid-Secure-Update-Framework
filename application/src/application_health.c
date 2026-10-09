#include "application_health.h"

#include "active_firmware.h"
#include "boot_mailbox_io.h"
#include "watchdog.h"

#include "stm32f10x.h"

bool ApplicationHealth_BeginTrialWatchdog(void){
    Metadata_Record_t metadata;

    if(!ActiveFirmware_LoadMetadata(&metadata)){
        return false;
    }
    if(metadata.trial_boot_flag != METADATA_TRIAL_BOOT_TRUE){
        return true;
    }
    return Watchdog_StartMaxTimeout() == WATCHDOG_OK;
}

bool ApplicationHealth_ConfirmTrialIfNeeded(void){
    Metadata_Record_t metadata;

    if(!ActiveFirmware_LoadMetadata(&metadata)){
        return false;
    }
    if(metadata.trial_boot_flag != METADATA_TRIAL_BOOT_TRUE){
        return true;
    }
    if(BootMailboxIo_Write(
           BOOT_MAILBOX_COMMAND_TRIAL_CONFIRM,
           metadata.operation_id
       ) != BOOT_MAILBOX_IO_OK){
        return false;
    }
    NVIC_SystemReset();
    return false;
}
