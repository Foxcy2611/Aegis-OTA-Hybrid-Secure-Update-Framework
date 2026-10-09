#include "crc32.h"
#include "firmware_auth.h"
#include "memory_map.h"
#include "monocypher-ed25519.h"
#include "monocypher.h"
#include "secure_transport.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t test_count = 0U;
static uint32_t failure_count = 0U;

#define CHECK(condition)                                                   \
    do{                                                                    \
        ++test_count;                                                      \
        if(!(condition)){                                                  \
            ++failure_count;                                               \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);   \
            return false;                                                  \
        }                                                                  \
    }while(0)

typedef struct{
    const uint8_t *data;
    uint32_t size;
    bool fail_enabled;
    uint32_t fail_offset;
} MemoryReader_t;

static bool MemoryReader_Read(
    void *context,
    uint32_t offset,
    uint8_t *buffer,
    uint32_t length
){
    MemoryReader_t *reader = (MemoryReader_t *)context;

    if((reader == NULL) || (buffer == NULL)){
        return false;
    }

    if(reader->fail_enabled && (offset >= reader->fail_offset)){
        return false;
    }

    if((offset > reader->size) || (length > (reader->size - offset))){
        return false;
    }

    memcpy(buffer, &reader->data[offset], length);
    return true;
}

static bool BufferIsZero(const uint8_t *buffer, size_t length){
    for(size_t index = 0U; index < length; ++index){
        if(buffer[index] != 0U){
            return false;
        }
    }

    return true;
}

static bool TestCryptoContractSerialization(void){
    static const uint8_t session_id[CRYPTO_SESSION_ID_SIZE] = {
        0x00U, 0x11U, 0x22U, 0x33U,
        0x44U, 0x55U, 0x66U, 0x77U
    };
    static const uint8_t expected_nonce[CRYPTO_AEAD_NONCE_SIZE] = {
        0x00U, 0x11U, 0x22U, 0x33U,
        0x44U, 0x55U, 0x66U, 0x77U,
        0x78U, 0x56U, 0x34U, 0x12U
    };
    static const uint8_t expected_aad[CRYPTO_FRAME_AAD_SIZE] = {
        0x02U, 0x56U, 0x34U, 0x70U, 0x00U
    };
    Crypto_Nonce_t nonce;
    Crypto_FrameAad_t aad;

    CHECK(SecureTransport_BuildNonce(
              session_id,
              UINT32_C(0x12345678),
              &nonce
          ) == SECURE_TRANSPORT_OK);
    CHECK(memcmp(&nonce, expected_nonce, sizeof(expected_nonce)) == 0);
    CHECK(SecureTransport_BuildAad(
              0x02U,
              UINT16_C(0x3456),
              UINT16_C(0x0070),
              &aad
          ) == SECURE_TRANSPORT_OK);
    CHECK(memcmp(&aad, expected_aad, sizeof(expected_aad)) == 0);
    CHECK(SecureTransport_BuildNonce(
              NULL,
              0U,
              &nonce
          ) == SECURE_TRANSPORT_INVALID_ARGUMENT);
    CHECK(SecureTransport_BuildAad(
              0U,
              0U,
              257U,
              &aad
          ) == SECURE_TRANSPORT_INVALID_LENGTH);

    return true;
}

