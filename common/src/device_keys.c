#include "device_keys.h"

#include <string.h>

/*
 * Bản mẫu giữ key bằng 0 và fail-closed. Production override hai symbol weak
 * bằng source/linker section sinh từ quy trình provision; không commit secret.
 */
__attribute__((weak)) const uint8_t
g_aegis_release_public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE] = {0U};

__attribute__((weak)) const uint8_t
g_aegis_transport_key[CRYPTO_TRANSPORT_KEY_SIZE] = {0U};

static bool DeviceKeys_CopyIfProvisioned(
    uint8_t *output,
    const uint8_t *source,
    uint32_t length
){
    uint8_t combined = 0U;

    for(uint32_t index = 0U; index < length; ++index){
        combined |= source[index];
    }
    if(combined == 0U){
        memset(output, 0, length);
        return false;
    }
    memcpy(output, source, length);
    return true;
}

bool DeviceKeys_GetReleasePublicKey(
    uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE]
){
    if(public_key == NULL){
        return false;
    }
    return DeviceKeys_CopyIfProvisioned(
        public_key,
        g_aegis_release_public_key,
        CRYPTO_ED25519_PUBLIC_KEY_SIZE
    );
}

bool DeviceKeys_GetTransportKey(
    uint8_t key[CRYPTO_TRANSPORT_KEY_SIZE]
){
    if(key == NULL){
        return false;
    }
    return DeviceKeys_CopyIfProvisioned(
        key,
        g_aegis_transport_key,
        CRYPTO_TRANSPORT_KEY_SIZE
    );
}
