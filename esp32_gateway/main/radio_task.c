#include "radio_task.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <string.h>

#define RADIO_TASK_QUEUE_LENGTH                  8U
#define RADIO_TASK_STACK_SIZE                    4096U
#define RADIO_TASK_PRIORITY                      8U
#define RADIO_TASK_IO_TIMEOUT_MS                 5000U

static QueueHandle_t radio_request_queue = NULL;
static QueueHandle_t radio_result_queue = NULL;

static TickType_t RadioTask_TimeoutTicks(uint32_t timeout_ms){
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);

    return ticks == 0U ? 1U : ticks;
}

static As32Adapter_Status_t RadioTask_ReceiveOne(uint8_t *byte){
    size_t received_length = 0U;

    return As32Adapter_Receive(
        byte,
        1U,
        &received_length,
        RADIO_TASK_IO_TIMEOUT_MS
    );
}

static As32Adapter_Status_t RadioTask_ReceiveRaw(
    uint8_t raw[PHASE4_LINK_MAX_RAW_SIZE],
    size_t *raw_length
){
    bool first_magic_seen = false;
    uint8_t byte;
    uint16_t payload_length;
    size_t received_length;
    size_t remaining_length;
    As32Adapter_Status_t status;

    for(;;){
        status = RadioTask_ReceiveOne(&byte);
        if(status != AS32_ADAPTER_OK){
            return status;
        }
        if(!first_magic_seen){
            first_magic_seen = byte == PHASE4_LINK_MAGIC_0;
            continue;
        }
        if(byte == PHASE4_LINK_MAGIC_1){
            raw[0] = PHASE4_LINK_MAGIC_0;
            raw[1] = PHASE4_LINK_MAGIC_1;
            break;
        }
        first_magic_seen = byte == PHASE4_LINK_MAGIC_0;
    }

    status = As32Adapter_Receive(
        &raw[2],
        PHASE4_LINK_HEADER_SIZE - 2U,
        &received_length,
        RADIO_TASK_IO_TIMEOUT_MS
    );
    if(status != AS32_ADAPTER_OK){
        return status;
    }

    payload_length = (uint16_t)raw[PHASE4_LINK_LENGTH_OFFSET] |
        (uint16_t)(
            (uint16_t)raw[PHASE4_LINK_LENGTH_OFFSET + 1U] << 8U
        );
    if(payload_length > PHASE4_LINK_MAX_PAYLOAD_SIZE){
        return AS32_ADAPTER_OUT_OF_RANGE;
    }

    remaining_length = payload_length + PHASE4_LINK_CRC_SIZE;
    status = As32Adapter_Receive(
        &raw[PHASE4_LINK_HEADER_SIZE],
        remaining_length,
        &received_length,
        RADIO_TASK_IO_TIMEOUT_MS
    );
    if(status != AS32_ADAPTER_OK){
        return status;
    }

    *raw_length = PHASE4_LINK_HEADER_SIZE + remaining_length;
    return AS32_ADAPTER_OK;
}