static bool TestSecureTransportKnownAnswers(void){
    static const uint8_t key[CRYPTO_TRANSPORT_KEY_SIZE] = {
        0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U,
        0x08U, 0x09U, 0x0AU, 0x0BU, 0x0CU, 0x0DU, 0x0EU, 0x0FU,
        0x10U, 0x11U, 0x12U, 0x13U, 0x14U, 0x15U, 0x16U, 0x17U,
        0x18U, 0x19U, 0x1AU, 0x1BU, 0x1CU, 0x1DU, 0x1EU, 0x1FU
    };
    static const uint8_t session_id[CRYPTO_SESSION_ID_SIZE] = {
        0xA0U, 0xA1U, 0xA2U, 0xA3U,
        0xA4U, 0xA5U, 0xA6U, 0xA7U
    };
    static const uint8_t plaintext[] = "Aegis Phase 3 vector";
    static const uint8_t expected_device_ciphertext[] = {
        0x89U, 0xABU, 0x29U, 0x8BU, 0x78U, 0xEBU, 0x58U, 0x67U,
        0x30U, 0xDBU, 0xD8U, 0xF8U, 0xE9U, 0x74U, 0xB0U, 0x46U,
        0x60U, 0x8DU, 0x08U, 0x16U
    };
    static const uint8_t expected_device_tag[CRYPTO_AEAD_TAG_SIZE] = {
        0x0AU, 0x8AU, 0xEBU, 0x70U, 0xA5U, 0x9DU, 0x74U, 0x36U,
        0xC9U, 0xC9U, 0x87U, 0x9CU, 0xBCU, 0xF4U, 0x75U, 0x46U
    };
    static const uint8_t host_ciphertext[] = {
        0xB8U, 0xB0U, 0xF7U, 0x2CU, 0xABU, 0x12U, 0x6BU, 0x15U,
        0x2AU, 0xF5U, 0x9DU, 0xB1U, 0x6EU, 0xF4U, 0x81U, 0x11U,
        0x81U, 0x6AU, 0xA4U, 0xB4U
    };
    static const uint8_t host_tag[CRYPTO_AEAD_TAG_SIZE] = {
        0xBAU, 0x68U, 0x87U, 0xF4U, 0xB0U, 0xC7U, 0x35U, 0x01U,
        0x4FU, 0xE8U, 0x09U, 0x97U, 0x7EU, 0x92U, 0x1AU, 0x13U
    };
    const uint16_t plaintext_length =
        (uint16_t)(sizeof(plaintext) - 1U);
    uint8_t ciphertext[sizeof(plaintext) - 1U];
    uint8_t tag[CRYPTO_AEAD_TAG_SIZE];
    uint8_t output[sizeof(plaintext) - 1U];
    uint8_t tampered_ciphertext[sizeof(host_ciphertext)];
    uint8_t tampered_tag[sizeof(host_tag)];
    uint8_t tampered_session[sizeof(session_id)];

    CHECK(SecureTransport_Encrypt(
              key,
              session_id,
              UINT32_C(0x81234567),
              0x42U,
              UINT16_C(0x1234),
              plaintext,
              plaintext_length,
              ciphertext,
              tag
          ) == SECURE_TRANSPORT_OK);
    CHECK(memcmp(
              ciphertext,
              expected_device_ciphertext,
              sizeof(ciphertext)
          ) == 0);
    CHECK(memcmp(tag, expected_device_tag, sizeof(tag)) == 0);

    CHECK(SecureTransport_Decrypt(
              key,
              session_id,
              UINT32_C(0x01234567),
              0x42U,
              UINT16_C(0x1234),
              host_ciphertext,
              plaintext_length,
              host_tag,
              output
          ) == SECURE_TRANSPORT_OK);
    CHECK(memcmp(output, plaintext, plaintext_length) == 0);

    memcpy(tampered_ciphertext, host_ciphertext, sizeof(host_ciphertext));
    tampered_ciphertext[3] ^= 0x01U;
    memset(output, 0xA5, sizeof(output));
    CHECK(SecureTransport_Decrypt(
              key,
              session_id,
              UINT32_C(0x01234567),
              0x42U,
              UINT16_C(0x1234),
              tampered_ciphertext,
              plaintext_length,
              host_tag,
              output
          ) == SECURE_TRANSPORT_AUTH_FAILED);
    CHECK(BufferIsZero(output, sizeof(output)));

    memcpy(tampered_tag, host_tag, sizeof(host_tag));
    tampered_tag[15] ^= 0x80U;
    CHECK(SecureTransport_Decrypt(
              key,
              session_id,
              UINT32_C(0x01234567),
              0x42U,
              UINT16_C(0x1234),
              host_ciphertext,
              plaintext_length,
              tampered_tag,
              output
          ) == SECURE_TRANSPORT_AUTH_FAILED);
    CHECK(SecureTransport_Decrypt(
              key,
              session_id,
              UINT32_C(0x01234567),
              0x43U,
              UINT16_C(0x1234),
              host_ciphertext,
              plaintext_length,
              host_tag,
              output
          ) == SECURE_TRANSPORT_AUTH_FAILED);

    memcpy(tampered_session, session_id, sizeof(session_id));
    tampered_session[0] ^= 0x01U;
    CHECK(SecureTransport_Decrypt(
              key,
              tampered_session,
              UINT32_C(0x01234567),
              0x42U,
              UINT16_C(0x1234),
              host_ciphertext,
              plaintext_length,
              host_tag,
              output
          ) == SECURE_TRANSPORT_AUTH_FAILED);
    CHECK(SecureTransport_Decrypt(
              key,
              session_id,
              UINT32_C(0x01234566),
              0x42U,
              UINT16_C(0x1234),
              host_ciphertext,
              plaintext_length,
              host_tag,
              output
          ) == SECURE_TRANSPORT_AUTH_FAILED);
    CHECK(SecureTransport_Encrypt(
              key,
              session_id,
              1U,
              0U,
              0U,
              plaintext,
              plaintext_length,
              ciphertext,
              tag
          ) == SECURE_TRANSPORT_WRONG_COUNTER_DOMAIN);
    CHECK(SecureTransport_Decrypt(
              key,
              session_id,
              CRYPTO_DEVICE_TO_HOST_COUNTER_MIN,
              0U,
              0U,
              host_ciphertext,
              plaintext_length,
              host_tag,
              output
          ) == SECURE_TRANSPORT_WRONG_COUNTER_DOMAIN);

    return true;
}

