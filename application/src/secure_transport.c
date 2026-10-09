#include "secure_transport.h"

#include "monocypher.h"

#include <stddef.h>
#include <string.h>

SecureTransport_Status_t SecureTransport_BuildNonce(
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE],
    uint32_t frame_counter,
    Crypto_Nonce_t *nonce
){
    if((session_id == NULL) || (nonce == NULL)){
        return SECURE_TRANSPORT_INVALID_ARGUMENT;
    }

    memcpy(nonce->session_id, session_id, CRYPTO_SESSION_ID_SIZE);
    nonce->frame_counter_le[0] = (uint8_t)frame_counter;
    nonce->frame_counter_le[1] = (uint8_t)(frame_counter >> 8U);
    nonce->frame_counter_le[2] = (uint8_t)(frame_counter >> 16U);
    nonce->frame_counter_le[3] = (uint8_t)(frame_counter >> 24U);

    return SECURE_TRANSPORT_OK;
}

SecureTransport_Status_t SecureTransport_BuildAad(
    uint8_t type,
    uint16_t sequence,
    uint16_t length,
    Crypto_FrameAad_t *aad
){
    if(aad == NULL){
        return SECURE_TRANSPORT_INVALID_ARGUMENT;
    }

    if(length > CRYPTO_FRAME_MAX_PAYLOAD_SIZE){
        return SECURE_TRANSPORT_INVALID_LENGTH;
    }

    aad->type = type;
    aad->sequence_le[0] = (uint8_t)sequence;
    aad->sequence_le[1] = (uint8_t)(sequence >> 8U);
    aad->length_le[0] = (uint8_t)length;
    aad->length_le[1] = (uint8_t)(length >> 8U);

    return SECURE_TRANSPORT_OK;
}

SecureTransport_Status_t SecureTransport_Encrypt(
    const uint8_t key[CRYPTO_TRANSPORT_KEY_SIZE],
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE],
    uint32_t frame_counter,
    uint8_t type,
    uint16_t sequence,
    const uint8_t *plaintext,
    uint16_t length,
    uint8_t *ciphertext,
    uint8_t tag[CRYPTO_AEAD_TAG_SIZE]
){
    Crypto_Nonce_t nonce;
    Crypto_FrameAad_t aad;
    crypto_aead_ctx context;
    uint8_t empty_input = 0U;
    uint8_t empty_output = 0U;
    const uint8_t *input = plaintext;
    uint8_t *output = ciphertext;

    if((key == NULL) || (session_id == NULL) || (tag == NULL) ||
       ((length > 0U) && ((plaintext == NULL) || (ciphertext == NULL)))){
        return SECURE_TRANSPORT_INVALID_ARGUMENT;
    }

    if(length > CRYPTO_FRAME_MAX_PAYLOAD_SIZE){
        return SECURE_TRANSPORT_INVALID_LENGTH;
    }

    if(frame_counter < CRYPTO_DEVICE_TO_HOST_COUNTER_MIN){
        return SECURE_TRANSPORT_WRONG_COUNTER_DOMAIN;
    }

    if(length == 0U){
        input = &empty_input;
        output = &empty_output;
    }

    (void)SecureTransport_BuildNonce(session_id, frame_counter, &nonce);
    (void)SecureTransport_BuildAad(type, sequence, length, &aad);

    crypto_aead_init_ietf(&context, key, (const uint8_t *)&nonce);
    crypto_aead_write(
        &context,
        output,
        tag,
        (const uint8_t *)&aad,
        sizeof(aad),
        input,
        length
    );

    crypto_wipe(&context, sizeof(context));
    crypto_wipe(&nonce, sizeof(nonce));
    crypto_wipe(&aad, sizeof(aad));

    return SECURE_TRANSPORT_OK;
}

SecureTransport_Status_t SecureTransport_Decrypt(
    const uint8_t key[CRYPTO_TRANSPORT_KEY_SIZE],
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE],
    uint32_t frame_counter,
    uint8_t type,
    uint16_t sequence,
    const uint8_t *ciphertext,
    uint16_t length,
    const uint8_t tag[CRYPTO_AEAD_TAG_SIZE],
    uint8_t *plaintext
){
    Crypto_Nonce_t nonce;
    Crypto_FrameAad_t aad;
    crypto_aead_ctx context;
    uint8_t empty_input = 0U;
    uint8_t empty_output = 0U;
    const uint8_t *input = ciphertext;
    uint8_t *output = plaintext;
    int authentication_result;

    if((key == NULL) || (session_id == NULL) || (tag == NULL) ||
       ((length > 0U) && ((ciphertext == NULL) || (plaintext == NULL)))){
        return SECURE_TRANSPORT_INVALID_ARGUMENT;
    }

    if(length > CRYPTO_FRAME_MAX_PAYLOAD_SIZE){
        return SECURE_TRANSPORT_INVALID_LENGTH;
    }

    if(frame_counter > CRYPTO_HOST_TO_DEVICE_COUNTER_MAX){
        return SECURE_TRANSPORT_WRONG_COUNTER_DOMAIN;
    }

    if(length == 0U){
        input = &empty_input;
        output = &empty_output;
    }

    (void)SecureTransport_BuildNonce(session_id, frame_counter, &nonce);
    (void)SecureTransport_BuildAad(type, sequence, length, &aad);

    crypto_aead_init_ietf(&context, key, (const uint8_t *)&nonce);
    authentication_result = crypto_aead_read(
        &context,
        output,
        tag,
        (const uint8_t *)&aad,
        sizeof(aad),
        input,
        length
    );

    crypto_wipe(&context, sizeof(context));
    crypto_wipe(&nonce, sizeof(nonce));
    crypto_wipe(&aad, sizeof(aad));

    if(authentication_result != 0){
        if((plaintext != NULL) && (length > 0U)){
            crypto_wipe(plaintext, length);
        }

        return SECURE_TRANSPORT_AUTH_FAILED;
    }

    return SECURE_TRANSPORT_OK;
}
