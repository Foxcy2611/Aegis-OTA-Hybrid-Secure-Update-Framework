#ifndef AEGIS_SESSION_JOURNAL_H
#define AEGIS_SESSION_JOURNAL_H

/* Record bền vững chống mở lại secure session cũ sau reset. */

#include "crypto_config.h"
#include "memory_map.h"

#include <stddef.h>
#include <stdint.h>

#define SESSION_JOURNAL_COMMIT_MARKER_VALUE    UINT32_C(0x53455331)
#define SESSION_JOURNAL_ERASED_BYTE            UINT8_C(0xFF)

typedef struct __attribute__((packed)){
    uint8_t session_id[CRYPTO_SESSION_ID_SIZE];
    /* Byte [0..7]: session ID ngẫu nhiên do ESP32 tạo. */

    uint32_t session_crc32;
    /* Byte [8..11]: CRC32 của session_id để phát hiện record hỏng. */

    uint32_t commit_marker;
    /* Byte [12..15]: ghi cuối để công bố ID đã được sử dụng. */
} SessionJournal_Record_t;

#define SESSION_JOURNAL_CRC_SIZE               \
    ((uint32_t)offsetof(SessionJournal_Record_t, session_crc32))
#define SESSION_JOURNAL_COMMIT_OFFSET           \
    ((uint32_t)offsetof(SessionJournal_Record_t, commit_marker))
#define SESSION_JOURNAL_RECORD_SIZE             \
    ((uint32_t)sizeof(SessionJournal_Record_t))
#define SESSION_JOURNAL_RECORD_COUNT            \
    (W25Q32_SESSION_JOURNAL_SIZE / SESSION_JOURNAL_RECORD_SIZE)

_Static_assert(sizeof(SessionJournal_Record_t) == 16U,
               "Session journal record phải đúng 16 byte");
_Static_assert((W25Q32_SESSION_JOURNAL_SIZE %
                SESSION_JOURNAL_RECORD_SIZE) == 0U,
               "Session journal phải chứa số record nguyên vẹn");
_Static_assert((W25Q32_PAGE_SIZE % SESSION_JOURNAL_RECORD_SIZE) == 0U,
               "Session record không được đi qua biên page");

#endif /* AEGIS_SESSION_JOURNAL_H */