static void SignFixture(
    const Firmware_SignedHeader_t *header,
    const uint8_t *image,
    uint32_t image_size,
    uint8_t signature[CRYPTO_ED25519_SIGNATURE_SIZE],
    uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE]
){
    uint8_t seed[CRYPTO_ED25519_SEED_SIZE];
    uint8_t secret_key[CRYPTO_ED25519_SECRET_KEY_SIZE];
    uint8_t header_bytes[FIRMWARE_SIGNED_HEADER_SIZE];
    uint8_t prehash[CRYPTO_SHA512_HASH_SIZE];
    crypto_sha512_ctx sha_context;

    for(uint32_t index = 0U; index < sizeof(seed); ++index){
        seed[index] = (uint8_t)index;
    }

    (void)FirmwareAuth_SerializeHeader(header, header_bytes);
    crypto_sha512_init(&sha_context);
    crypto_sha512_update(&sha_context, header_bytes, sizeof(header_bytes));
    crypto_sha512_update(&sha_context, image, image_size);
    crypto_sha512_final(&sha_context, prehash);
    crypto_ed25519_key_pair(secret_key, public_key, seed);
    crypto_ed25519_ph_sign(signature, secret_key, prehash);

    crypto_wipe(secret_key, sizeof(secret_key));
    crypto_wipe(prehash, sizeof(prehash));
    crypto_wipe(header_bytes, sizeof(header_bytes));
}

static bool TestFirmwareHeaderSerialization(void){
    static const uint8_t expected[FIRMWARE_SIGNED_HEADER_SIZE] = {
        'A', 'E', 'G', 'I', 'S', '-', 'F', '1',
        '0', '3', '-', 'F', 'W', '-', 'V', '1',
        0x01U, 0x00U, 0x00U, 0x00U,
        0x4DU, 0x01U, 0x00U, 0x00U,
        0x04U, 0x03U, 0x02U, 0x01U,
        0xEFU, 0xCDU, 0xABU, 0x89U
    };
    Firmware_SignedHeader_t header;
    uint8_t bytes[FIRMWARE_SIGNED_HEADER_SIZE];

    memcpy(header.domain, FIRMWARE_DOMAIN_STRING, FIRMWARE_DOMAIN_SIZE);
    header.format_version = FIRMWARE_FORMAT_VERSION;
    header.image_size = 333U;
    header.image_version = UINT32_C(0x01020304);
    header.image_crc32 = UINT32_C(0x89ABCDEF);

    CHECK(FirmwareAuth_SerializeHeader(&header, bytes));
    CHECK(memcmp(bytes, expected, sizeof(expected)) == 0);
    CHECK(!FirmwareAuth_SerializeHeader(NULL, bytes));

    return true;
}

