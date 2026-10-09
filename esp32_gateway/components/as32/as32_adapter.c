#include "as32_adapter.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AS32_COMMAND_WRITE_PERSISTENT           UINT8_C(0xC0)
#define AS32_COMMAND_READ_CONFIG                UINT8_C(0xC1)
#define AS32_COMMAND_WRITE_VOLATILE             UINT8_C(0xC2)
#define AS32_COMMAND_READ_VERSION               UINT8_C(0xC3)
#define AS32_COMMAND_RESET                      UINT8_C(0xC4)

#define AS32_UART_BAUD_9600_BITS                UINT8_C(0x18)
#define AS32_UART_BAUD_MASK                     UINT8_C(0x38)
#define AS32_UART_PARITY_MASK                   UINT8_C(0xC0)
#define AS32_UART_PARITY_8N1_ALT                UINT8_C(0xC0)
#define AS32_AIR_RATE_MASK                      UINT8_C(0x07)
#define AS32_AIR_RATE_MAX                       UINT8_C(0x05)
#define AS32_CHANNEL_MAX                        UINT8_C(0x1F)
#define AS32_OPTION_RESERVED_MASK               UINT8_C(0x04)

#define AS32_UART_RX_BUFFER_SIZE                1024
#define AS32_AUX_STABLE_TIME_MS                 2U
#define AS32_MODE_GUARD_TIME_MS                 5U
#define AS32_RESPONSE_IDLE_TIME_MS              10U

static As32Adapter_Hardware_t as32_hardware;
static As32Adapter_Status_t as32_status = AS32_ADAPTER_NOT_READY;
static As32Adapter_Mode_t as32_mode = AS32_ADAPTER_MODE_NORMAL;
static bool as32_initialized = false;

/* ==================== Helper private: status/time/config ==================== */

static As32Adapter_Status_t As32Adapter_RecordStatus(
    As32Adapter_Status_t status
){
    as32_status = status;
    return status;
}

static TickType_t As32Adapter_TimeoutTicks(uint32_t timeout_ms){
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);

    return ticks == 0U ? 1U : ticks;
}

static int64_t As32Adapter_NowMs(void){
    return esp_timer_get_time() / 1000;
}

static bool As32Adapter_ConfigIsSupported(
    const As32Adapter_Config_t *config
){
    uint8_t parity;

    if(config == NULL){
        return false;
    }

    parity = config->speed & AS32_UART_PARITY_MASK;
    if((config->speed & AS32_UART_BAUD_MASK) !=
       AS32_UART_BAUD_9600_BITS){
        return false;
    }
    if((parity != 0U) && (parity != AS32_UART_PARITY_8N1_ALT)){
        return false;
    }
    if((config->speed & AS32_AIR_RATE_MASK) > AS32_AIR_RATE_MAX){
        return false;
    }
    if(config->channel > AS32_CHANNEL_MAX){
        return false;
    }

    return (config->option & AS32_OPTION_RESERVED_MASK) == 0U;
}

static bool As32Adapter_ConfigEquals(
    const As32Adapter_Config_t *left,
    const As32Adapter_Config_t *right
){
    return (left->address_high == right->address_high) &&
           (left->address_low == right->address_low) &&
           (left->speed == right->speed) &&
           (left->channel == right->channel) &&
           (left->option == right->option);
}

/* ==================== Helper private: mode/UART response ==================== */

static As32Adapter_Status_t As32Adapter_SetMode(
    As32Adapter_Mode_t mode,
    uint32_t timeout_ms
){
    As32Adapter_Status_t status;

    if(!as32_initialized){
        return As32Adapter_RecordStatus(AS32_ADAPTER_NOT_READY);
    }
    if(timeout_ms == 0U){
        return As32Adapter_RecordStatus(AS32_ADAPTER_INVALID_ARGUMENT);
    }

    status = As32Adapter_WaitAuxReady(timeout_ms);
    if(status != AS32_ADAPTER_OK){
        return status;
    }

    gpio_set_level(
        as32_hardware.md0_pin,
        mode == AS32_ADAPTER_MODE_CONFIG ? 1 : 0
    );
    gpio_set_level(
        as32_hardware.md1_pin,
        mode == AS32_ADAPTER_MODE_CONFIG ? 1 : 0
    );
    vTaskDelay(As32Adapter_TimeoutTicks(AS32_MODE_GUARD_TIME_MS));

    status = As32Adapter_WaitAuxReady(timeout_ms);
    if(status == AS32_ADAPTER_OK){
        as32_mode = mode;
    }
    return status;
}

