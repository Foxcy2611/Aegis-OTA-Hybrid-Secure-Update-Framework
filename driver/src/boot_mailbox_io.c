#include "boot_mailbox_io.h"

#include "crc32.h"

#include "stm32f10x_bkp.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_rcc.h"

#include <string.h>

#define BOOT_MAILBOX_REGISTER_COUNT 10U

static uint16_t BootMailboxIo_Register(uint32_t index){
    return (uint16_t)(BKP_DR1 + (index * 4U));
}

static void BootMailboxIo_EnableWrite(void){
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
}

static void BootMailboxIo_ReadRaw(uint8_t bytes[sizeof(BootMailbox_Record_t)]){
    for(uint32_t index = 0U; index < BOOT_MAILBOX_REGISTER_COUNT; ++index){
        const uint16_t value = BKP_ReadBackupRegister(
            BootMailboxIo_Register(index)
        );
        bytes[index * 2U] = (uint8_t)value;
        bytes[index * 2U + 1U] = (uint8_t)(value >> 8U);
    }
}

static void BootMailboxIo_WriteHalfWord(uint32_t index, const uint8_t *bytes){
    const uint16_t value = (uint16_t)bytes[index * 2U] |
        (uint16_t)((uint16_t)bytes[index * 2U + 1U] << 8U);

    BKP_WriteBackupRegister(BootMailboxIo_Register(index), value);
}

static bool BootMailboxIo_CommandIsValid(uint16_t command){
    return (command == BOOT_MAILBOX_COMMAND_UPDATE_READY) ||
           (command == BOOT_MAILBOX_COMMAND_TRIAL_CONFIRM);
}

void BootMailboxIo_Clear(void){
    BootMailboxIo_EnableWrite();
    BKP_WriteBackupRegister(BKP_DR1, 0U);
    BKP_WriteBackupRegister(BKP_DR2, 0U);
}

BootMailboxIo_Status_t BootMailboxIo_Write(
    BootMailbox_Command_t command,
    uint64_t operation_id
){
    BootMailbox_Record_t record;
    uint8_t bytes[sizeof(record)];
    uint8_t verify[sizeof(record)];

    if(!BootMailboxIo_CommandIsValid((uint16_t)command) ||
       (operation_id == UINT64_C(0))){
        return BOOT_MAILBOX_IO_INVALID_ARGUMENT;
    }
    memset(&record, 0, sizeof(record));
    record.magic = BOOT_MAILBOX_MAGIC_VALUE;
    record.command = (uint16_t)command;
    record.reserved = BOOT_MAILBOX_RESERVED_VALUE;
    record.operation_id = operation_id;
    record.crc32 = Crc32_Compute(&record, BOOT_MAILBOX_CRC_REGION_SIZE);
    memcpy(bytes, &record, sizeof(bytes));

    BootMailboxIo_EnableWrite();
    BootMailboxIo_Clear();
    /* Body + CRC trước; hai half-word magic được commit cuối cùng. */
    for(uint32_t index = 2U; index < BOOT_MAILBOX_REGISTER_COUNT; ++index){
        BootMailboxIo_WriteHalfWord(index, bytes);
    }
    BootMailboxIo_WriteHalfWord(0U, bytes);
    BootMailboxIo_WriteHalfWord(1U, bytes);

    BootMailboxIo_ReadRaw(verify);
    return memcmp(bytes, verify, sizeof(bytes)) == 0 ?
        BOOT_MAILBOX_IO_OK : BOOT_MAILBOX_IO_VERIFY_FAILED;
}

BootMailboxIo_Status_t BootMailboxIo_Consume(BootMailbox_Record_t *record){
    uint8_t bytes[sizeof(BootMailbox_Record_t)];

    if(record == NULL){
        return BOOT_MAILBOX_IO_INVALID_ARGUMENT;
    }
    BootMailboxIo_EnableWrite();
    BootMailboxIo_ReadRaw(bytes);
    memcpy(record, bytes, sizeof(*record));
    BootMailboxIo_Clear();

    if(record->magic != BOOT_MAILBOX_MAGIC_VALUE){
        return BOOT_MAILBOX_IO_EMPTY;
    }
    if(!BootMailboxIo_CommandIsValid(record->command) ||
       (record->reserved != BOOT_MAILBOX_RESERVED_VALUE) ||
       (record->operation_id == UINT64_C(0)) ||
       (record->crc32 != Crc32_Compute(
           record,
           BOOT_MAILBOX_CRC_REGION_SIZE
       ))){
        return BOOT_MAILBOX_IO_INVALID_RECORD;
    }
    return BOOT_MAILBOX_IO_OK;
}
