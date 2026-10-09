#ifndef AEGIS_DEVICE_KEYS_H
#define AEGIS_DEVICE_KEYS_H

/* Provisioning boundary: override các weak symbol bằng key của từng thiết bị. */

#include "crypto_config.h"

#include <stdbool.h>
#include <stdint.h>

bool DeviceKeys_GetReleasePublicKey(
    uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE]
);

/* Chỉ Application dùng transport key; Bootloader không gọi API này. */
bool DeviceKeys_GetTransportKey(
    uint8_t key[CRYPTO_TRANSPORT_KEY_SIZE]
);

#endif /* AEGIS_DEVICE_KEYS_H */