static As32Adapter_Status_t As32Adapter_ReceiveUntilIdle(
    uint8_t *data,
    size_t capacity,
    size_t *received_length,
    uint32_t timeout_ms
){
    const int64_t start_ms = As32Adapter_NowMs();
    int64_t last_byte_ms = start_ms;
    size_t count = 0U;

    if((data == NULL) || (capacity == 0U) ||
       (received_length == NULL) || (timeout_ms == 0U)){
        return As32Adapter_RecordStatus(AS32_ADAPTER_INVALID_ARGUMENT);
    }

    while((uint64_t)(As32Adapter_NowMs() - start_ms) < timeout_ms){
        int read_count = uart_read_bytes(
            as32_hardware.uart_port,
            &data[count],
            1U,
            As32Adapter_TimeoutTicks(AS32_RESPONSE_IDLE_TIME_MS)
        );

        if(read_count < 0){
            *received_length = count;
            return As32Adapter_RecordStatus(AS32_ADAPTER_UART_ERROR);
        }
        if(read_count == 1){
            ++count;
            last_byte_ms = As32Adapter_NowMs();
            if(count == capacity){
                *received_length = count;
                return As32Adapter_RecordStatus(AS32_ADAPTER_OK);
            }
        }else if((count > 0U) &&
                 ((uint64_t)(As32Adapter_NowMs() - last_byte_ms) >=
                  AS32_RESPONSE_IDLE_TIME_MS)){
            *received_length = count;
            return As32Adapter_RecordStatus(AS32_ADAPTER_OK);
        }
    }

    *received_length = count;
    return As32Adapter_RecordStatus(AS32_ADAPTER_TIMEOUT);
}

/* ==================== API public ==================== */

