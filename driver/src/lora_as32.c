#include "lora_as32.h"

#include "pinout_hw.h"
#include "timebase.h"

#include "misc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"

#include <stddef.h>

#define LORA_AS32_COMMAND_READ_CONFIG           UINT8_C(0xC1)
#define LORA_AS32_COMMAND_READ_VERSION          UINT8_C(0xC3)
#define LORA_AS32_COMMAND_RESET                 UINT8_C(0xC4)

#define LORA_AS32_RESPONSE_OK_0                 UINT8_C('O')
#define LORA_AS32_RESPONSE_OK_1                 UINT8_C('K')

#define LORA_AS32_USART_ERROR_MASK                                      \
    (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE)

_Static_assert(
    (LORA_AS32_RX_RING_CAPACITY &
     (LORA_AS32_RX_RING_CAPACITY - 1U)) == 0U,
    "AS32 RX ring capacity phải là lũy thừa của 2"
);

_Static_assert(
    LORA_AS32_RX_RING_CAPACITY <= UINT16_MAX,
    "AS32 RX ring index phải biểu diễn được bằng uint16_t"
);

static volatile uint8_t lora_rx_ring[LORA_AS32_RX_RING_CAPACITY];
static volatile uint16_t lora_rx_head = 0U;
static volatile uint16_t lora_rx_tail = 0U;
static volatile LoraAs32_Status_t lora_status = LORA_AS32_NOT_READY;
static bool lora_initialized = false;
static LoraAs32_Mode_t lora_mode = LORA_AS32_MODE_NORMAL;

/* ==================== Helper private: state/ring ==================== */

static LoraAs32_Status_t LoraAs32_RecordStatus(
    LoraAs32_Status_t status
){
    lora_status = status;
    return status;
}

static bool LoraAs32_PopRxByte(uint8_t *byte){
    uint16_t tail;

    if((byte == NULL) || (lora_rx_head == lora_rx_tail)){
        return false;
    }

    tail = lora_rx_tail;
    *byte = lora_rx_ring[tail];
    lora_rx_tail = (uint16_t)(
        (tail + 1U) & (LORA_AS32_RX_RING_CAPACITY - 1U)
    );
    return true;
}

static bool LoraAs32_ConfigEquals(
    const LoraAs32_Config_t *left,
    const LoraAs32_Config_t *right
){
    return (left->address_high == right->address_high) &&
           (left->address_low == right->address_low) &&
           (left->speed == right->speed) &&
           (left->channel == right->channel) &&
           (left->option == right->option);
}

/* ==================== Helper private: mode/UART response ==================== */

static LoraAs32_Status_t LoraAs32_SetMode(
    LoraAs32_Mode_t mode,
    uint32_t timeout_ms
){
    LoraAs32_Status_t status;

    if(!lora_initialized){
        return LoraAs32_RecordStatus(LORA_AS32_NOT_READY);
    }

    if(timeout_ms == 0U){
        return LoraAs32_RecordStatus(LORA_AS32_INVALID_ARGUMENT);
    }

    status = LoraAs32_WaitAuxReady(timeout_ms);
    if(status != LORA_AS32_OK){
        return status;
    }

    if(mode == LORA_AS32_MODE_CONFIG){
        GPIO_SetBits(AS32_MD0_PORT, AS32_MD0_PIN);
        GPIO_SetBits(AS32_MD1_PORT, AS32_MD1_PIN);
    }else{
        GPIO_ResetBits(AS32_MD0_PORT, AS32_MD0_PIN);
        GPIO_ResetBits(AS32_MD1_PORT, AS32_MD1_PIN);
    }

    Timebase_DelayMs(LORA_AS32_MODE_GUARD_TIME_MS);
    status = LoraAs32_WaitAuxReady(timeout_ms);
    if(status == LORA_AS32_OK){
        lora_mode = mode;
    }

    return status;
}

