#include "delta_auth.h"

#include "memory_map.h"

#include "monocypher.h"
#include "monocypher-ed25519.h"

#include <string.h>

#define DELTA_AUTH_BUFFER_SIZE 128U

static bool DeltaAuth_IdIsNonzero(const uint8_t *id, uint32_t length){
    uint8_t combined = 0U;

    for(uint32_t index = 0U; index < length; ++index){
        combined |= id[index];
    }
    return combined != 0U;
}

DeltaAuth_Status_t DeltaAuth_VerifyManifest(
    const Delta_Manifest_t *manifest,
    const uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE]
){
    uint8_t prehash[CRYPTO_SHA512_HASH_SIZE];
    const Delta_ImmutableBody_t *body;
    DeltaAuth_Status_t status = DELTA_AUTH_OK;

    if((manifest == NULL) || (public_key == NULL)){
        return DELTA_AUTH_INVALID_ARGUMENT;
    }
    body = &manifest->immutable_body;
    if((memcmp(body->domain, DELTA_DOMAIN_STRING,
               DELTA_DOMAIN_FIELD_SIZE) != 0) ||
       (body->format_version != DELTA_FORMAT_VERSION) ||
       (body->algorithm_id != DELTA_ALGORITHM_COPY_ADD_V1) ||
       !DeltaAuth_IdIsNonzero(body->artifact_id, sizeof(body->artifact_id)) ||
       (body->base_size == 0U) || (body->base_size > APPLICATION_SIZE) ||
       (body->target_header.image_size == 0U) ||
       (body->target_header.image_size > APPLICATION_SIZE) ||
       (body->patch_size == 0U) ||
       (body->patch_size > W25Q32_DELTA_DATA_SIZE)){
        return DELTA_AUTH_INVALID_FORMAT;
    }

    crypto_sha512(prehash, (const uint8_t *)body, sizeof(*body));
    if(crypto_ed25519_ph_check(
           manifest->delta_signature,
           public_key,
           prehash
       ) != 0){
        status = DELTA_AUTH_SIGNATURE_FAILED;
    }
    crypto_wipe(prehash, sizeof(prehash));
    return status;
}

DeltaAuth_Status_t DeltaAuth_VerifyStreamHash(
    DeltaAuth_ReadCallback_t reader,
    void *reader_context,
    uint32_t length,
    const uint8_t expected_sha512[CRYPTO_SHA512_HASH_SIZE]
){
    crypto_sha512_ctx sha_context;
    uint8_t buffer[DELTA_AUTH_BUFFER_SIZE];
    uint8_t actual[CRYPTO_SHA512_HASH_SIZE];
    uint32_t offset = 0U;
    DeltaAuth_Status_t status = DELTA_AUTH_OK;

    if((reader == NULL) || (expected_sha512 == NULL) || (length == 0U)){
        return DELTA_AUTH_INVALID_ARGUMENT;
    }
    crypto_sha512_init(&sha_context);
    while(offset < length){
        const uint32_t remaining = length - offset;
        const uint32_t chunk = remaining < sizeof(buffer) ?
            remaining : (uint32_t)sizeof(buffer);

        if(!reader(reader_context, offset, buffer, chunk)){
            status = DELTA_AUTH_READ_ERROR;
            break;
        }
        crypto_sha512_update(&sha_context, buffer, chunk);
        offset += chunk;
    }
    if(status == DELTA_AUTH_OK){
        crypto_sha512_final(&sha_context, actual);
        if(crypto_verify64(actual, expected_sha512) != 0){
            status = DELTA_AUTH_HASH_MISMATCH;
        }
    }
    crypto_wipe(&sha_context, sizeof(sha_context));
    crypto_wipe(buffer, sizeof(buffer));
    crypto_wipe(actual, sizeof(actual));
    return status;
}
