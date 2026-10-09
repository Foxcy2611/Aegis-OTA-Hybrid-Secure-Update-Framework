#ifndef AEGIS_TRANSPORT_SESSION_STORE_H
#define AEGIS_TRANSPORT_SESSION_STORE_H

/* Append-only W25 journal: mỗi session ID chỉ được mở đúng một lần. */

#include "crypto_config.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum{
    TRANSPORT_SESSION_STORE_OK = 0U,
    /* Session ID mới đã được commit. */

    TRANSPORT_SESSION_STORE_INVALID_ARGUMENT,
    /* Con trỏ NULL hoặc session ID toàn 0. */

    TRANSPORT_SESSION_STORE_REUSED,
    /* ID đã tồn tại và tuyệt đối không được mở lại. */

    TRANSPORT_SESSION_STORE_FULL,
    /* Không còn record erased để append. */

    TRANSPORT_SESSION_STORE_CORRUPT,
    /* Gặp record đã commit nhưng CRC sai. */

    TRANSPORT_SESSION_STORE_IO_ERROR
    /* W25Q32 read/program/verify/erase thất bại. */
} TransportSessionStore_Status_t;

TransportSessionStore_Status_t TransportSessionStore_Accept(
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE]
);

/* Chỉ gọi trong local factory/reprovision flow khi transport key đã đổi. */
TransportSessionStore_Status_t TransportSessionStore_EraseForProvisioning(void);

/* Adapter đúng chữ ký callback của SecureFrame. */
bool TransportSessionStore_AcceptCallback(
    void *context,
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE]
);

#endif /* AEGIS_TRANSPORT_SESSION_STORE_H */