static bool TestFirmwareAuthentication(void){
    uint8_t image[333];
    uint8_t original_image_byte;
    Firmware_SignedHeader_t header;
    Firmware_SignedHeader_t modified_header;
    uint8_t signature[CRYPTO_ED25519_SIGNATURE_SIZE];
    uint8_t modified_signature[CRYPTO_ED25519_SIGNATURE_SIZE];
    uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE];
    uint8_t modified_public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE];
    MemoryReader_t reader;

    for(uint32_t index = 0U; index < sizeof(image); ++index){
        image[index] = (uint8_t)((index * 37U) + 11U);
    }

    memcpy(header.domain, FIRMWARE_DOMAIN_STRING, FIRMWARE_DOMAIN_SIZE);
    header.format_version = FIRMWARE_FORMAT_VERSION;
    header.image_size = sizeof(image);
    header.image_version = 7U;
    header.image_crc32 = Crc32_Compute(image, sizeof(image));
    SignFixture(
        &header,
        image,
        sizeof(image),
        signature,
        public_key
    );

    reader.data = image;
    reader.size = sizeof(image);
    reader.fail_enabled = false;
    reader.fail_offset = 0U;

    CHECK(FirmwareAuth_Verify(
              &header,
              signature,
              public_key,
              MemoryReader_Read,
              &reader
          ) == FIRMWARE_AUTH_OK);

    modified_header = header;
    modified_header.domain[0] ^= 0x01U;
    CHECK(FirmwareAuth_Verify(
              &modified_header,
              signature,
              public_key,
              MemoryReader_Read,
              &reader
          ) == FIRMWARE_AUTH_INVALID_HEADER);

    modified_header = header;
    modified_header.image_version += 1U;
    CHECK(FirmwareAuth_Verify(
              &modified_header,
              signature,
              public_key,
              MemoryReader_Read,
              &reader
          ) == FIRMWARE_AUTH_SIGNATURE_FAILED);

    original_image_byte = image[200];
    image[200] ^= 0x01U;
    CHECK(FirmwareAuth_Verify(
              &header,
              signature,
              public_key,
              MemoryReader_Read,
              &reader
          ) == FIRMWARE_AUTH_CRC_MISMATCH);

    modified_header = header;
    modified_header.image_crc32 = Crc32_Compute(image, sizeof(image));
    CHECK(FirmwareAuth_Verify(
              &modified_header,
              signature,
              public_key,
              MemoryReader_Read,
              &reader
          ) == FIRMWARE_AUTH_SIGNATURE_FAILED);
    image[200] = original_image_byte;

    memcpy(modified_signature, signature, sizeof(signature));
    modified_signature[10] ^= 0x01U;
    CHECK(FirmwareAuth_Verify(
              &header,
              modified_signature,
              public_key,
              MemoryReader_Read,
              &reader
          ) == FIRMWARE_AUTH_SIGNATURE_FAILED);

    memcpy(modified_public_key, public_key, sizeof(public_key));
    modified_public_key[0] ^= 0x80U;
    CHECK(FirmwareAuth_Verify(
              &header,
              signature,
              modified_public_key,
              MemoryReader_Read,
              &reader
          ) == FIRMWARE_AUTH_SIGNATURE_FAILED);

    reader.fail_enabled = true;
    reader.fail_offset = 128U;
    CHECK(FirmwareAuth_Verify(
              &header,
              signature,
              public_key,
              MemoryReader_Read,
              &reader
          ) == FIRMWARE_AUTH_READ_ERROR);
    reader.fail_enabled = false;

    modified_header = header;
    modified_header.image_size = 0U;
    CHECK(!FirmwareAuth_HeaderIsValid(&modified_header));
    modified_header = header;
    modified_header.image_size = APPLICATION_SIZE + 1U;
    CHECK(!FirmwareAuth_HeaderIsValid(&modified_header));
    modified_header = header;
    modified_header.image_version = 0U;
    CHECK(FirmwareAuth_Verify(
              &modified_header,
              signature,
              public_key,
              MemoryReader_Read,
              &reader
          ) == FIRMWARE_AUTH_SIGNATURE_FAILED);
    CHECK(FirmwareAuth_Verify(
              NULL,
              signature,
              public_key,
              MemoryReader_Read,
              &reader
          ) == FIRMWARE_AUTH_INVALID_ARGUMENT);

    return true;
}

static void RunTest(const char *name, bool (*test_function)(void)){
    const uint32_t failures_before = failure_count;
    const bool completed = test_function();

    if(completed && (failure_count == failures_before)){
        printf("PASS %s\n", name);
    }
}

int main(void){
    RunTest("crypto_contract_serialization", TestCryptoContractSerialization);
    RunTest("secure_transport_known_answers", TestSecureTransportKnownAnswers);
    RunTest("firmware_header_serialization", TestFirmwareHeaderSerialization);
    RunTest("firmware_authentication", TestFirmwareAuthentication);

    printf(
        "Phase 3 C tests: %lu checks, %lu failures\n",
        (unsigned long)test_count,
        (unsigned long)failure_count
    );

    return (failure_count == 0U) ? 0 : 1;
}
