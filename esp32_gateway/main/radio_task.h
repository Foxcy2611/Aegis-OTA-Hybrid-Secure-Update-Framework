#ifndef AEGIS_ESP32_RADIO_TASK_H
#define AEGIS_ESP32_RADIO_TASK_H

#include "as32_adapter.h"
#include "phase4_link_packet.h"

#include <stddef.h>
#include <stdint.h>

typedef enum{
    RADIO_TASK_OK = 0U,
    RADIO_TASK_INVALID_ARGUMENT,
    RADIO_TASK_NOT_READY,
    RADIO_TASK_QUEUE_FULL,
    RADIO_TASK_TIMEOUT,
    RADIO_TASK_AS32_ERROR,
    RADIO_TASK_PACKET_ERROR,
    RADIO_TASK_ECHO_MISMATCH
} RadioTask_Status_t;

typedef struct{
    uint32_t counter;
    uint16_t payload_length;
    uint8_t payload[PHASE4_LINK_MAX_PAYLOAD_SIZE];
} RadioTask_Request_t;

typedef struct{
    RadioTask_Status_t status;
    As32Adapter_Status_t as32_status;
    Phase4Link_Status_t packet_status;
    uint32_t counter;
    uint16_t payload_length;
    int64_t round_trip_us;
} RadioTask_Result_t;

RadioTask_Status_t RadioTask_Start(void);
RadioTask_Status_t RadioTask_Submit(
    const RadioTask_Request_t *request,
    uint32_t timeout_ms
);
RadioTask_Status_t RadioTask_WaitResult(
    RadioTask_Result_t *result,
    uint32_t timeout_ms
);

#endif /* AEGIS_ESP32_RADIO_TASK_H */
