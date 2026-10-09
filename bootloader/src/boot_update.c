#include "boot_update.h"

#include "artifact_manifest.h"
#include "boot_mailbox_io.h"
#include "boot_metadata.h"
#include "boot_state_machine.h"
#include "crc32.h"
#include "device_keys.h"
#include "ext_flash.h"
#include "firmware_auth.h"
#include "flash_program.h"
#include "memory_map.h"
#include "watchdog.h"

#include "stm32f10x.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define BOOT_UPDATE_COPY_BUFFER_SIZE 128U

typedef struct{
    uint32_t base_address;
    uint32_t size;
} BootUpdate_ReaderContext_t;

static bool BootUpdate_W25Reader(
    void *context,
    uint32_t offset,
    uint8_t *buffer,
    uint32_t length
){
    const BootUpdate_ReaderContext_t *reader = context;

    if((reader == NULL) || (buffer == NULL) ||
       (offset > reader->size) || (length > (reader->size - offset))){
        return false;
    }
    Watchdog_Feed();
    return W25Q32_ReadBlock(reader->base_address + offset, buffer, length) ==
        EXT_FLASH_OK;
}

static bool BootUpdate_InternalReader(
    void *context,
    uint32_t offset,
    uint8_t *buffer,
    uint32_t length
){
    const BootUpdate_ReaderContext_t *reader = context;

    if((reader == NULL) || (buffer == NULL) ||
       (offset > reader->size) || (length > (reader->size - offset))){
        return false;
    }
    Watchdog_Feed();
    memcpy(buffer,
           (const void *)(uintptr_t)(reader->base_address + offset),
           length);
    return true;
}

static bool BootUpdate_StagingManifestIsValid(
    const Staging_Manifest_t *manifest
){
    return (manifest->magic == STAGING_MANIFEST_MAGIC_VALUE) &&
           (manifest->format_version == STAGING_MANIFEST_FORMAT_VERSION) &&
           (manifest->artifact_type == STAGING_ARTIFACT_TYPE_FULL) &&
           (manifest->operation_id != UINT64_C(0)) &&
           (manifest->data_size == manifest->image_header.image_size) &&
           (manifest->data_size > 0U) &&
           (manifest->data_size <= APPLICATION_SIZE) &&
           (manifest->header_commit_marker ==
                STAGING_HEADER_COMMIT_MARKER_VALUE) &&
           (manifest->artifact_valid_marker ==
                STAGING_ARTIFACT_VALID_MARKER_VALUE) &&
           (manifest->header_crc32 == Crc32_Compute(
                manifest,
                STAGING_MANIFEST_HEADER_CRC_SIZE
            ));
}

static bool BootUpdate_BackupManifestIsValid(
    const Backup_Manifest_t *manifest
){
    return (manifest->magic == BACKUP_MANIFEST_MAGIC_VALUE) &&
           (manifest->format_version == BACKUP_MANIFEST_FORMAT_VERSION) &&
           (manifest->operation_id != UINT64_C(0)) &&
           (manifest->image_header.image_size > 0U) &&
           (manifest->image_header.image_size <= APPLICATION_SIZE) &&
           (manifest->commit_marker == BACKUP_MANIFEST_COMMIT_MARKER_VALUE) &&
           (manifest->manifest_crc32 == Crc32_Compute(
                manifest,
                BACKUP_MANIFEST_CRC_SIZE
            ));
}

static bool BootUpdate_VerifyImage(
    const Firmware_SignedHeader_t *header,
    const uint8_t signature[FIRMWARE_SIGNATURE_SIZE],
    const uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE],
    FirmwareAuth_ReadCallback_t reader,
    BootUpdate_ReaderContext_t *reader_context
){
    return FirmwareAuth_Verify(
        header,
        signature,
        public_key,
        reader,
        reader_context
    ) == FIRMWARE_AUTH_OK;
}