static LoraAs32_Status_t LoraAs32_ReceiveUntilIdle(
    uint8_t *data,
    uint16_t capacity,
    uint16_t *received_length,
    uint32_t timeout_ms
){
    const uint32_t operation_start = Timebase_Millis();
    uint32_t last_byte_time = operation_start;
    uint16_t count = 0U;
    bool received_any = false;

    if((data == NULL) || (capacity == 0U) ||
       (received_length == NULL) || (timeout_ms == 0U)){
        return LoraAs32_RecordStatus(LORA_AS32_INVALID_ARGUMENT);
    }

    *received_length = 0U;

    while(!Timebase_HasElapsed(operation_start, timeout_ms)){
        uint8_t byte;

        if(lora_status == LORA_AS32_RX_OVERFLOW ||
           lora_status == LORA_AS32_UART_ERROR){
            return lora_status;
        }

        if(LoraAs32_PopRxByte(&byte)){
            if(count >= capacity){
                *received_length = count;
                return LoraAs32_RecordStatus(LORA_AS32_OUT_OF_RANGE);
            }

            data[count] = byte;
            ++count;
            received_any = true;
            last_byte_time = Timebase_Millis();
        }else if(received_any &&
                 Timebase_HasElapsed(
                     last_byte_time,
                     LORA_AS32_RESPONSE_IDLE_TIME_MS
                 )){
            *received_length = count;
            return LoraAs32_RecordStatus(LORA_AS32_OK);
        }
    }

    *received_length = count;
    return LoraAs32_RecordStatus(LORA_AS32_TIMEOUT);
}

/* ==================== API public ==================== */

LoraAs32_Status_t LoraAs32_Init(void){
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef uart;
    NVIC_InitTypeDef interrupt;
    LoraAs32_Status_t status;

    if(Timebase_Init() != TIMEBASE_OK){
        return LoraAs32_RecordStatus(LORA_AS32_IO_ERROR);
    }

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_AFIO |
        RCC_APB2Periph_GPIOA |
        RCC_APB2Periph_GPIOB |
        RCC_APB2Periph_USART1,
        ENABLE
    );

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = AS32_UART_TX_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(AS32_UART_PORT, &gpio);

    gpio.GPIO_Pin = AS32_UART_RX_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(AS32_UART_PORT, &gpio);

    gpio.GPIO_Pin = AS32_MD0_PIN | AS32_MD1_PIN;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &gpio);
    GPIO_ResetBits(AS32_MD0_PORT, AS32_MD0_PIN);
    GPIO_ResetBits(AS32_MD1_PORT, AS32_MD1_PIN);

    gpio.GPIO_Pin = AS32_AUX_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(AS32_AUX_PORT, &gpio);

    USART_DeInit(AS32_UART_INSTANCE);
    USART_StructInit(&uart);
    uart.USART_BaudRate = AS32_UART_BAUDRATE;
    uart.USART_WordLength = USART_WordLength_8b;
    uart.USART_StopBits = USART_StopBits_1;
    uart.USART_Parity = USART_Parity_No;
    uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    uart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(AS32_UART_INSTANCE, &uart);

    lora_rx_head = 0U;
    lora_rx_tail = 0U;
    lora_status = LORA_AS32_OK;
    lora_mode = LORA_AS32_MODE_NORMAL;
    lora_initialized = true;

    interrupt.NVIC_IRQChannel = AS32_UART_IRQn;
    interrupt.NVIC_IRQChannelPreemptionPriority = 2U;
    interrupt.NVIC_IRQChannelSubPriority = 0U;
    interrupt.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&interrupt);

    USART_ITConfig(AS32_UART_INSTANCE, USART_IT_RXNE, ENABLE);
    USART_Cmd(AS32_UART_INSTANCE, ENABLE);

    status = LoraAs32_EnterNormal(LORA_AS32_DEFAULT_TIMEOUT_MS);
    if(status != LORA_AS32_OK){
        lora_initialized = false;
    }

    return status;
}

