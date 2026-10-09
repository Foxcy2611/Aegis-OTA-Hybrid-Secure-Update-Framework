#include "ext_flash.h"

#include "memory_map.h"
#include "pinout_hw.h"
#include "watchdog.h"

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_spi.h"

#include <stdbool.h>
#include <stddef.h>

/* ==================== W25Q32 command ==================== */
#define W25Q32_COMMAND_WRITE_ENABLE       UINT8_C(0x06)
#define W25Q32_COMMAND_READ_STATUS_1      UINT8_C(0x05)
#define W25Q32_COMMAND_READ_DATA          UINT8_C(0x03)
#define W25Q32_COMMAND_PAGE_PROGRAM       UINT8_C(0x02)
#define W25Q32_COMMAND_SECTOR_ERASE       UINT8_C(0x20)
#define W25Q32_COMMAND_JEDEC_ID           UINT8_C(0x9F)

#define W25Q32_DUMMY_BYTE                 UINT8_C(0xFF)

/* ==================== Status Register 1 ==================== */

#define W25Q32_STATUS_BUSY_MASK           UINT8_C(0x01)
#define W25Q32_STATUS_WEL_MASK            UINT8_C(0x02)
#define W25Q32_VERIFY_BUFFER_SIZE         UINT32_C(64)

/*
 * JEDEC ID chuẩn:
 * - EF: Winbond
 * - 40: SPI NOR memory type
 * - 16: capacity 32 Mbit = 4 MB
 */
#define W25Q32_EXPECTED_JEDEC_ID           UINT32_C(0x00EF4016)

/*
 * Timeout theo số vòng polling.
 *
 * Đây chưa phải millisecond chính xác; mục đích hiện tại là bảo đảm
 * không có vòng chờ vô hạn. Có thể chuyển sang timer tick sau này.
 */
#define W25Q32_SPI_WAIT_LIMIT              UINT32_C(200000)
#define W25Q32_BUSY_POLL_LIMIT             UINT32_C(4000000)

/* ==================== Helper private ==================== */
static void W25Q32_Select(void){
    GPIO_ResetBits(W25Q32_PORT, W25Q32_CS_PIN);
}

static void W25Q32_Deselect(void){
    GPIO_SetBits(W25Q32_PORT, W25Q32_CS_PIN);
}

/*
 * Range check chống integer overflow.
 */
static bool W25Q32_IsRangeValid(uint32_t address, uint32_t length){
    if(length == 0U){
        return false;
    }

    if(address >= W25Q32_TOTAL_SIZE){
        return false;
    }

    return length <= (W25Q32_TOTAL_SIZE - address);
}

/*
 * Gửi và đồng thời nhận một byte SPI.
 */
static ExtFlash_Status_t W25Q32_TransferByte(
    uint8_t output,
    uint8_t *input
){
    uint32_t timeout = W25Q32_SPI_WAIT_LIMIT;

    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET){
        if(timeout == 0U){
            return EXT_FLASH_SPI_TIMEOUT;
        }

        --timeout;
    }

    SPI_I2S_SendData(SPI1, output);

    timeout = W25Q32_SPI_WAIT_LIMIT;

    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET){
        if(timeout == 0U){
            return EXT_FLASH_SPI_TIMEOUT;
        }

        --timeout;
    }

    const uint8_t received = (uint8_t)SPI_I2S_ReceiveData(SPI1);

    if(input != NULL){
        *input = received;
    }

    return EXT_FLASH_OK;
}

/*
 * Chờ SPI truyền xong byte cuối rồi mới kéo CS high.
 *
 * Hàm luôn kéo CS high, kể cả khi SPI timeout.
 */
static ExtFlash_Status_t W25Q32_EndTransaction(
    ExtFlash_Status_t current_status
){
    uint32_t timeout = W25Q32_SPI_WAIT_LIMIT;

    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET){
        if(timeout == 0U){
            W25Q32_Deselect();

            if(current_status == EXT_FLASH_OK){
                return EXT_FLASH_SPI_TIMEOUT;
            }

            return current_status;
        }

        --timeout;
    }

    W25Q32_Deselect();
    return current_status;
}

/*
 * Gửi địa chỉ W25Q32 theo thứ tự MSB trước:
 * A23..A16, A15..A8, A7..A0.
 */
