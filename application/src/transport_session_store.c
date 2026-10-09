#include "transport_session_store.h"

#include "crc32.h"
#include "ext_flash.h"
#include "session_journal.h"

#include <stddef.h>
#include <string.h>

static bool TransportSessionStore_IsErased(
    const SessionJournal_Record_t *record
){
    const uint8_t *bytes = (const uint8_t *)record;

    for(size_t index = 0U; index < sizeof(*record); ++index){
        if(bytes[index] != SESSION_JOURNAL_ERASED_BYTE){
            return false;
        }
    }
    return true;
}

static bool TransportSessionStore_IsCommitted(
    const SessionJournal_Record_t *record
){
    return (record->commit_marker ==
                SESSION_JOURNAL_COMMIT_MARKER_VALUE) &&
           (record->session_crc32 == Crc32_Compute(
                record->session_id,
                SESSION_JOURNAL_CRC_SIZE
            ));
}

TransportSessionStore_Status_t TransportSessionStore_Accept(
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE]
){
    SessionJournal_Record_t record;
    uint32_t first_erased_address = UINT32_MAX;
    bool nonzero = false;

    if(session_id == NULL){
        return TRANSPORT_SESSION_STORE_INVALID_ARGUMENT;
    }
    for(size_t index = 0U; index < CRYPTO_SESSION_ID_SIZE; ++index){
        nonzero = nonzero || (session_id[index] != 0U);
    }
    if(!nonzero){
        return TRANSPORT_SESSION_STORE_INVALID_ARGUMENT;
    }

    for(uint32_t index = 0U;
        index < SESSION_JOURNAL_RECORD_COUNT;
        ++index){
        const uint32_t address = W25Q32_SESSION_JOURNAL_BASE +
            (index * SESSION_JOURNAL_RECORD_SIZE);

        if(W25Q32_ReadBlock(
               address,
               (uint8_t *)&record,
               sizeof(record)
           ) != EXT_FLASH_OK){
            return TRANSPORT_SESSION_STORE_IO_ERROR;
        }
        if(TransportSessionStore_IsErased(&record)){
            if(first_erased_address == UINT32_MAX){
                first_erased_address = address;
            }
            continue;
        }
        if(record.commit_marker != SESSION_JOURNAL_COMMIT_MARKER_VALUE){
            /* Record mất điện giữa chừng: không reuse slot, tiếp tục scan. */
            continue;
        }
        if(!TransportSessionStore_IsCommitted(&record)){
            return TRANSPORT_SESSION_STORE_CORRUPT;
        }
        if(memcmp(record.session_id, session_id,
                  CRYPTO_SESSION_ID_SIZE) == 0){
            return TRANSPORT_SESSION_STORE_REUSED;
        }
    }
    if(first_erased_address == UINT32_MAX){
        return TRANSPORT_SESSION_STORE_FULL;
    }

    memset(&record, 0xFF, sizeof(record));
    memcpy(record.session_id, session_id, sizeof(record.session_id));
    record.session_crc32 = Crc32_Compute(
        record.session_id,
        SESSION_JOURNAL_CRC_SIZE
    );
    if(W25Q32_WriteBlock(
           first_erased_address,
           (const uint8_t *)&record,
           SESSION_JOURNAL_COMMIT_OFFSET
       ) != EXT_FLASH_OK){
        return TRANSPORT_SESSION_STORE_IO_ERROR;
    }
    const uint32_t commit_marker = SESSION_JOURNAL_COMMIT_MARKER_VALUE;
    if((W25Q32_WriteBlock(
            first_erased_address + SESSION_JOURNAL_COMMIT_OFFSET,
            (const uint8_t *)&commit_marker,
            sizeof(commit_marker)
        ) != EXT_FLASH_OK) ||
       (W25Q32_ReadBlock(
            first_erased_address,
            (uint8_t *)&record,
            sizeof(record)
        ) != EXT_FLASH_OK) ||
       !TransportSessionStore_IsCommitted(&record) ||
       (memcmp(record.session_id, session_id,
               CRYPTO_SESSION_ID_SIZE) != 0)){
        return TRANSPORT_SESSION_STORE_IO_ERROR;
    }
    return TRANSPORT_SESSION_STORE_OK;
}

TransportSessionStore_Status_t TransportSessionStore_EraseForProvisioning(void){
    for(uint32_t address = W25Q32_SESSION_JOURNAL_BASE;
        address < W25Q32_SESSION_JOURNAL_END;
        address += W25Q32_SECTOR_SIZE){
        if(W25Q32_SectorErase(address) != EXT_FLASH_OK){
            return TRANSPORT_SESSION_STORE_IO_ERROR;
        }
    }
    return TRANSPORT_SESSION_STORE_OK;
}

bool TransportSessionStore_AcceptCallback(
    void *context,
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE]
){
    (void)context;
    return TransportSessionStore_Accept(session_id) ==
        TRANSPORT_SESSION_STORE_OK;
}