static void RadioTask_Run(void *argument){
    RadioTask_Request_t request;
    RadioTask_Result_t result;
    Phase4Link_Packet_t packet;
    uint8_t raw_tx[PHASE4_LINK_MAX_RAW_SIZE];
    uint8_t raw_rx[PHASE4_LINK_MAX_RAW_SIZE];
    size_t raw_tx_length;
    size_t raw_rx_length;

    (void)argument;

    for(;;){
        if(xQueueReceive(
               radio_request_queue,
               &request,
               portMAX_DELAY
           ) != pdTRUE){
            continue;
        }

        memset(&result, 0, sizeof(result));
        result.counter = request.counter;
        result.payload_length = request.payload_length;
        result.as32_status = AS32_ADAPTER_OK;
        result.packet_status = PHASE4_LINK_OK;

        const int64_t started_us = esp_timer_get_time();

        result.packet_status = Phase4Link_Encode(
            PHASE4_LINK_GATEWAY_TO_NODE,
            request.counter,
            request.payload,
            request.payload_length,
            raw_tx,
            sizeof(raw_tx),
            &raw_tx_length
        );
        if(result.packet_status != PHASE4_LINK_OK){
            result.status = RADIO_TASK_PACKET_ERROR;
        }else{
            As32Adapter_FlushRx();
            result.as32_status = As32Adapter_Send(
                raw_tx,
                raw_tx_length,
                RADIO_TASK_IO_TIMEOUT_MS
            );
            if(result.as32_status != AS32_ADAPTER_OK){
                result.status = RADIO_TASK_AS32_ERROR;
            }else{
                result.as32_status = RadioTask_ReceiveRaw(
                    raw_rx,
                    &raw_rx_length
                );
                if(result.as32_status != AS32_ADAPTER_OK){
                    result.status = result.as32_status ==
                        AS32_ADAPTER_TIMEOUT ?
                            RADIO_TASK_TIMEOUT :
                            RADIO_TASK_AS32_ERROR;
                }else{
                    result.packet_status = Phase4Link_Decode(
                        raw_rx,
                        raw_rx_length,
                        &packet
                    );
                    if(result.packet_status != PHASE4_LINK_OK){
                        result.status = RADIO_TASK_PACKET_ERROR;
                    }else if(
                        (packet.direction !=
                         PHASE4_LINK_NODE_TO_GATEWAY) ||
                        (packet.counter != request.counter) ||
                        (packet.payload_length !=
                         request.payload_length) ||
                        (memcmp(
                             packet.payload,
                             request.payload,
                             request.payload_length
                         ) != 0)
                    ){
                        result.status = RADIO_TASK_ECHO_MISMATCH;
                    }else{
                        result.status = RADIO_TASK_OK;
                    }
                }
            }
        }

        result.round_trip_us = esp_timer_get_time() - started_us;
        (void)xQueueSend(radio_result_queue, &result, 0U);
    }
}

RadioTask_Status_t RadioTask_Start(void){
    if((radio_request_queue != NULL) || (radio_result_queue != NULL)){
        return RADIO_TASK_NOT_READY;
    }

    radio_request_queue = xQueueCreate(
        RADIO_TASK_QUEUE_LENGTH,
        sizeof(RadioTask_Request_t)
    );
    radio_result_queue = xQueueCreate(
        RADIO_TASK_QUEUE_LENGTH,
        sizeof(RadioTask_Result_t)
    );
    if((radio_request_queue == NULL) || (radio_result_queue == NULL)){
        return RADIO_TASK_NOT_READY;
    }
    if(xTaskCreate(
           RadioTask_Run,
           "aegis_radio",
           RADIO_TASK_STACK_SIZE,
           NULL,
           RADIO_TASK_PRIORITY,
           NULL
       ) != pdPASS){
        return RADIO_TASK_NOT_READY;
    }
    return RADIO_TASK_OK;
}

RadioTask_Status_t RadioTask_Submit(
    const RadioTask_Request_t *request,
    uint32_t timeout_ms
){
    if((request == NULL) ||
       (request->payload_length > PHASE4_LINK_MAX_PAYLOAD_SIZE)){
        return RADIO_TASK_INVALID_ARGUMENT;
    }
    if(radio_request_queue == NULL){
        return RADIO_TASK_NOT_READY;
    }
    return xQueueSend(
        radio_request_queue,
        request,
        RadioTask_TimeoutTicks(timeout_ms)
    ) == pdTRUE ? RADIO_TASK_OK : RADIO_TASK_QUEUE_FULL;
}

RadioTask_Status_t RadioTask_WaitResult(
    RadioTask_Result_t *result,
    uint32_t timeout_ms
){
    if(result == NULL){
        return RADIO_TASK_INVALID_ARGUMENT;
    }
    if(radio_result_queue == NULL){
        return RADIO_TASK_NOT_READY;
    }
    return xQueueReceive(
        radio_result_queue,
        result,
        RadioTask_TimeoutTicks(timeout_ms)
    ) == pdTRUE ? RADIO_TASK_OK : RADIO_TASK_TIMEOUT;
}