static ExtFlash_Status_t W25Q32_SendAddress(uint32_t address){
    ExtFlash_Status_t status = W25Q32_TransferByte(
        (uint8_t)(address >> 16U),
        NULL
    );

    if(status == EXT_FLASH_OK){
        status = W25Q32_TransferByte(
            (uint8_t)(address >> 8U),
            NULL
        );
    }

    if(status == EXT_FLASH_OK){
        status = W25Q32_TransferByte(
            (uint8_t)address,
            NULL
        );
    }

    return status;
}

/*
 * Gửi Write Enable và kiểm tra WEL đã được W25Q32 chấp nhận.
 */
static ExtFlash_Status_t W25Q32_WriteEnable(void){
    ExtFlash_Status_t status = W25Q32_WaitWhileBusy();

    if(status != EXT_FLASH_OK){
        return status;
    }

    W25Q32_Select();

    status = W25Q32_TransferByte(
        W25Q32_COMMAND_WRITE_ENABLE,
        NULL
    );

    status = W25Q32_EndTransaction(status);

    if(status != EXT_FLASH_OK){
        return status;
    }

    uint8_t status_register = 0U;

    status = W25Q32_ReadStatus1(&status_register);

    if(status != EXT_FLASH_OK){
        return status;
    }

    if((status_register & W25Q32_STATUS_WEL_MASK) == 0U){
        return EXT_FLASH_WRITE_ENABLE_FAILED;
    }

    return EXT_FLASH_OK;
}

ExtFlash_Status_t W25Q32_Init(void){
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    /* Giữ CS high khi bus đang idle. */
    W25Q32_Deselect();   

    SPI_InitTypeDef spi;
    GPIO_InitTypeDef gpio;

    /*
     * SCK và MOSI: Alternate Function Push-Pull.
     */
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Pin = W25Q32_CLK_PIN | W25Q32_MOSI_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(W25Q32_PORT, &gpio);

    /*
     * MISO: Input Floating.
     */
    gpio.GPIO_Pin = W25Q32_MISO_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(W25Q32_PORT, &gpio);

    /*
     * CS: GPIO output push-pull, software điều khiển.
     */
    gpio.GPIO_Pin = W25Q32_CS_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(W25Q32_PORT, &gpio);
    W25Q32_Deselect();

    /*
     * Reset SPI1 về trạng thái xác định trước khi cấu hình.
     */
    SPI_I2S_DeInit(SPI1);
    SPI_StructInit(&spi);

    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;

    /*
     * W25Q32 Standard SPI dùng Mode 0:
     * CPOL = 0, CPHA = cạnh thứ nhất.
     */
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = SPI_CPHA_1Edge;

    spi.SPI_NSS = SPI_NSS_Soft;

    /*
     * PCLK2 = 72 MHz thì prescaler 8 tạo SCK khoảng 9 MHz.
     */
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;

    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7U;
    SPI_Init(SPI1, &spi);
    SPI_Cmd(SPI1, ENABLE);

    uint32_t jedec_id = 0U;

    const ExtFlash_Status_t status = W25Q32_ReadJedecId(&jedec_id);

    if(status != EXT_FLASH_OK){
        return status;
    }

    if(jedec_id != W25Q32_EXPECTED_JEDEC_ID){
        return EXT_FLASH_JEDEC_ID_MISMATCH;
    }

    return EXT_FLASH_OK;
}

ExtFlash_Status_t W25Q32_ReadJedecId(uint32_t *jedec_id){
    if(jedec_id == NULL){
        return EXT_FLASH_INVALID_ARGUMENT;
    }

    uint8_t manufacturer = 0U;
    uint8_t memory_type = 0U;
    uint8_t capacity = 0U;

    W25Q32_Select();

    // Truyền NULL để tránh đọc lại byte rác mà SPI return về
    ExtFlash_Status_t status = W25Q32_TransferByte(
        W25Q32_COMMAND_JEDEC_ID,
        NULL
    );

    if(status == EXT_FLASH_OK){
        status = W25Q32_TransferByte(
            W25Q32_DUMMY_BYTE,
            &manufacturer
        );
    }

    if(status == EXT_FLASH_OK){
        status = W25Q32_TransferByte(
            W25Q32_DUMMY_BYTE,
            &memory_type
        );
    }

    if(status == EXT_FLASH_OK){
        status = W25Q32_TransferByte(
            W25Q32_DUMMY_BYTE,
            &capacity
        );
    }

    status = W25Q32_EndTransaction(status);

    if(status != EXT_FLASH_OK){
        return status;
    }

    *jedec_id =
        ((uint32_t)manufacturer << 16U) |
        ((uint32_t)memory_type << 8U) |
        (uint32_t)capacity;

    return EXT_FLASH_OK;
}

