#include "as32_adapter.h"
#include "phase4_link_packet.h"
#include "radio_task.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PHASE4_RESULT_TIMEOUT_MS                 15000U
#define PHASE4_QUEUE_TIMEOUT_MS                  100U

static const char *TAG = "aegis_phase4";

static void Phase4_Fatal(
    const char *step,
    uint32_t status
){
    ESP_LOGE(TAG, "%s thất bại, status=%" PRIu32, step, status);
    for(;;){
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}

static void Phase4_LogProfile(
    const As32Adapter_Config_t *config,
    const uint8_t *version,
    size_t version_length
){
    ESP_LOGI(
        TAG,
        "AS32 config: ADDH=%02X ADDL=%02X SPEED=%02X CHAN=%02X OPTION=%02X",
        config->address_high,
        config->address_low,
        config->speed,
        config->channel,
        config->option
    );
    ESP_LOGI(TAG, "AS32 version response (%u byte)", (unsigned)version_length);
    ESP_LOG_BUFFER_HEX_LEVEL(
        TAG,
        version,
        version_length,
        ESP_LOG_INFO
    );
}

void app_main(void){
    static const uint16_t test_lengths[] = {
        0U,
        1U,
        PHASE4_LINK_MAX_PAYLOAD_SIZE - 1U,
        PHASE4_LINK_MAX_PAYLOAD_SIZE
    };
    const As32Adapter_Hardware_t hardware = {
        .uart_port = (uart_port_t)CONFIG_AEGIS_AS32_UART_PORT,
        .tx_pin = (gpio_num_t)CONFIG_AEGIS_AS32_TX_PIN,
        .rx_pin = (gpio_num_t)CONFIG_AEGIS_AS32_RX_PIN,
        .md0_pin = (gpio_num_t)CONFIG_AEGIS_AS32_MD0_PIN,
        .md1_pin = (gpio_num_t)CONFIG_AEGIS_AS32_MD1_PIN,
        .aux_pin = (gpio_num_t)CONFIG_AEGIS_AS32_AUX_PIN,
        .baud_rate = CONFIG_AEGIS_AS32_UART_BAUD
    };
    As32Adapter_Config_t config;
    uint8_t version[AS32_ADAPTER_VERSION_MAX_SIZE];
    size_t version_length = 0U;
    RadioTask_Request_t request;
    RadioTask_Result_t result;
    As32Adapter_Status_t as32_status;
    RadioTask_Status_t radio_status;
    uint32_t counter = 1U;
    uint32_t success_count = 0U;
    uint32_t failure_count = 0U;
    uint64_t success_payload_bytes = 0U;
    int64_t total_round_trip_us = 0;

    ESP_LOGI(
        TAG,
        "UART%d TX=%d RX=%d MD0=%d MD1=%d AUX=%d baud=%d",
        CONFIG_AEGIS_AS32_UART_PORT,
        CONFIG_AEGIS_AS32_TX_PIN,
        CONFIG_AEGIS_AS32_RX_PIN,
        CONFIG_AEGIS_AS32_MD0_PIN,
        CONFIG_AEGIS_AS32_MD1_PIN,
        CONFIG_AEGIS_AS32_AUX_PIN,
        CONFIG_AEGIS_AS32_UART_BAUD
    );

    as32_status = As32Adapter_Init(&hardware);
    if(as32_status != AS32_ADAPTER_OK){
        Phase4_Fatal("As32Adapter_Init", (uint32_t)as32_status);
    }
    as32_status = As32Adapter_EnterConfig(
        AS32_ADAPTER_DEFAULT_TIMEOUT_MS
    );
    if(as32_status != AS32_ADAPTER_OK){
        Phase4_Fatal("EnterConfig", (uint32_t)as32_status);
    }
    as32_status = As32Adapter_ReadConfig(
        &config,
        AS32_ADAPTER_DEFAULT_TIMEOUT_MS
    );
    if(as32_status != AS32_ADAPTER_OK){
        Phase4_Fatal("ReadConfig", (uint32_t)as32_status);
    }
    as32_status = As32Adapter_ReadVersion(
        version,
        sizeof(version),
        &version_length,
        AS32_ADAPTER_DEFAULT_TIMEOUT_MS
    );
    if(as32_status != AS32_ADAPTER_OK){
        Phase4_Fatal("ReadVersion", (uint32_t)as32_status);
    }
    Phase4_LogProfile(&config, version, version_length);

    as32_status = As32Adapter_EnterNormal(
        AS32_ADAPTER_DEFAULT_TIMEOUT_MS
    );
    if(as32_status != AS32_ADAPTER_OK){
        Phase4_Fatal("EnterNormal", (uint32_t)as32_status);
    }
    radio_status = RadioTask_Start();
    if(radio_status != RADIO_TASK_OK){
        Phase4_Fatal("RadioTask_Start", (uint32_t)radio_status);
    }

    for(uint32_t round = 0U;
        round < CONFIG_AEGIS_PHASE4_ROUNDS;
        ++round){
        for(size_t case_index = 0U;
            case_index < (sizeof(test_lengths) / sizeof(test_lengths[0]));
            ++case_index){
            memset(&request, 0, sizeof(request));
            request.counter = counter++;
            request.payload_length = test_lengths[case_index];
            for(uint16_t index = 0U;
                index < request.payload_length;
                ++index){
                request.payload[index] = (uint8_t)(
                    index ^ request.counter ^ round
                );
            }
            memset(&result, 0, sizeof(result));
            result.status = RADIO_TASK_NOT_READY;
            result.as32_status = AS32_ADAPTER_NOT_READY;
            result.packet_status = PHASE4_LINK_INVALID_ARGUMENT;

            radio_status = RadioTask_Submit(
                &request,
                PHASE4_QUEUE_TIMEOUT_MS
            );
            if(radio_status == RADIO_TASK_OK){
                radio_status = RadioTask_WaitResult(
                    &result,
                    PHASE4_RESULT_TIMEOUT_MS
                );
            }

            if((radio_status == RADIO_TASK_OK) &&
               (result.status == RADIO_TASK_OK)){
                ++success_count;
                success_payload_bytes += result.payload_length;
                total_round_trip_us += result.round_trip_us;
                ESP_LOGI(
                    TAG,
                    "PASS counter=%" PRIu32
                    " payload=%u RTT=%" PRIi64 " us",
                    result.counter,
                    result.payload_length,
                    result.round_trip_us
                );
            }else{
                ++failure_count;
                ESP_LOGE(
                    TAG,
                    "FAIL counter=%" PRIu32
                    " queue=%u radio=%u as32=%u packet=%u",
                    request.counter,
                    (unsigned)radio_status,
                    (unsigned)result.status,
                    (unsigned)result.as32_status,
                    (unsigned)result.packet_status
                );
            }
        }
    }

    const uint32_t total_count = success_count + failure_count;
    const uint32_t success_per_mille = total_count == 0U ?
        0U : (success_count * 1000U) / total_count;
    const uint64_t payload_bps = total_round_trip_us <= 0 ?
        0U :
        (success_payload_bytes * UINT64_C(8000000)) /
            (uint64_t)total_round_trip_us;

    ESP_LOGI(
        TAG,
        "RESULT success=%" PRIu32 "/%" PRIu32
        " success=%" PRIu32 ".%" PRIu32
        "%% payload-throughput=%" PRIu64 " bit/s",
        success_count,
        total_count,
        success_per_mille / 10U,
        success_per_mille % 10U,
        payload_bps
    );
    ESP_LOGI(
        TAG,
        "Ghi profile/môi trường/khoảng cách và log này vào evidence Phase 4."
    );
}