static bool BootUpdate_LoadAndVerifyStaging(
    Staging_Manifest_t *manifest,
    const uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE]
){
    BootUpdate_ReaderContext_t reader = {
        .base_address = W25Q32_STAGING_IMAGE_BASE,
        .size = W25Q32_STAGING_IMAGE_SIZE
    };

    if(W25Q32_ReadBlock(
           W25Q32_STAGING_MANIFEST_BASE,
           (uint8_t *)manifest,
           sizeof(*manifest)
       ) != EXT_FLASH_OK || !BootUpdate_StagingManifestIsValid(manifest)){
        return false;
    }
    return BootUpdate_VerifyImage(
        &manifest->image_header,
        manifest->image_signature,
        public_key,
        BootUpdate_W25Reader,
        &reader
    );
}

static bool BootUpdate_LoadAndVerifyBackup(
    Backup_Manifest_t *manifest,
    const uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE]
){
    BootUpdate_ReaderContext_t reader = {
        .base_address = W25Q32_BACKUP_IMAGE_BASE,
        .size = W25Q32_BACKUP_IMAGE_SIZE
    };

    if(W25Q32_ReadBlock(
           W25Q32_BACKUP_MANIFEST_BASE,
           (uint8_t *)manifest,
           sizeof(*manifest)
       ) != EXT_FLASH_OK || !BootUpdate_BackupManifestIsValid(manifest)){
        return false;
    }
    return BootUpdate_VerifyImage(
        &manifest->image_header,
        manifest->image_signature,
        public_key,
        BootUpdate_W25Reader,
        &reader
    );
}

static bool BootUpdate_VerifyCurrentApplication(
    const Metadata_Record_t *metadata,
    const uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE]
){
    BootUpdate_ReaderContext_t reader = {
        .base_address = APPLICATION_BASE_ADDRESS,
        .size = APPLICATION_SIZE
    };

    if(metadata->active_header.image_size == 0U){
        return false;
    }
    return BootUpdate_VerifyImage(
        &metadata->active_header,
        metadata->active_signature,
        public_key,
        BootUpdate_InternalReader,
        &reader
    );
}

static bool BootUpdate_ProgramApplication(
    uint32_t source_address,
    uint32_t image_size
){
    uint8_t buffer[BOOT_UPDATE_COPY_BUFFER_SIZE];
    uint32_t erase_size;
    uint32_t offset = 0U;

    if((image_size == 0U) || (image_size > APPLICATION_SIZE)){
        return false;
    }
    erase_size = (image_size + FLASH_PAGE_SIZE - 1U) &
        ~(FLASH_PAGE_SIZE - 1U);
    for(uint32_t page = 0U; page < erase_size; page += FLASH_PAGE_SIZE){
        Watchdog_Feed();
        if(FlashProgram_ErasePage(APPLICATION_BASE_ADDRESS + page) !=
           FLASH_PROGRAM_OK){
            return false;
        }
    }
    while(offset < image_size){
        const uint32_t remaining = image_size - offset;
        const uint32_t chunk = remaining < sizeof(buffer) ?
            remaining : (uint32_t)sizeof(buffer);

        Watchdog_Feed();
        if(W25Q32_ReadBlock(source_address + offset, buffer, chunk) !=
           EXT_FLASH_OK){
            return false;
        }
        if(FlashProgram_WriteBuffer(
               APPLICATION_BASE_ADDRESS + offset,
               buffer,
               chunk
           ) != FLASH_PROGRAM_OK){
            return false;
        }
        offset += chunk;
    }
    return true;
}

static bool BootUpdate_CommitOperation(
    BootMetadata_Context_t *context,
    Metadata_Record_t *metadata,
    Metadata_OperationState_t state,
    uint64_t operation_id
){
    metadata->operation_state = (uint8_t)state;
    metadata->operation_id = operation_id;
    return BootMetadata_Commit(context, metadata) == BOOT_METADATA_OK;
}

