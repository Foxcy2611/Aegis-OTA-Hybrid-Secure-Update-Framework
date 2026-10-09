#ifndef AEGIS_BOOT_MAILBOX_IO_H
#define AEGIS_BOOT_MAILBOX_IO_H

/* Đọc/ghi/consume Boot mailbox trong 10 BKP register của STM32F103. */

#include "boot_mailbox.h"

#include <stdbool.h>

typedef enum{
    BOOT_MAILBOX_IO_OK = 0U,
    BOOT_MAILBOX_IO_EMPTY,
    BOOT_MAILBOX_IO_INVALID_ARGUMENT,
    BOOT_MAILBOX_IO_INVALID_RECORD,
    BOOT_MAILBOX_IO_VERIFY_FAILED
} BootMailboxIo_Status_t;

BootMailboxIo_Status_t BootMailboxIo_Write(
    BootMailbox_Command_t command,
    uint64_t operation_id
);

/* Consume luôn clear magic trước khi trả record, tránh xử lý lặp sau reset. */
BootMailboxIo_Status_t BootMailboxIo_Consume(BootMailbox_Record_t *record);
void BootMailboxIo_Clear(void);

#endif /* AEGIS_BOOT_MAILBOX_IO_H */