ExtFlash_Status_t W25Q32_ReadStatus1(uint8_t *status_register){
    if(status_register == NULL){
        return EXT_FLASH_INVALID_ARGUMENT;
    }

    W25Q32_Select();

    ExtFlash_Status_t status = W25Q32_TransferByte(
        W25Q32_COMMAND_READ_STATUS_1,
        NULL
    );

    if(status == EXT_FLASH_OK){
        status = W25Q32_TransferByte(
            W25Q32_DUMMY_BYTE,
            status_register
        );
    }

    return W25Q32_EndTransaction(status);
}

ExtFlash_Status_t W25Q32_WaitWhileBusy(void){
    for(uint32_t attempt = 0U;
        attempt < W25Q32_BUSY_POLL_LIMIT;
        ++attempt){

        Watchdog_Feed();

        uint8_t status_register = 0U;

        const ExtFlash_Status_t status = W25Q32_ReadStatus1(&status_register);

        if(status != EXT_FLASH_OK){
            return status;
        }

        if((status_register & W25Q32_STATUS_BUSY_MASK) == 0U){
            return EXT_FLASH_OK;
        }
    }

    return EXT_FLASH_BUSY_TIMEOUT;
}

ExtFlash_Status_t W25Q32_Read(
    uint32_t address,
    uint8_t *data,
    uint32_t length
){
    if((data == NULL) || (length == 0U)){
        return EXT_FLASH_INVALID_ARGUMENT;
    }

    if(!W25Q32_IsRangeValid(address, length)){
        return EXT_FLASH_OUT_OF_RANGE;
    }

    W25Q32_Select();

    ExtFlash_Status_t status = W25Q32_TransferByte(
        W25Q32_COMMAND_READ_DATA,
        NULL
    );

    if(status == EXT_FLASH_OK){
        status = W25Q32_SendAddress(address);
    }

    for(uint32_t i = 0U;
        (i < length) && (status == EXT_FLASH_OK);
        ++i){

        status = W25Q32_TransferByte(
            W25Q32_DUMMY_BYTE,
            &data[i]
        );
    }

    return W25Q32_EndTransaction(status);
}

ExtFlash_Status_t W25Q32_PageProgram(
    uint32_t address,
    const uint8_t *data,
    uint32_t length
){
    if((data == NULL) || (length == 0U)){
        return EXT_FLASH_INVALID_ARGUMENT;
    }

    if(!W25Q32_IsRangeValid(address, length)){
        return EXT_FLASH_OUT_OF_RANGE;
    }

    /*
     * Không được vượt kích thước page hoặc đi qua biên page hiện tại.
     */
    const uint32_t page_offset = address % W25Q32_PAGE_SIZE;

    const uint32_t page_remaining = W25Q32_PAGE_SIZE - page_offset;

    if((length > W25Q32_PAGE_SIZE) ||
       (length > page_remaining)){
        return EXT_FLASH_PAGE_BOUNDARY;
    }

    ExtFlash_Status_t status = W25Q32_WriteEnable();

    if(status != EXT_FLASH_OK){
        return status;
    }

    W25Q32_Select();

    status = W25Q32_TransferByte(
        W25Q32_COMMAND_PAGE_PROGRAM,
        NULL
    );

    if(status == EXT_FLASH_OK){
        status = W25Q32_SendAddress(address);
    }

    for(uint32_t i = 0U;
        (i < length) && (status == EXT_FLASH_OK);
        ++i){

        status = W25Q32_TransferByte(data[i], NULL);
    }

    status = W25Q32_EndTransaction(status);

    if(status != EXT_FLASH_OK){
        return status;
    }

    return W25Q32_WaitWhileBusy();
}