static bool BootUpdate_Install(
    BootMetadata_Context_t *context,
    Metadata_Record_t *metadata,
    const Staging_Manifest_t *staging,
    const uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE]
){
    /* Rollback có policy riêng; install mới luôn phải tăng version. */
    if(staging->image_header.image_version <=
       metadata->active_header.image_version){
        return false;
    }
    if(metadata->operation_state == METADATA_OPERATION_IDLE){
        if(!BootUpdate_CommitOperation(
            context,
            metadata,
            METADATA_OPERATION_INSTALL_IN_PROGRESS,
            staging->operation_id
        )){
            return false;
        }
    }
    if((metadata->operation_id != staging->operation_id) ||
       !BootUpdate_ProgramApplication(
           W25Q32_STAGING_IMAGE_BASE,
           staging->image_header.image_size
       )){
        return false;
    }

    metadata->active_header = staging->image_header;
    memcpy(metadata->active_signature,
           staging->image_signature,
           sizeof(metadata->active_signature));
    metadata->operation_state = METADATA_OPERATION_IDLE;
    metadata->trial_boot_flag = METADATA_TRIAL_BOOT_TRUE;
    metadata->boot_attempt_count = 1U;
    if(BootMetadata_Commit(context, metadata) != BOOT_METADATA_OK){
        return false;
    }
    return BootUpdate_VerifyCurrentApplication(metadata, public_key);
}

static bool BootUpdate_Rollback(
    BootMetadata_Context_t *context,
    Metadata_Record_t *metadata,
    const Backup_Manifest_t *backup,
    const uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE]
){
    if(metadata->operation_state != METADATA_OPERATION_ROLLBACK_IN_PROGRESS){
        if(!BootUpdate_CommitOperation(
            context,
            metadata,
            METADATA_OPERATION_ROLLBACK_IN_PROGRESS,
            backup->operation_id
        )){
            return false;
        }
    }
    if((metadata->operation_id != backup->operation_id) ||
       !BootUpdate_ProgramApplication(
           W25Q32_BACKUP_IMAGE_BASE,
           backup->image_header.image_size
       )){
        return false;
    }
    metadata->active_header = backup->image_header;
    memcpy(metadata->active_signature,
           backup->image_signature,
           sizeof(metadata->active_signature));
    metadata->operation_state = METADATA_OPERATION_IDLE;
    metadata->trial_boot_flag = METADATA_TRIAL_BOOT_FALSE;
    metadata->boot_attempt_count = 0U;
    metadata->operation_id = UINT64_C(0);
    if(BootMetadata_Commit(context, metadata) != BOOT_METADATA_OK){
        return false;
    }
    return BootUpdate_VerifyCurrentApplication(metadata, public_key);
}

static bool BootUpdate_JumpToApplication(void){
    const uint32_t app_stack = *(const uint32_t *)(uintptr_t)
        APPLICATION_BASE_ADDRESS;
    const uint32_t app_reset = *(const uint32_t *)(uintptr_t)
        (APPLICATION_BASE_ADDRESS + 4U);
    typedef void (*EntryPoint_t)(void);

    if((app_stack < SRAM_START_ADDRESS) || (app_stack > SRAM_END_ADDRESS) ||
       ((app_stack & 7U) != 0U) ||
       (app_reset < APPLICATION_BASE_ADDRESS) ||
       (app_reset >= APPLICATION_END_ADDRESS) ||
       ((app_reset & 1U) == 0U)){
        return false;
    }

    __disable_irq();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    SCB->VTOR = APPLICATION_BASE_ADDRESS;
    __asm volatile ("msr msp, %0" : : "r" (app_stack) : );
    ((EntryPoint_t)(uintptr_t)app_reset)();
    return false;
}