LoraAs32_Status_t LoraAs32_WaitAuxReady(uint32_t timeout_ms){
    const uint32_t start_ms = Timebase_Millis();

    if(!lora_initialized){
        return LoraAs32_RecordStatus(LORA_AS32_NOT_READY);
    }

    if(timeout_ms == 0U){
        return (GPIO_ReadInputDataBit(AS32_AUX_PORT, AS32_AUX_PIN) !=
                Bit_RESET) ?
                   LoraAs32_RecordStatus(LORA_AS32_OK) :
                   LoraAs32_RecordStatus(LORA_AS32_BUSY);
    }

    while(!Timebase_HasElapsed(start_ms, timeout_ms)){
        if(GPIO_ReadInputDataBit(AS32_AUX_PORT, AS32_AUX_PIN) != Bit_RESET){
            Timebase_DelayMs(LORA_AS32_AUX_STABLE_TIME_MS);

            if(GPIO_ReadInputDataBit(
                   AS32_AUX_PORT,
                   AS32_AUX_PIN
               ) != Bit_RESET){
                return LoraAs32_RecordStatus(LORA_AS32_OK);
            }
        }
    }

    return LoraAs32_RecordStatus(LORA_AS32_TIMEOUT);
}

LoraAs32_Status_t LoraAs32_EnterConfig(uint32_t timeout_ms){
    return LoraAs32_SetMode(LORA_AS32_MODE_CONFIG, timeout_ms);
}

LoraAs32_Status_t LoraAs32_EnterNormal(uint32_t timeout_ms){
    return LoraAs32_SetMode(LORA_AS32_MODE_NORMAL, timeout_ms);
}

LoraAs32_Status_t LoraAs32_Send(
    const uint8_t *data,
    uint16_t length,
    uint32_t timeout_ms
){
    const uint32_t start_ms = Timebase_Millis();
    LoraAs32_Status_t status;

    if(!lora_initialized){
        return LoraAs32_RecordStatus(LORA_AS32_NOT_READY);
    }

    if((data == NULL) || (length == 0U) || (timeout_ms == 0U)){
        return LoraAs32_RecordStatus(LORA_AS32_INVALID_ARGUMENT);
    }

    if(length > LORA_AS32_MAX_TRANSFER_SIZE){
        return LoraAs32_RecordStatus(LORA_AS32_OUT_OF_RANGE);
    }

    status = LoraAs32_WaitAuxReady(timeout_ms);
    if(status != LORA_AS32_OK){
        return status;
    }

    for(uint16_t index = 0U; index < length; ++index){
        while(USART_GetFlagStatus(
                  AS32_UART_INSTANCE,
                  USART_FLAG_TXE
              ) == RESET){
            if(Timebase_HasElapsed(start_ms, timeout_ms)){
                return LoraAs32_RecordStatus(LORA_AS32_TIMEOUT);
            }
        }

        USART_SendData(AS32_UART_INSTANCE, data[index]);
    }

    while(USART_GetFlagStatus(
              AS32_UART_INSTANCE,
              USART_FLAG_TC
          ) == RESET){
        if(Timebase_HasElapsed(start_ms, timeout_ms)){
            return LoraAs32_RecordStatus(LORA_AS32_TIMEOUT);
        }
    }

    return LoraAs32_WaitAuxReady(timeout_ms);
}

LoraAs32_Status_t LoraAs32_Receive(
    uint8_t *data,
    uint16_t requested_length,
    uint16_t *received_length,
    uint32_t timeout_ms
){
    const uint32_t start_ms = Timebase_Millis();
    uint16_t count = 0U;

    if(!lora_initialized){
        return LoraAs32_RecordStatus(LORA_AS32_NOT_READY);
    }

    if((data == NULL) || (requested_length == 0U) ||
       (received_length == NULL)){
        return LoraAs32_RecordStatus(LORA_AS32_INVALID_ARGUMENT);
    }

    if(requested_length > LORA_AS32_MAX_TRANSFER_SIZE){
        return LoraAs32_RecordStatus(LORA_AS32_OUT_OF_RANGE);
    }

    *received_length = 0U;

    if(lora_status == LORA_AS32_RX_OVERFLOW ||
       lora_status == LORA_AS32_UART_ERROR){
        return lora_status;
    }

    lora_status = LORA_AS32_OK;

    while(count < requested_length){
        if(lora_status == LORA_AS32_RX_OVERFLOW ||
           lora_status == LORA_AS32_UART_ERROR){
            *received_length = count;
            return lora_status;
        }

        if(LoraAs32_PopRxByte(&data[count])){
            ++count;
            continue;
        }

        if((timeout_ms == 0U) ||
           Timebase_HasElapsed(start_ms, timeout_ms)){
            *received_length = count;
            return LoraAs32_RecordStatus(LORA_AS32_TIMEOUT);
        }
    }

    *received_length = count;
    return LoraAs32_RecordStatus(LORA_AS32_OK);
}

