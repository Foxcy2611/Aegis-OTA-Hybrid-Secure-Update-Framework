#include "firmware_auth.h"

#include "crc32.h"
#include "memory_map.h"
#include "monocypher.h"
#include "monocypher-ed25519.h"

#include <stddef.h>
#include <string.h>

#define FIRMWARE_AUTH_IO_BUFFER_SIZE    128U

bool FirmwareAuth_HeaderIsValid(const Firmware_SignedHeader_t *header){
    if(header == NULL){
        return false;
    }

    if(memcmp(
           header->domain,
           FIRMWARE_DOMAIN_STRING,
           FIRMWARE_DOMAIN_SIZE
       ) != 0){
        return false;
    }

    return (header->format_version == FIRMWARE_FORMAT_VERSION) &&
           (header->image_size > 0U) &&
           (header->image_size <= APPLICATION_SIZE);
}

bool FirmwareAuth_SerializeHeader(
    const Firmware_SignedHeader_t *header,
    uint8_t bytes[FIRMWARE_SIGNED_HEADER_SIZE]
){
    if((header == NULL) || (bytes == NULL)){
        return false;
    }

    memcpy(
        &bytes[FIRMWARE_HEADER_DOMAIN_OFFSET],
        header->domain,
        FIRMWARE_DOMAIN_SIZE
    );

    bytes[FIRMWARE_HEADER_FORMAT_OFFSET + 0U] =
        (uint8_t)header->format_version;
    bytes[FIRMWARE_HEADER_FORMAT_OFFSET + 1U] =
        (uint8_t)(header->format_version >> 8U);
    bytes[FIRMWARE_HEADER_FORMAT_OFFSET + 2U] =
        (uint8_t)(header->format_version >> 16U);
    bytes[FIRMWARE_HEADER_FORMAT_OFFSET + 3U] =
        (uint8_t)(header->format_version >> 24U);

    bytes[FIRMWARE_HEADER_IMAGE_SIZE_OFFSET + 0U] =
        (uint8_t)header->image_size;
    bytes[FIRMWARE_HEADER_IMAGE_SIZE_OFFSET + 1U] =
        (uint8_t)(header->image_size >> 8U);
    bytes[FIRMWARE_HEADER_IMAGE_SIZE_OFFSET + 2U] =
        (uint8_t)(header->image_size >> 16U);
    bytes[FIRMWARE_HEADER_IMAGE_SIZE_OFFSET + 3U] =
        (uint8_t)(header->image_size >> 24U);

    bytes[FIRMWARE_HEADER_IMAGE_VERSION_OFFSET + 0U] =
        (uint8_t)header->image_version;
    bytes[FIRMWARE_HEADER_IMAGE_VERSION_OFFSET + 1U] =
        (uint8_t)(header->image_version >> 8U);
    bytes[FIRMWARE_HEADER_IMAGE_VERSION_OFFSET + 2U] =
        (uint8_t)(header->image_version >> 16U);
    bytes[FIRMWARE_HEADER_IMAGE_VERSION_OFFSET + 3U] =
        (uint8_t)(header->image_version >> 24U);

    bytes[FIRMWARE_HEADER_IMAGE_CRC32_OFFSET + 0U] =
        (uint8_t)header->image_crc32;
    bytes[FIRMWARE_HEADER_IMAGE_CRC32_OFFSET + 1U] =
        (uint8_t)(header->image_crc32 >> 8U);
    bytes[FIRMWARE_HEADER_IMAGE_CRC32_OFFSET + 2U] =
        (uint8_t)(header->image_crc32 >> 16U);
    bytes[FIRMWARE_HEADER_IMAGE_CRC32_OFFSET + 3U] =
        (uint8_t)(header->image_crc32 >> 24U);

    return true;
}

FirmwareAuth_Status_t FirmwareAuth_Verify(
    const Firmware_SignedHeader_t *header,
    const uint8_t signature[CRYPTO_ED25519_SIGNATURE_SIZE],
    const uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE],
    FirmwareAuth_ReadCallback_t reader,
    void *reader_context
){
    crypto_sha512_ctx sha_context;
    uint8_t header_bytes[FIRMWARE_SIGNED_HEADER_SIZE];
    uint8_t image_buffer[FIRMWARE_AUTH_IO_BUFFER_SIZE];
    uint8_t prehash[CRYPTO_SHA512_HASH_SIZE];
    uint32_t image_offset = 0U;
    uint32_t image_crc32 = CRC32_INITIAL_VALUE;
    FirmwareAuth_Status_t status = FIRMWARE_AUTH_OK;

    if((header == NULL) || (signature == NULL) ||
       (public_key == NULL) || (reader == NULL)){
        return FIRMWARE_AUTH_INVALID_ARGUMENT;
    }

    if(!FirmwareAuth_HeaderIsValid(header)){
        return FIRMWARE_AUTH_INVALID_HEADER;
    }

    (void)FirmwareAuth_SerializeHeader(header, header_bytes);
    crypto_sha512_init(&sha_context);
    crypto_sha512_update(&sha_context, header_bytes, sizeof(header_bytes));

    while(image_offset < header->image_size){
        const uint32_t remaining = header->image_size - image_offset;
        const uint32_t block_size =
            (remaining < (uint32_t)sizeof(image_buffer)) ?
                remaining : (uint32_t)sizeof(image_buffer);

        if(!reader(
               reader_context,
               image_offset,
               image_buffer,
               block_size
           )){
            status = FIRMWARE_AUTH_READ_ERROR;
            break;
        }

        image_crc32 = Crc32_Update(
            image_crc32,
            image_buffer,
            block_size
        );
        crypto_sha512_update(&sha_context, image_buffer, block_size);
        image_offset += block_size;
    }

    if((status == FIRMWARE_AUTH_OK) &&
       (image_crc32 != header->image_crc32)){
        status = FIRMWARE_AUTH_CRC_MISMATCH;
    }

    if(status == FIRMWARE_AUTH_OK){
        crypto_sha512_final(&sha_context, prehash);

        if(crypto_ed25519_ph_check(
               signature,
               public_key,
               prehash
           ) != 0){
            status = FIRMWARE_AUTH_SIGNATURE_FAILED;
        }
    }

    crypto_wipe(&sha_context, sizeof(sha_context));
    crypto_wipe(header_bytes, sizeof(header_bytes));
    crypto_wipe(image_buffer, sizeof(image_buffer));
    crypto_wipe(prehash, sizeof(prehash));

    return status;
}