BootUpdate_Status_t BootUpdate_Run(void){
    BootMetadata_Context_t metadata_context;
    Metadata_Record_t metadata;
    Staging_Manifest_t staging;
    Backup_Manifest_t backup;
    BootMailbox_Record_t mailbox;
    BootState_Input_t input;
    uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE];
    BootMetadata_Status_t metadata_status;
    bool staging_valid = false;
    bool backup_valid = false;
    bool update_requested = false;
    bool mailbox_valid;

    /* IWDG có thể vẫn chạy sau reset do candidate treo. */
    Watchdog_Feed();
    if(!DeviceKeys_GetReleasePublicKey(public_key)){
        return BOOT_UPDATE_RECOVERY;
    }
    metadata_status = BootMetadata_Init(&metadata_context, &metadata);
    if((metadata_status != BOOT_METADATA_OK) &&
       (metadata_status != BOOT_METADATA_FIRST_BOOT)){
        return BOOT_UPDATE_METADATA_ERROR;
    }
    mailbox_valid = BootMailboxIo_Consume(&mailbox) == BOOT_MAILBOX_IO_OK;

    if(W25Q32_Init() == EXT_FLASH_OK){
        staging_valid = BootUpdate_LoadAndVerifyStaging(&staging, public_key);
        backup_valid = BootUpdate_LoadAndVerifyBackup(&backup, public_key);
    }
    if(metadata_status == BOOT_METADATA_OK && mailbox_valid &&
       (mailbox.command == BOOT_MAILBOX_COMMAND_TRIAL_CONFIRM) &&
       (metadata.trial_boot_flag == METADATA_TRIAL_BOOT_TRUE) &&
       (mailbox.operation_id == metadata.operation_id)){
        metadata.trial_boot_flag = METADATA_TRIAL_BOOT_FALSE;
        metadata.boot_attempt_count = 0U;
        metadata.operation_state = METADATA_OPERATION_IDLE;
        metadata.operation_id = UINT64_C(0);
        if(BootMetadata_Commit(&metadata_context, &metadata) !=
           BOOT_METADATA_OK){
            return BOOT_UPDATE_METADATA_ERROR;
        }
    }
    if(mailbox_valid &&
       (mailbox.command == BOOT_MAILBOX_COMMAND_UPDATE_READY) &&
       staging_valid && backup_valid &&
       (mailbox.operation_id == staging.operation_id) &&
       (mailbox.operation_id == backup.operation_id)){
        update_requested = true;
    }

    memset(&input, 0, sizeof(input));
    input.metadata_status = metadata_status == BOOT_METADATA_OK ?
        BOOT_INPUT_METADATA_VALID : BOOT_INPUT_METADATA_FIRST_BOOT;
    input.operation_state = (Metadata_OperationState_t)
        metadata.operation_state;
    input.app_valid = metadata_status == BOOT_METADATA_OK &&
        BootUpdate_VerifyCurrentApplication(&metadata, public_key);
    input.staging_valid = staging_valid &&
        (metadata_status == BOOT_METADATA_OK) &&
        (staging.image_header.image_version >
            metadata.active_header.image_version) &&
        ((metadata.operation_state != METADATA_OPERATION_INSTALL_IN_PROGRESS) ||
         (metadata.operation_id == staging.operation_id));
    input.backup_valid = backup_valid &&
        ((metadata.operation_state == METADATA_OPERATION_IDLE) ||
         (metadata.operation_id == backup.operation_id));
    input.update_requested = update_requested;
    input.trial_boot = metadata.trial_boot_flag == METADATA_TRIAL_BOOT_TRUE;
    input.boot_attempt_count = metadata.boot_attempt_count;

    switch(BootState_Decide(&input)){
        case BOOT_ACTION_INSTALL_UPDATE:
            if(!BootUpdate_Install(
                &metadata_context, &metadata, &staging, public_key
            )){
                return BOOT_UPDATE_FLASH_ERROR;
            }
            (void)BootUpdate_JumpToApplication();
            return BOOT_UPDATE_VERIFY_ERROR;

        case BOOT_ACTION_ROLLBACK:
            if(!BootUpdate_Rollback(
                &metadata_context, &metadata, &backup, public_key
            )){
                return BOOT_UPDATE_FLASH_ERROR;
            }
            (void)BootUpdate_JumpToApplication();
            return BOOT_UPDATE_VERIFY_ERROR;

        case BOOT_ACTION_START_APP:
            if(metadata.trial_boot_flag == METADATA_TRIAL_BOOT_TRUE){
                if(metadata.boot_attempt_count < UINT8_MAX){
                    ++metadata.boot_attempt_count;
                }
                if(BootMetadata_Commit(&metadata_context, &metadata) !=
                   BOOT_METADATA_OK){
                    return BOOT_UPDATE_METADATA_ERROR;
                }
            }
            (void)BootUpdate_JumpToApplication();
            return BOOT_UPDATE_VERIFY_ERROR;

        case BOOT_ACTION_RECOVERY:
        default:
            return BOOT_UPDATE_RECOVERY;
    }
}
