#include "crypto_service.h"

#include <string.h>

#include "memory_map.h"
#include "Monocypher/monocypher.h"
#include "Monocypher/monocypher-ed25519.h"

// STATE nội bộ cho VERIFICATION
typedef enum {
    CRYPTO_VERIFY_IDLE = 0U,
    CRYPTO_VERIFY_HASHING
} Crypto_VerifyState_t;

typedef struct {
    crypto_sha512_ctx sha512;
    uint32_t expected_size;
    uint32_t processed_size;
    Crypto_VerifyState_t state;
} Crypto_VerifyContext_t;

static Crypto_VerifyContext_t verify_ctx = { .state = CRYPTO_VERIFY_IDLE };

static void CryptoService_WriteLE16(uint8_t *dst, uint16_t value){
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8U) & 0xFFU);
}
 
static void CryptoService_WriteLE32(uint8_t *dst, uint32_t value){
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8U) & 0xFFU);
    dst[2] = (uint8_t)((value >> 16U) & 0xFFU);
    dst[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

static void CryptoService_BuildNonceBytes(const Crypto_Nonce_t *nonce_info, uint8_t out_nonce[CRYPTO_AEAD_NONCE_SIZE]){
    for(uint32_t i = 0U ; i < CRYPTO_SESSION_ID_SIZE ; i++){
        out_nonce[i] = nonce_info->session_id[i];
    }

    CryptoService_WriteLE32(&out_nonce[CRYPTO_NONCE_FRAME_COUNTER_OFFSET], nonce_info->frame_counter);
}

static void CryptoService_BuildAADBytes(const Crypto_FrameAad_t *aad_info, uint8_t out_aad[CRYPTO_FRAME_AAD_SIZE]){
    out_aad[CRYPTO_AAD_TYPE_OFFSET] = aad_info->type;
    CryptoService_WriteLE16(&out_aad[CRYPTO_AAD_SEQUENCE_OFFSET], aad_info->sequence);
    CryptoService_WriteLE16(&out_aad[CRYPTO_AAD_LENGTH_OFFSET], aad_info->length);
}

static void CryptoService_BuildHeaderBytes(const Crypto_SignedHeader_t *header, uint8_t out_header[CRYPTO_SIGNED_HEADER_SIZE]){
    for(uint32_t i = 0U ; i < CRYPTO_DOMAIN_SIZE ; i++){
        out_header[CRYPTO_SIGNED_DOMAIN_OFFSET + i] = header->domain[i];
    }

    CryptoService_WriteLE32(&out_header[CRYPTO_SIGNED_FORMAT_OFFSET], header->format_version);
    CryptoService_WriteLE32(&out_header[CRYPTO_SIGNED_IMAGE_SIZE_OFFSET], header->image_size);
    CryptoService_WriteLE32(&out_header[CRYPTO_SIGNED_IMAGE_VERSION_OFFSET], header->image_version);
    CryptoService_WriteLE32(&out_header[CRYPTO_SIGNED_IMAGE_CRC32_OFFSET], header->image_crc32);
}

Crypto_Status_t CryptoService_DecryptFrame(
    const uint8_t transport_key[CRYPTO_TRANSPORT_KEY_SIZE],
    const Crypto_Nonce_t *nonce_info,
    const Crypto_FrameAad_t *aad_info,
    const uint8_t *ciphertext,
    uint8_t *plaintext,
    const uint8_t tag[CRYPTO_AEAD_TAG_SIZE]
){
    if(!transport_key || !aad_info ||
        !nonce_info || !tag
    ){
        return CRYPTO_STATUS_INVALID_ARGUMENT;
    }
    if(aad_info->length > CRYPTO_FRAME_MAX_PAYLOAD_SIZE){
        return CRYPTO_STATUS_INVALID_LENGTH;
    }
    if((aad_info->length > 0U) && ((ciphertext == NULL) || (plaintext == NULL))){
        return CRYPTO_STATUS_INVALID_ARGUMENT;
    }

    uint8_t nonce[CRYPTO_AEAD_NONCE_SIZE];
    uint8_t aad[CRYPTO_FRAME_AAD_SIZE];
    CryptoService_BuildNonceBytes(nonce_info, nonce);
    CryptoService_BuildAADBytes(aad_info, aad);

    crypto_aead_ctx ctx;
    crypto_aead_init_ietf(&ctx, transport_key, nonce);

    int status = crypto_aead_read(
        &ctx, plaintext,
        tag, aad, CRYPTO_FRAME_AAD_SIZE,
        ciphertext, aad_info->length
    );

    crypto_wipe(&ctx, sizeof(ctx));
    crypto_wipe(aad, sizeof(aad));
    crypto_wipe(nonce, sizeof(nonce));

    if(status != 0){
        if((plaintext != NULL) && (aad_info->length > 0U)){
            crypto_wipe(plaintext, aad_info->length);
        }

        return CRYPTO_STATUS_AUTH_FAILED;
    }

    return CRYPTO_STATUS_OK;
}

Crypto_Status_t CryptoService_EncryptFrame(
    const uint8_t transport_key[CRYPTO_TRANSPORT_KEY_SIZE],
    const Crypto_Nonce_t *nonce_info,
    const Crypto_FrameAad_t *aad_info,
    const uint8_t *plaintext,
    uint8_t *out_ciphertext,
    uint8_t generated_tag[CRYPTO_AEAD_TAG_SIZE]
){
    if(!transport_key || !nonce_info || 
        !aad_info || !generated_tag
    ){
        return CRYPTO_STATUS_INVALID_ARGUMENT;
    }
    if(aad_info->length > CRYPTO_FRAME_MAX_PAYLOAD_SIZE){
        return CRYPTO_STATUS_INVALID_LENGTH;
    }
    if(aad_info->length > 0U && (!plaintext || !out_ciphertext)){
        return CRYPTO_STATUS_INVALID_ARGUMENT;
    }
    
    uint8_t nonce[CRYPTO_AEAD_NONCE_SIZE];
    uint8_t aad[CRYPTO_FRAME_AAD_SIZE];
    CryptoService_BuildNonceBytes(nonce_info, nonce);
    CryptoService_BuildAADBytes(aad_info, aad);

    crypto_aead_ctx ctx;
    crypto_aead_init_ietf(&ctx, transport_key, nonce);

    crypto_aead_write(
        &ctx, out_ciphertext,
        generated_tag, aad, CRYPTO_FRAME_AAD_SIZE,
        plaintext, aad_info->length
    );

    crypto_wipe(&ctx, sizeof(ctx));
    crypto_wipe(aad, CRYPTO_FRAME_AAD_SIZE);
    crypto_wipe(nonce, CRYPTO_AEAD_NONCE_SIZE);

    return CRYPTO_STATUS_OK;
}

// ========================= PHÂN LUỒNG CHO SHA và ED Check ========================= //
Crypto_Status_t CryptoService_VerifyBegin(const Crypto_SignedHeader_t *header){
    if(verify_ctx.state != CRYPTO_VERIFY_IDLE){
        return CRYPTO_STATUS_INVALID_STATE;
    }
    if(!header){
        return CRYPTO_STATUS_INVALID_ARGUMENT;
    }
    if(memcmp(header->domain, CRYPTO_DOMAIN_STRING, CRYPTO_DOMAIN_SIZE) != 0){
        return CRYPTO_STATUS_INVALID_HEADER;
    }
    if(header->format_version != CRYPTO_FORMAT_VERSION){
        return CRYPTO_STATUS_INVALID_HEADER;
    }
    if(header->image_size == 0U || header->image_size > APPLICATION_SIZE){
        return CRYPTO_STATUS_INVALID_LENGTH;
    }

    uint8_t header_bytes[CRYPTO_SIGNED_HEADER_SIZE];
    CryptoService_BuildHeaderBytes(header, header_bytes);

    crypto_sha512_init(&verify_ctx.sha512);
    crypto_sha512_update(&verify_ctx.sha512, header_bytes, CRYPTO_SIGNED_HEADER_SIZE);

    crypto_wipe(header_bytes, CRYPTO_SIGNED_HEADER_SIZE);

    verify_ctx.expected_size = header->image_size;
    verify_ctx.processed_size = 0U;
    verify_ctx.state = CRYPTO_VERIFY_HASHING;

    return CRYPTO_STATUS_OK;
}

Crypto_Status_t CryptoService_VerifyUpdate(const uint8_t *firmware_data, uint32_t length){
    if(verify_ctx.state != CRYPTO_VERIFY_HASHING){
        return CRYPTO_STATUS_INVALID_STATE;
    }
    if(firmware_data == NULL || length == 0U){
        return CRYPTO_STATUS_INVALID_ARGUMENT;
    }
    if(length > (verify_ctx.expected_size - verify_ctx.processed_size)){
        return CRYPTO_STATUS_INVALID_LENGTH;
    }

    crypto_sha512_update(&verify_ctx.sha512, firmware_data, length);
    verify_ctx.processed_size += length;

    return CRYPTO_STATUS_OK;
}

Crypto_Status_t CryptoService_VerifyFinal(
    const uint8_t signature[CRYPTO_ED25519_SIGNATURE_SIZE],
    const uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE]
){
    if(verify_ctx.state != CRYPTO_VERIFY_HASHING) {
        return CRYPTO_STATUS_INVALID_STATE;
    }
    if(!signature || !public_key) {
        return CRYPTO_STATUS_INVALID_ARGUMENT;
    }
    if(verify_ctx.processed_size != verify_ctx.expected_size) {
        return CRYPTO_STATUS_INCOMPLETE_IMAGE;
    }

    uint8_t hash[CRYPTO_SHA512_HASH_SIZE];
    crypto_sha512_final(&verify_ctx.sha512, hash);

    int status = crypto_ed25519_ph_check(signature, public_key, hash);

    crypto_wipe(hash, sizeof(hash));
    CryptoService_VerifyAbort();

    return (status == 0) ? CRYPTO_STATUS_OK : CRYPTO_STATUS_SIGNATURE_FAILED;
}

void CryptoService_VerifyAbort(void){
    crypto_wipe(&verify_ctx.sha512, sizeof(verify_ctx.sha512));
    verify_ctx.expected_size = 0U;
    verify_ctx.processed_size = 0U;
    verify_ctx.state = CRYPTO_VERIFY_IDLE;
}