ExtFlash_Status_t W25Q32_SectorErase(uint32_t address){
    if(!W25Q32_IsRangeValid(address, W25Q32_SECTOR_SIZE)){
        return EXT_FLASH_OUT_OF_RANGE;
    }

    if((address % W25Q32_SECTOR_SIZE) != 0U){
        return EXT_FLASH_UNALIGNED;
    }

    ExtFlash_Status_t status = W25Q32_WriteEnable();

    if(status != EXT_FLASH_OK){
        return status;
    }

    W25Q32_Select();

    status = W25Q32_TransferByte(
        W25Q32_COMMAND_SECTOR_ERASE,
        NULL
    );

    if(status == EXT_FLASH_OK){
        status = W25Q32_SendAddress(address);
    }

    status = W25Q32_EndTransaction(status);

    if(status != EXT_FLASH_OK){
        return status;
    }

    return W25Q32_WaitWhileBusy();
}

/* ======================== API BLOCK ======================== */
ExtFlash_Status_t W25Q32_ReadBlock(
    uint32_t address,
    uint8_t *data,
    uint32_t length
){
    if(length == 0U){
        return EXT_FLASH_OK;
    }

    if(data == NULL){
        return EXT_FLASH_INVALID_ARGUMENT;
    }

    if(!W25Q32_IsRangeValid(address, length)){
        return EXT_FLASH_OUT_OF_RANGE;
    }

    return W25Q32_Read(address, data, length);
}

ExtFlash_Status_t W25Q32_WriteBlock(
    uint32_t address,
    const uint8_t *data,
    uint32_t length
){
    if(length == 0U){
        return EXT_FLASH_OK;
    }

    if(data == NULL){
        return EXT_FLASH_INVALID_ARGUMENT;
    }

    /*
     * Kiểm tra toàn bộ range trước khi program page đầu tiên.
     * Nhờ vậy không xảy ra tình trạng ghi được một phần rồi mới phát hiện
     * cuối block vượt khỏi W25Q32.
     */
    if(!W25Q32_IsRangeValid(address, length)){
        return EXT_FLASH_OUT_OF_RANGE;
    }

    uint32_t offset = 0U;

    while(offset < length){
        // Địa chỉ hiện tại
        const uint32_t current_address = address + offset;

        // Địa chỉ page hiện tại thứ bao nhiêu
        const uint32_t page_offset = current_address % W25Q32_PAGE_SIZE;

        // Số byte còn lại
        const uint32_t page_remaining = W25Q32_PAGE_SIZE - page_offset;

        // Số byte còn lại cần phải ghi
        const uint32_t block_remaining = length - offset;

        // Quyết định ghi dựa vào byte
        const uint32_t chunk_size = 
        (block_remaining < page_remaining) ? block_remaining : page_remaining;

        const ExtFlash_Status_t status = W25Q32_PageProgram(
            current_address,
            &data[offset],
            chunk_size
        );

        if(status != EXT_FLASH_OK){
            return status;
        }

        offset += chunk_size;
    }

    return EXT_FLASH_OK;
}

ExtFlash_Status_t W25Q32_VerifyBlock(
    uint32_t address,
    const uint8_t *expected,
    uint32_t length
){
    if(length == 0U){
        return EXT_FLASH_OK;
    }

    if(expected == NULL){
        return EXT_FLASH_INVALID_ARGUMENT;
    }

    if(!W25Q32_IsRangeValid(address, length)){
        return EXT_FLASH_OUT_OF_RANGE;
    }

    /*
     * Không cấp buffer bằng kích thước firmware.
     * Chỉ dùng 64 byte RAM và verify tuần tự.
     */
    uint8_t read_buffer[W25Q32_VERIFY_BUFFER_SIZE];
    uint32_t offset = 0U;

    while(offset < length){
        const uint32_t block_remaining = length - offset;

        const uint32_t chunk_size =
        (block_remaining < W25Q32_VERIFY_BUFFER_SIZE) ? block_remaining : W25Q32_VERIFY_BUFFER_SIZE;

        const ExtFlash_Status_t status = W25Q32_Read(
            address + offset,
            read_buffer,
            chunk_size
        );

        if(status != EXT_FLASH_OK){
            return status;
        }

        for(uint32_t i = 0U; i < chunk_size; ++i){
            if(read_buffer[i] != expected[offset + i]){
                return EXT_FLASH_VERIFY_FAILED;
            }
        }

        offset += chunk_size;
    }

    return EXT_FLASH_OK;
}