uint16_t LoraAs32_Available(void){
    const uint16_t head = lora_rx_head;
    const uint16_t tail = lora_rx_tail;

    return (uint16_t)(
        (head - tail) & (LORA_AS32_RX_RING_CAPACITY - 1U)
    );
}

void LoraAs32_FlushRx(void){
    if(!lora_initialized){
        lora_status = LORA_AS32_NOT_READY;
        return;
    }

    USART_ITConfig(AS32_UART_INSTANCE, USART_IT_RXNE, DISABLE);

    while(USART_GetFlagStatus(
              AS32_UART_INSTANCE,
              USART_FLAG_RXNE
          ) != RESET){
        (void)USART_ReceiveData(AS32_UART_INSTANCE);
    }

    lora_rx_tail = lora_rx_head;
    lora_status = LORA_AS32_OK;
    USART_ITConfig(AS32_UART_INSTANCE, USART_IT_RXNE, ENABLE);
}

LoraAs32_Status_t LoraAs32_ReadConfig(
    LoraAs32_Config_t *config,
    uint32_t timeout_ms
){
    static const uint8_t command[3] = {
        LORA_AS32_COMMAND_READ_CONFIG,
        LORA_AS32_COMMAND_READ_CONFIG,
        LORA_AS32_COMMAND_READ_CONFIG
    };
    uint8_t response[LORA_AS32_CONFIG_RESPONSE_SIZE];
    uint16_t received_length;
    LoraAs32_Status_t status;

    if(config == NULL){
        return LoraAs32_RecordStatus(LORA_AS32_INVALID_ARGUMENT);
    }

    if(!lora_initialized || (lora_mode != LORA_AS32_MODE_CONFIG)){
        return LoraAs32_RecordStatus(LORA_AS32_NOT_READY);
    }

    LoraAs32_FlushRx();
    status = LoraAs32_Send(command, sizeof(command), timeout_ms);
    if(status == LORA_AS32_OK){
        status = LoraAs32_Receive(
            response,
            sizeof(response),
            &received_length,
            timeout_ms
        );
    }

    if(status != LORA_AS32_OK){
        return status;
    }

    if(received_length != sizeof(response)){
        return LoraAs32_RecordStatus(LORA_AS32_CONFIG_ERROR);
    }

    return LoraAs32_RecordStatus(
        LoraAs32_DecodeConfigResponse(response, config)
    );
}

LoraAs32_Status_t LoraAs32_WriteConfig(
    const LoraAs32_Config_t *config,
    bool persist,
    uint32_t timeout_ms
){
    uint8_t command[LORA_AS32_CONFIG_RESPONSE_SIZE];
    uint8_t response[2];
    uint16_t received_length;
    LoraAs32_Config_t read_back;
    LoraAs32_Status_t status;

    if(!lora_initialized || (lora_mode != LORA_AS32_MODE_CONFIG)){
        return LoraAs32_RecordStatus(LORA_AS32_NOT_READY);
    }

    status = LoraAs32_BuildWriteCommand(config, persist, command);
    if(status != LORA_AS32_OK){
        return LoraAs32_RecordStatus(status);
    }

    LoraAs32_FlushRx();
    status = LoraAs32_Send(command, sizeof(command), timeout_ms);
    if(status == LORA_AS32_OK){
        status = LoraAs32_Receive(
            response,
            sizeof(response),
            &received_length,
            timeout_ms
        );
    }

    if(status != LORA_AS32_OK){
        return status;
    }

    if((received_length != sizeof(response)) ||
       (response[0] != LORA_AS32_RESPONSE_OK_0) ||
       (response[1] != LORA_AS32_RESPONSE_OK_1)){
        return LoraAs32_RecordStatus(LORA_AS32_CONFIG_ERROR);
    }

    status = LoraAs32_ReadConfig(&read_back, timeout_ms);
    if(status != LORA_AS32_OK){
        return status;
    }

    return LoraAs32_RecordStatus(
        LoraAs32_ConfigEquals(config, &read_back) ?
            LORA_AS32_OK : LORA_AS32_CONFIG_ERROR
    );
}

