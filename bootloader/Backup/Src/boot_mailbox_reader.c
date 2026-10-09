#include "boot_mailbox_reader.h"

#include "stm32f10x_bkp.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_rcc.h"

#include <stddef.h>

#define BOOT_MAILBOX_READER_REGISTER_COUNT 10U

static uint16_t BootMailboxReader_Register(uint32_t index){
    return (uint16_t)(BKP_DR1 + (uint16_t)(index * 4U));
}

static void BootMailboxReader_StoreLe16(uint8_t *data, uint16_t value){
    data[0] = (uint8_t)(value & 0xFFU);
    data[1] = (uint8_t)((value >> 8U) & 0xFFU);
}

static void BootMailboxReader_EnableAccess(void){
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
}

static void BootMailboxReader_DisableAccess(void){
    PWR_BackupAccessCmd(DISABLE);
}

static void BootMailboxReader_ReadRecord(BootMailbox_Record_t *record){
    uint8_t *bytes = (uint8_t *)record;

    for(uint32_t i = 0U; i < BOOT_MAILBOX_READER_REGISTER_COUNT; ++i){
        BootMailboxReader_StoreLe16(
            &bytes[i * 2U],
            BKP_ReadBackupRegister(BootMailboxReader_Register(i))
        );
    }
}

static bool BootMailboxReader_ClearMagic(void){
    BKP_WriteBackupRegister(BootMailboxReader_Register(0U), 0U);
    BKP_WriteBackupRegister(BootMailboxReader_Register(1U), 0U);

    return (BKP_ReadBackupRegister(BootMailboxReader_Register(0U)) == 0U) &&
           (BKP_ReadBackupRegister(BootMailboxReader_Register(1U)) == 0U);
}

BootMailboxReader_Status_t BootMailboxReader_Clear(void){
    BootMailboxReader_EnableAccess();
    const bool cleared = BootMailboxReader_ClearMagic();
    BootMailboxReader_DisableAccess();
    return cleared ? BOOT_MAILBOX_READER_OK :
                     BOOT_MAILBOX_READER_CLEAR_FAILED;
}

BootMailboxReader_Status_t BootMailboxReader_ReadAndConsume(
    BootMailbox_Record_t *record
){
    if(record == NULL){
        return BOOT_MAILBOX_READER_INVALID;
    }

    BootMailboxReader_EnableAccess();
    BootMailboxReader_ReadRecord(record);

    const bool empty = record->magic == 0U;
    const bool cleared = BootMailboxReader_ClearMagic();
    BootMailboxReader_DisableAccess();

    if(!cleared){
        return BOOT_MAILBOX_READER_CLEAR_FAILED;
    }

    if(empty){
        return BOOT_MAILBOX_READER_EMPTY;
    }

    return BootMailbox_IsValid(record) ?
               BOOT_MAILBOX_READER_OK :
               BOOT_MAILBOX_READER_INVALID;
}
