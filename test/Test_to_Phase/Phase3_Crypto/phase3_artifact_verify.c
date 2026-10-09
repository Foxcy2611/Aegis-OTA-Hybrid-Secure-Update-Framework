#include "firmware_auth.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct{
    const uint8_t *image;
    uint32_t image_size;
} ArtifactReader_t;

static uint32_t ReadU32Le(const uint8_t *bytes){
    return ((uint32_t)bytes[0]) |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static bool ArtifactReader_Read(
    void *context,
    uint32_t offset,
    uint8_t *buffer,
    uint32_t length
){
    const ArtifactReader_t *reader = (const ArtifactReader_t *)context;

    if((reader == NULL) || (buffer == NULL)){
        return false;
    }

    if((offset > reader->image_size) ||
       (length > (reader->image_size - offset))){
        return false;
    }

    memcpy(buffer, &reader->image[offset], length);
    return true;
}

static bool ReadWholeFile(
    const char *path,
    uint8_t **data,
    size_t *size
){
    FILE *file;
    long file_size;
    uint8_t *buffer;

    if((path == NULL) || (data == NULL) || (size == NULL)){
        return false;
    }

    file = fopen(path, "rb");
    if(file == NULL){
        return false;
    }

    if((fseek(file, 0L, SEEK_END) != 0) ||
       ((file_size = ftell(file)) < 0L) ||
       (fseek(file, 0L, SEEK_SET) != 0)){
        (void)fclose(file);
        return false;
    }

    buffer = (uint8_t *)malloc((size_t)file_size);
    if((buffer == NULL) && (file_size > 0L)){
        (void)fclose(file);
        return false;
    }

    if((file_size > 0L) &&
       (fread(buffer, 1U, (size_t)file_size, file) != (size_t)file_size)){
        free(buffer);
        (void)fclose(file);
        return false;
    }

    if(fclose(file) != 0){
        free(buffer);
        return false;
    }

    *data = buffer;
    *size = (size_t)file_size;
    return true;
}

int main(int argument_count, char **arguments){
    uint8_t *artifact = NULL;
    uint8_t *public_key_file = NULL;
    size_t artifact_size = 0U;
    size_t public_key_size = 0U;
    Firmware_SignedHeader_t header;
    ArtifactReader_t reader;
    const uint8_t *signature;
    size_t expected_artifact_size;
    FirmwareAuth_Status_t status;
    int exit_code = 1;

    if(argument_count != 3){
        fprintf(
            stderr,
            "Usage: %s firmware.aegisfw public_key.bin\n",
            arguments[0]
        );
        return 2;
    }

    if(!ReadWholeFile(arguments[1], &artifact, &artifact_size) ||
       !ReadWholeFile(arguments[2], &public_key_file, &public_key_size)){
        fprintf(stderr, "Không đọc được artifact hoặc public key\n");
        goto cleanup;
    }

    if((artifact_size < (FIRMWARE_SIGNED_HEADER_SIZE +
                         FIRMWARE_SIGNATURE_SIZE)) ||
       (public_key_size != CRYPTO_ED25519_PUBLIC_KEY_SIZE)){
        fprintf(stderr, "Sai kích thước artifact hoặc public key\n");
        goto cleanup;
    }

    memcpy(
        header.domain,
        &artifact[FIRMWARE_HEADER_DOMAIN_OFFSET],
        FIRMWARE_DOMAIN_SIZE
    );
    header.format_version = ReadU32Le(
        &artifact[FIRMWARE_HEADER_FORMAT_OFFSET]
    );
    header.image_size = ReadU32Le(
        &artifact[FIRMWARE_HEADER_IMAGE_SIZE_OFFSET]
    );
    header.image_version = ReadU32Le(
        &artifact[FIRMWARE_HEADER_IMAGE_VERSION_OFFSET]
    );
    header.image_crc32 = ReadU32Le(
        &artifact[FIRMWARE_HEADER_IMAGE_CRC32_OFFSET]
    );

    expected_artifact_size =
        (size_t)FIRMWARE_SIGNED_HEADER_SIZE +
        (size_t)header.image_size +
        (size_t)FIRMWARE_SIGNATURE_SIZE;
    if(expected_artifact_size != artifact_size){
        fprintf(stderr, "image_size không khớp kích thước artifact\n");
        goto cleanup;
    }

    reader.image = &artifact[FIRMWARE_SIGNED_HEADER_SIZE];
    reader.image_size = header.image_size;
    signature = &artifact[FIRMWARE_SIGNED_HEADER_SIZE + header.image_size];
    status = FirmwareAuth_Verify(
        &header,
        signature,
        public_key_file,
        ArtifactReader_Read,
        &reader
    );

    if(status != FIRMWARE_AUTH_OK){
        fprintf(stderr, "C verifier từ chối artifact, status=%u\n", status);
        goto cleanup;
    }

    printf(
        "PASS artifact C verify: image=%lu bytes, version=%lu\n",
        (unsigned long)header.image_size,
        (unsigned long)header.image_version
    );
    exit_code = 0;

cleanup:
    free(public_key_file);
    free(artifact);
    return exit_code;
}
