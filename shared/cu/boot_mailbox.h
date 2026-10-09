#ifndef AEGIS_BOOT_MAILBOX_H
#define AEGIS_BOOT_MAILBOX_H

/* Contract request từ Application sang Bootloader tồn tại qua software reset. */

#include <stddef.h>
#include <stdint.h>

#define BOOT_MAILBOX_MAGIC_VALUE          UINT32_C(0x41474D42)
#define BOOT_MAILBOX_RESERVED_VALUE       0U

typedef enum {
    BOOT_MAILBOX_COMMAND_NONE = 0U,
    BOOT_MAILBOX_COMMAND_UPDATE_READY = 1U,
    BOOT_MAILBOX_COMMAND_TRIAL_CONFIRM = 2U
} BootMailbox_Command_t;

typedef struct __attribute__((packed)){
    uint32_t magic;          /* Vị trí 0: giá trị commit, được ghi cuối. */
    uint16_t command;        /* Vị trí 4: giá trị BootMailbox_Command_t. */
    uint16_t reserved;       /* Vị trí 6: phải bằng 0. */
    uint64_t operation_id;   /* Vị trí 8: nhận diện update hoặc trial candidate. */
    uint32_t crc32;          /* Vị trí 16: CRC bao phủ các byte [0, 16). */
} BootMailbox_Record_t;

#define BOOT_MAILBOX_CRC_REGION_SIZE ((uint32_t)offsetof(BootMailbox_Record_t, crc32))

_Static_assert(offsetof(BootMailbox_Record_t, command) == 4U,
               "Sai vị trí command trong mailbox");
_Static_assert(offsetof(BootMailbox_Record_t, operation_id) == 8U,
               "Sai vị trí operation ID trong mailbox");
_Static_assert(offsetof(BootMailbox_Record_t, crc32) == 16U,
               "Sai vị trí CRC trong mailbox");
_Static_assert(sizeof(BootMailbox_Record_t) == 20U,
               "Mailbox phải nằm gọn trong mười backup register 16-bit");

#endif /* AEGIS_BOOT_MAILBOX_H */
