#include "application_health.h"
#include "device_keys.h"
#include "lora_as32.h"
#include "memory_map.h"
#include "ota_update_agent.h"
#include "watchdog.h"

#include "stm32f10x.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OTA_APP_FRAME_TIMEOUT_MS 15000U

static bool OtaApp_ReadExact(
    uint8_t *data,
    uint16_t length,
    uint32_t timeout_ms
){
    uint16_t received = 0U;

    Watchdog_Feed();

    const bool received_exactly =
        LoraAs32_Receive(data, length, &received, timeout_ms) ==
            LORA_AS32_OK &&
        received == length;

    Watchdog_Feed();
    return received_exactly;
}

/* Tìm SYNC rồi đọc đúng LEN-dependent secure frame, không parse OTA tại ISR. */
static bool OtaApp_ReceiveFrame(
    uint8_t raw[CRYPTO_FRAME_MAX_RAW_SIZE],
    size_t *raw_length
){
    bool first_sync_seen = false;
    uint8_t byte;
    uint16_t payload_length;
    uint16_t remaining;

    if((raw == NULL) || (raw_length == NULL)){
        return false;
    }
    for(;;){
        if(!OtaApp_ReadExact(&byte, 1U, OTA_APP_FRAME_TIMEOUT_MS)){
            return false;
        }
        if(!first_sync_seen){
            first_sync_seen = byte == CRYPTO_FRAME_SYNC_BYTE_0;
            continue;
        }
        if(byte == CRYPTO_FRAME_SYNC_BYTE_1){
            raw[0] = CRYPTO_FRAME_SYNC_BYTE_0;
            raw[1] = CRYPTO_FRAME_SYNC_BYTE_1;
            break;
        }
        first_sync_seen = byte == CRYPTO_FRAME_SYNC_BYTE_0;
    }

    if(!OtaApp_ReadExact(
           &raw[CRYPTO_FRAME_SESSION_ID_OFFSET],
           CRYPTO_FRAME_CIPHERTEXT_OFFSET - CRYPTO_FRAME_SESSION_ID_OFFSET,
           OTA_APP_FRAME_TIMEOUT_MS
       )){
        return false;
    }
    payload_length = (uint16_t)raw[CRYPTO_FRAME_LENGTH_OFFSET] |
        (uint16_t)((uint16_t)raw[CRYPTO_FRAME_LENGTH_OFFSET + 1U] << 8U);
    if(payload_length > CRYPTO_FRAME_MAX_PAYLOAD_SIZE){
        return false;
    }
    remaining = (uint16_t)(payload_length +
        CRYPTO_AEAD_TAG_SIZE + CRYPTO_FRAME_CRC32_SIZE);
    if(!OtaApp_ReadExact(
           &raw[CRYPTO_FRAME_CIPHERTEXT_OFFSET],
           remaining,
           OTA_APP_FRAME_TIMEOUT_MS
       )){
        return false;
    }
    *raw_length = CRYPTO_FRAME_RAW_SIZE(payload_length);
    return true;
}

static void OtaApp_RecoveryLoop(void){
    for(;;){
        __asm volatile ("wfi");
    }
}

int main(void){
    static OtaAgent_Context_t agent;
    static uint8_t request[CRYPTO_FRAME_MAX_RAW_SIZE];
    static uint8_t response[CRYPTO_FRAME_MAX_RAW_SIZE];
    uint8_t transport_key[CRYPTO_TRANSPORT_KEY_SIZE];
    uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE];

    SCB->VTOR = APPLICATION_BASE_ADDRESS;

    if(!ApplicationHealth_BeginTrialWatchdog() ||
       !DeviceKeys_GetTransportKey(transport_key) ||
       !DeviceKeys_GetReleasePublicKey(public_key) ||
       (LoraAs32_Init() != LORA_AS32_OK) ||
       (OtaAgent_Init(
            &agent,
            transport_key,
            public_key
        ) != OTA_AGENT_OK)){
        OtaApp_RecoveryLoop();
    }

    /*
     * Chỉ confirm Trial sau khi key, AS32, W25Q32 và Update Agent đã init.
     * Project thật có thể bổ sung self-test riêng ngay trước checkpoint này.
     */
    if(!ApplicationHealth_ConfirmTrialIfNeeded()){
        OtaApp_RecoveryLoop();
    }

    for(;;){
        size_t request_length;
        size_t response_length;
        bool reset_requested;

        Watchdog_Feed();
        if(!OtaApp_ReceiveFrame(request, &request_length)){
            continue;
        }
        if(OtaAgent_ProcessFrame(
               &agent,
               request,
               request_length,
               response,
               sizeof(response),
               &response_length,
               &reset_requested
           ) != OTA_AGENT_OK){
            continue;
        }
        if(LoraAs32_Send(
               response,
               (uint16_t)response_length,
               OTA_APP_FRAME_TIMEOUT_MS
           ) != LORA_AS32_OK){
            continue;
        }
        if(reset_requested){
            NVIC_SystemReset();
        }
    }
}