As32Adapter_Status_t As32Adapter_Init(
    const As32Adapter_Hardware_t *hardware
){
    uart_config_t uart_config;
    gpio_config_t output_config;
    gpio_config_t input_config;

    if((hardware == NULL) ||
       (hardware->baud_rate != 9600U) ||
       ((unsigned int)hardware->uart_port >=
        (unsigned int)UART_NUM_MAX) ||
       !GPIO_IS_VALID_OUTPUT_GPIO(hardware->tx_pin) ||
       !GPIO_IS_VALID_GPIO(hardware->rx_pin) ||
       !GPIO_IS_VALID_OUTPUT_GPIO(hardware->md0_pin) ||
       !GPIO_IS_VALID_OUTPUT_GPIO(hardware->md1_pin) ||
       !GPIO_IS_VALID_GPIO(hardware->aux_pin)){
        return As32Adapter_RecordStatus(AS32_ADAPTER_INVALID_ARGUMENT);
    }

    uart_config = (uart_config_t){
        .baud_rate = (int)hardware->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };
    output_config = (gpio_config_t){
        .pin_bit_mask =
            (UINT64_C(1) << hardware->md0_pin) |
            (UINT64_C(1) << hardware->md1_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    input_config = (gpio_config_t){
        .pin_bit_mask = UINT64_C(1) << hardware->aux_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    as32_hardware = *hardware;
    if(uart_param_config(hardware->uart_port, &uart_config) != ESP_OK){
        return As32Adapter_RecordStatus(AS32_ADAPTER_IO_ERROR);
    }
    if(uart_set_pin(
           hardware->uart_port,
           hardware->tx_pin,
           hardware->rx_pin,
           UART_PIN_NO_CHANGE,
           UART_PIN_NO_CHANGE
       ) != ESP_OK){
        return As32Adapter_RecordStatus(AS32_ADAPTER_IO_ERROR);
    }
    if(uart_driver_install(
           hardware->uart_port,
           AS32_UART_RX_BUFFER_SIZE,
           0,
           0,
           NULL,
           0
       ) != ESP_OK){
        return As32Adapter_RecordStatus(AS32_ADAPTER_IO_ERROR);
    }
    if((gpio_config(&output_config) != ESP_OK) ||
       (gpio_config(&input_config) != ESP_OK)){
        uart_driver_delete(hardware->uart_port);
        return As32Adapter_RecordStatus(AS32_ADAPTER_IO_ERROR);
    }

    gpio_set_level(hardware->md0_pin, 0);
    gpio_set_level(hardware->md1_pin, 0);
    as32_initialized = true;
    as32_mode = AS32_ADAPTER_MODE_NORMAL;
    as32_status = AS32_ADAPTER_OK;

    return As32Adapter_EnterNormal(AS32_ADAPTER_DEFAULT_TIMEOUT_MS);
}

As32Adapter_Status_t As32Adapter_WaitAuxReady(uint32_t timeout_ms){
    const int64_t start_ms = As32Adapter_NowMs();

    if(!as32_initialized){
        return As32Adapter_RecordStatus(AS32_ADAPTER_NOT_READY);
    }
    if(timeout_ms == 0U){
        return gpio_get_level(as32_hardware.aux_pin) != 0 ?
            As32Adapter_RecordStatus(AS32_ADAPTER_OK) :
            As32Adapter_RecordStatus(AS32_ADAPTER_BUSY);
    }

    while((uint64_t)(As32Adapter_NowMs() - start_ms) < timeout_ms){
        if(gpio_get_level(as32_hardware.aux_pin) != 0){
            vTaskDelay(As32Adapter_TimeoutTicks(AS32_AUX_STABLE_TIME_MS));
            if(gpio_get_level(as32_hardware.aux_pin) != 0){
                return As32Adapter_RecordStatus(AS32_ADAPTER_OK);
            }
        }
        vTaskDelay(1U);
    }

    return As32Adapter_RecordStatus(AS32_ADAPTER_TIMEOUT);
}

As32Adapter_Status_t As32Adapter_EnterConfig(uint32_t timeout_ms){
    return As32Adapter_SetMode(AS32_ADAPTER_MODE_CONFIG, timeout_ms);
}

As32Adapter_Status_t As32Adapter_EnterNormal(uint32_t timeout_ms){
    return As32Adapter_SetMode(AS32_ADAPTER_MODE_NORMAL, timeout_ms);
}

As32Adapter_Status_t As32Adapter_Send(
    const uint8_t *data,
    size_t length,
    uint32_t timeout_ms
){
    As32Adapter_Status_t status;
    int written;

    if(!as32_initialized){
        return As32Adapter_RecordStatus(AS32_ADAPTER_NOT_READY);
    }
    if((data == NULL) || (length == 0U) || (timeout_ms == 0U)){
        return As32Adapter_RecordStatus(AS32_ADAPTER_INVALID_ARGUMENT);
    }
    if(length > AS32_ADAPTER_MAX_TRANSFER_SIZE){
        return As32Adapter_RecordStatus(AS32_ADAPTER_OUT_OF_RANGE);
    }

    status = As32Adapter_WaitAuxReady(timeout_ms);
    if(status != AS32_ADAPTER_OK){
        return status;
    }

    written = uart_write_bytes(as32_hardware.uart_port, data, length);
    if((written < 0) || ((size_t)written != length)){
        return As32Adapter_RecordStatus(AS32_ADAPTER_UART_ERROR);
    }
    if(uart_wait_tx_done(
           as32_hardware.uart_port,
           As32Adapter_TimeoutTicks(timeout_ms)
       ) != ESP_OK){
        return As32Adapter_RecordStatus(AS32_ADAPTER_TIMEOUT);
    }

    return As32Adapter_WaitAuxReady(timeout_ms);
}

As32Adapter_Status_t As32Adapter_Receive(
    uint8_t *data,
    size_t requested_length,
    size_t *received_length,
    uint32_t timeout_ms
){
    int read_count;

    if(!as32_initialized){
        return As32Adapter_RecordStatus(AS32_ADAPTER_NOT_READY);
    }
    if((data == NULL) || (requested_length == 0U) ||
       (received_length == NULL) || (timeout_ms == 0U)){
        return As32Adapter_RecordStatus(AS32_ADAPTER_INVALID_ARGUMENT);
    }
    if(requested_length > AS32_ADAPTER_MAX_TRANSFER_SIZE){
        return As32Adapter_RecordStatus(AS32_ADAPTER_OUT_OF_RANGE);
    }

    read_count = uart_read_bytes(
        as32_hardware.uart_port,
        data,
        requested_length,
        As32Adapter_TimeoutTicks(timeout_ms)
    );
    if(read_count < 0){
        *received_length = 0U;
        return As32Adapter_RecordStatus(AS32_ADAPTER_UART_ERROR);
    }

    *received_length = (size_t)read_count;
    return (size_t)read_count == requested_length ?
        As32Adapter_RecordStatus(AS32_ADAPTER_OK) :
        As32Adapter_RecordStatus(AS32_ADAPTER_TIMEOUT);
}

size_t As32Adapter_Available(void){
    size_t available = 0U;

    if(!as32_initialized){
        return 0U;
    }
    if(uart_get_buffered_data_len(
           as32_hardware.uart_port,
           &available
       ) != ESP_OK){
        As32Adapter_RecordStatus(AS32_ADAPTER_UART_ERROR);
        return 0U;
    }
    return available;
}

void As32Adapter_FlushRx(void){
    if(!as32_initialized){
        as32_status = AS32_ADAPTER_NOT_READY;
        return;
    }
    if(uart_flush_input(as32_hardware.uart_port) != ESP_OK){
        as32_status = AS32_ADAPTER_UART_ERROR;
    }else{
        as32_status = AS32_ADAPTER_OK;
    }
}

As32Adapter_Status_t As32Adapter_ReadConfig(
    As32Adapter_Config_t *config,
    uint32_t timeout_ms
){
    static const uint8_t command[3] = {
        AS32_COMMAND_READ_CONFIG,
        AS32_COMMAND_READ_CONFIG,
        AS32_COMMAND_READ_CONFIG
    };
    uint8_t response[AS32_ADAPTER_RESPONSE_SIZE];
    size_t received_length = 0U;
    As32Adapter_Status_t status;

    if(config == NULL){
        return As32Adapter_RecordStatus(AS32_ADAPTER_INVALID_ARGUMENT);
    }
    if(!as32_initialized ||
       (as32_mode != AS32_ADAPTER_MODE_CONFIG)){
        return As32Adapter_RecordStatus(AS32_ADAPTER_NOT_READY);
    }

    As32Adapter_FlushRx();
    status = As32Adapter_Send(command, sizeof(command), timeout_ms);
    if(status == AS32_ADAPTER_OK){
        status = As32Adapter_Receive(
            response,
            sizeof(response),
            &received_length,
            timeout_ms
        );
    }
    if(status != AS32_ADAPTER_OK){
        return status;
    }
    if((received_length != sizeof(response)) ||
       (response[0] != AS32_COMMAND_WRITE_PERSISTENT)){
        return As32Adapter_RecordStatus(AS32_ADAPTER_CONFIG_ERROR);
    }

    config->address_high = response[1];
    config->address_low = response[2];
    config->speed = response[3];
    config->channel = response[4];
    config->option = response[5];

    return As32Adapter_RecordStatus(
        As32Adapter_ConfigIsSupported(config) ?
            AS32_ADAPTER_OK : AS32_ADAPTER_CONFIG_ERROR
    );
}

As32Adapter_Status_t As32Adapter_WriteConfig(
    const As32Adapter_Config_t *config,
    bool persist,
    uint32_t timeout_ms
){
    uint8_t command[AS32_ADAPTER_RESPONSE_SIZE];
    uint8_t response[2];
    size_t received_length = 0U;
    As32Adapter_Config_t read_back;
    As32Adapter_Status_t status;

    if(!as32_initialized ||
       (as32_mode != AS32_ADAPTER_MODE_CONFIG)){
        return As32Adapter_RecordStatus(AS32_ADAPTER_NOT_READY);
    }
    if(!As32Adapter_ConfigIsSupported(config)){
        return As32Adapter_RecordStatus(
            config == NULL ?
                AS32_ADAPTER_INVALID_ARGUMENT :
                AS32_ADAPTER_OUT_OF_RANGE
        );
    }

    command[0] = persist ?
        AS32_COMMAND_WRITE_PERSISTENT :
        AS32_COMMAND_WRITE_VOLATILE;
    command[1] = config->address_high;
    command[2] = config->address_low;
    command[3] = config->speed;
    command[4] = config->channel;
    command[5] = config->option;

    As32Adapter_FlushRx();
    status = As32Adapter_Send(command, sizeof(command), timeout_ms);
    if(status == AS32_ADAPTER_OK){
        status = As32Adapter_Receive(
            response,
            sizeof(response),
            &received_length,
            timeout_ms
        );
    }
    if(status != AS32_ADAPTER_OK){
        return status;
    }
    if((received_length != sizeof(response)) ||
       (response[0] != (uint8_t)'O') ||
       (response[1] != (uint8_t)'K')){
        return As32Adapter_RecordStatus(AS32_ADAPTER_CONFIG_ERROR);
    }

    status = As32Adapter_ReadConfig(&read_back, timeout_ms);
    if(status != AS32_ADAPTER_OK){
        return status;
    }
    return As32Adapter_RecordStatus(
        As32Adapter_ConfigEquals(config, &read_back) ?
            AS32_ADAPTER_OK : AS32_ADAPTER_CONFIG_ERROR
    );
}

As32Adapter_Status_t As32Adapter_ReadVersion(
    uint8_t *version,
    size_t capacity,
    size_t *version_length,
    uint32_t timeout_ms
){
    static const uint8_t command[3] = {
        AS32_COMMAND_READ_VERSION,
        AS32_COMMAND_READ_VERSION,
        AS32_COMMAND_READ_VERSION
    };
    As32Adapter_Status_t status;

    if(!as32_initialized ||
       (as32_mode != AS32_ADAPTER_MODE_CONFIG)){
        return As32Adapter_RecordStatus(AS32_ADAPTER_NOT_READY);
    }
    if((version == NULL) || (version_length == NULL) ||
       (capacity == 0U)){
        return As32Adapter_RecordStatus(AS32_ADAPTER_INVALID_ARGUMENT);
    }
    if(capacity > AS32_ADAPTER_VERSION_MAX_SIZE){
        return As32Adapter_RecordStatus(AS32_ADAPTER_OUT_OF_RANGE);
    }

    As32Adapter_FlushRx();
    status = As32Adapter_Send(command, sizeof(command), timeout_ms);
    if(status != AS32_ADAPTER_OK){
        return status;
    }
    return As32Adapter_ReceiveUntilIdle(
        version,
        capacity,
        version_length,
        timeout_ms
    );
}

As32Adapter_Status_t As32Adapter_Reset(uint32_t timeout_ms){
    static const uint8_t command[3] = {
        AS32_COMMAND_RESET,
        AS32_COMMAND_RESET,
        AS32_COMMAND_RESET
    };
    uint8_t response[2];
    size_t received_length = 0U;
    As32Adapter_Status_t status;

    if(!as32_initialized ||
       (as32_mode != AS32_ADAPTER_MODE_CONFIG)){
        return As32Adapter_RecordStatus(AS32_ADAPTER_NOT_READY);
    }

    As32Adapter_FlushRx();
    status = As32Adapter_Send(command, sizeof(command), timeout_ms);
    if(status == AS32_ADAPTER_OK){
        status = As32Adapter_Receive(
            response,
            sizeof(response),
            &received_length,
            timeout_ms
        );
    }
    if(status != AS32_ADAPTER_OK){
        return status;
    }
    if((received_length != sizeof(response)) ||
       (response[0] != (uint8_t)'O') ||
       (response[1] != (uint8_t)'K')){
        return As32Adapter_RecordStatus(AS32_ADAPTER_CONFIG_ERROR);
    }
    return As32Adapter_WaitAuxReady(timeout_ms);
}

As32Adapter_Status_t As32Adapter_GetStatus(void){
    return as32_status;
}

void As32Adapter_ClearError(void){
    as32_status = as32_initialized ?
        AS32_ADAPTER_OK : AS32_ADAPTER_NOT_READY;
}
