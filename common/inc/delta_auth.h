#ifndef AEGIS_DELTA_AUTH_H
#define AEGIS_DELTA_AUTH_H

/* Xác thực immutable Delta manifest và hash stream base/patch/target. */

#include "delta_format.h"

#include <stdbool.h>
#include <stdint.h>

typedef bool (*DeltaAuth_ReadCallback_t)(
    void *context,
    uint32_t offset,
    uint8_t *buffer,
    uint32_t length
);

typedef enum{
    DELTA_AUTH_OK = 0U,
    DELTA_AUTH_INVALID_ARGUMENT,
    DELTA_AUTH_INVALID_FORMAT,
    DELTA_AUTH_READ_ERROR,
    DELTA_AUTH_HASH_MISMATCH,
    DELTA_AUTH_SIGNATURE_FAILED
} DeltaAuth_Status_t;

DeltaAuth_Status_t DeltaAuth_VerifyManifest(
    const Delta_Manifest_t *manifest,
    const uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE]
);

DeltaAuth_Status_t DeltaAuth_VerifyStreamHash(
    DeltaAuth_ReadCallback_t reader,
    void *reader_context,
    uint32_t length,
    const uint8_t expected_sha512[CRYPTO_SHA512_HASH_SIZE]
);

#endif /* AEGIS_DELTA_AUTH_H */