LoraAs32_Status_t LoraAs32_ReadVersion(
    uint8_t *version,
    uint16_t capacity,
    uint16_t *version_length,
    uint32_t timeout_ms
){
    static const uint8_t command[3] = {
        LORA_AS32_COMMAND_READ_VERSION,
        LORA_AS32_COMMAND_READ_VERSION,
        LORA_AS32_COMMAND_READ_VERSION
    };
    LoraAs32_Status_t status;

    if(!lora_initialized || (lora_mode != LORA_AS32_MODE_CONFIG)){
        return LoraAs32_RecordStatus(LORA_AS32_NOT_READY);
    }

    if(capacity > LORA_AS32_VERSION_MAX_SIZE){
        return LoraAs32_RecordStatus(LORA_AS32_OUT_OF_RANGE);
    }

    LoraAs32_FlushRx();
    status = LoraAs32_Send(command, sizeof(command), timeout_ms);
    if(status != LORA_AS32_OK){
        return status;
    }

    return LoraAs32_ReceiveUntilIdle(
        version,
        capacity,
        version_length,
        timeout_ms
    );
}

LoraAs32_Status_t LoraAs32_Reset(uint32_t timeout_ms){
    static const uint8_t command[3] = {
        LORA_AS32_COMMAND_RESET,
        LORA_AS32_COMMAND_RESET,
        LORA_AS32_COMMAND_RESET
    };
    uint8_t response[2];
    uint16_t received_length;
    LoraAs32_Status_t status;

    if(!lora_initialized || (lora_mode != LORA_AS32_MODE_CONFIG)){
        return LoraAs32_RecordStatus(LORA_AS32_NOT_READY);
    }

    LoraAs32_FlushRx();
    status = LoraAs32_Send(command, sizeof(command), timeout_ms);
    if(status == LORA_AS32_OK){
        status = LoraAs32_Receive(
            response,
            sizeof(response),
            &received_length,
            timeout_ms
        );
    }

    if(status != LORA_AS32_OK){
        return status;
    }

    if((received_length != sizeof(response)) ||
       (response[0] != LORA_AS32_RESPONSE_OK_0) ||
       (response[1] != LORA_AS32_RESPONSE_OK_1)){
        return LoraAs32_RecordStatus(LORA_AS32_CONFIG_ERROR);
    }

    return LoraAs32_WaitAuxReady(timeout_ms);
}

LoraAs32_Status_t LoraAs32_GetStatus(void){
    return lora_status;
}

void LoraAs32_ClearError(void){
    lora_status = lora_initialized ?
                      LORA_AS32_OK : LORA_AS32_NOT_READY;
}

void USART1_IRQHandler(void){
    const uint32_t status_register = AS32_UART_INSTANCE->SR;

    if((status_register & LORA_AS32_USART_ERROR_MASK) != 0U){
        volatile uint32_t discarded = AS32_UART_INSTANCE->DR;

        (void)discarded;
        lora_status = LORA_AS32_UART_ERROR;
        return;
    }

    if((status_register & USART_SR_RXNE) != 0U){
        const uint8_t byte = (uint8_t)AS32_UART_INSTANCE->DR;
        const uint16_t head = lora_rx_head;
        const uint16_t next = (uint16_t)(
            (head + 1U) & (LORA_AS32_RX_RING_CAPACITY - 1U)
        );

        if(next == lora_rx_tail){
            lora_status = LORA_AS32_RX_OVERFLOW;
        }else{
            lora_rx_ring[head] = byte;
            lora_rx_head = next;
        }
    }
}
