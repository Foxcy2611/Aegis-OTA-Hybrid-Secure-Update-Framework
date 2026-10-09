#ifndef AEGIS_BOOT_MAILBOX_READER_H
#define AEGIS_BOOT_MAILBOX_READER_H

/* Bootloader-owned reader/consumer for the reset-surviving BKP mailbox. */

#include "boot_mailbox.h"

typedef enum {
    BOOT_MAILBOX_READER_OK = 0U,          /* Valid request returned and consumed. */
    BOOT_MAILBOX_READER_EMPTY,            /* No committed request is present. */
    BOOT_MAILBOX_READER_INVALID,          /* A committed-looking record is invalid. */
    BOOT_MAILBOX_READER_CLEAR_FAILED      /* Magic could not be cleared. */
} BootMailboxReader_Status_t;

BootMailboxReader_Status_t BootMailboxReader_ReadAndConsume(
    BootMailbox_Record_t *record
);
BootMailboxReader_Status_t BootMailboxReader_Clear(void);

#endif /* AEGIS_BOOT_MAILBOX_READER_H */
