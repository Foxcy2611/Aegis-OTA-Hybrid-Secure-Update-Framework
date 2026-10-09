#ifndef AEGIS_BOOT_MAILBOX_H
#define AEGIS_BOOT_MAILBOX_H

/* ABI 20 byte qua BKP register giữa Application và Bootloader. */

#include <stddef.h>
#include <stdint.h>

#define BOOT_MAILBOX_MAGIC_VALUE                UINT32_C(0x41474D42)
#define BOOT_MAILBOX_RESERVED_VALUE             UINT16_C(0)

typedef enum{
    BOOT_MAILBOX_COMMAND_NONE = 0U,
    BOOT_MAILBOX_COMMAND_UPDATE_READY = 1U,
    BOOT_MAILBOX_COMMAND_TRIAL_CONFIRM = 2U
} BootMailbox_Command_t;

typedef struct __attribute__((packed)){
    uint32_t magic;          /* Byte 0: commit marker, ghi cuối. */
    uint16_t command;        /* Byte 4: BootMailbox_Command_t. */
    uint16_t reserved;       /* Byte 6: luôn bằng 0. */
    uint64_t operation_id;   /* Byte 8: bind đúng update/candidate. */
    uint32_t crc32;          /* Byte 16: CRC của byte [0, 16). */
} BootMailbox_Record_t;

#define BOOT_MAILBOX_CRC_REGION_SIZE            \
    ((uint32_t)offsetof(BootMailbox_Record_t, crc32))

_Static_assert(offsetof(BootMailbox_Record_t, command) == 4U,
               "Sai offset command mailbox");
_Static_assert(offsetof(BootMailbox_Record_t, operation_id) == 8U,
               "Sai offset operation_id mailbox");
_Static_assert(offsetof(BootMailbox_Record_t, crc32) == 16U,
               "Sai offset CRC mailbox");
_Static_assert(sizeof(BootMailbox_Record_t) == 20U,
               "Mailbox phải dùng đúng 10 BKP register");

#endif /* AEGIS_BOOT_MAILBOX_H */
