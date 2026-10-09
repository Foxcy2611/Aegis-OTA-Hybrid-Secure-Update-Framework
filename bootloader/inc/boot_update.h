#ifndef AEGIS_BOOT_UPDATE_H
#define AEGIS_BOOT_UPDATE_H

/* Orchestrator Bootloader: verify, decide, install/rollback, trial và jump. */

typedef enum{
    BOOT_UPDATE_STARTED_APP = 0U,
    BOOT_UPDATE_RECOVERY,
    BOOT_UPDATE_METADATA_ERROR,
    BOOT_UPDATE_STORAGE_ERROR,
    BOOT_UPDATE_FLASH_ERROR,
    BOOT_UPDATE_VERIFY_ERROR
} BootUpdate_Status_t;

/* Hàm chỉ return khi không thể jump an toàn và caller phải ở Recovery. */
BootUpdate_Status_t BootUpdate_Run(void);

#endif /* AEGIS_BOOT_UPDATE_H */
