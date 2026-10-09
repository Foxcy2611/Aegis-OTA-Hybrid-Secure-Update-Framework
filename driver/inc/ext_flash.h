#ifndef AEGIS_EXT_FLASH_H
#define AEGIS_EXT_FLASH_H

/*
 * Primitive driver cho W25Q32 qua SPI1.
 *
 * Module chịu trách nhiệm:
 * - Khởi tạo SPI1 và GPIO.
 * - Đọc JEDEC ID và Status Register 1.
 * - Read Data theo địa chỉ tuyệt đối.
 * - Page Program tối đa một page.
 * - Sector Erase đúng một sector.
 * - Timeout hữu hạn cho SPI và BUSY.
 *
 * Module không biết Staging, Backup, Delta hoặc manifest.
 */

#include <stdint.h>

typedef enum{
    EXT_FLASH_OK = 0U,
    /* Thao tác hoàn thành thành công. */

    EXT_FLASH_INVALID_ARGUMENT,
    /* Con trỏ NULL hoặc length bằng 0. */

    EXT_FLASH_OUT_OF_RANGE,
    /* Vùng truy cập vượt ngoài W25Q32. */

    EXT_FLASH_UNALIGNED,
    /* Địa chỉ erase không thẳng hàng sector. */

    EXT_FLASH_PAGE_BOUNDARY,
    /* Page Program vượt quá 256 byte hoặc đi qua biên page. */

    EXT_FLASH_SPI_TIMEOUT,
    /* TXE, RXNE hoặc BSY của SPI không đổi trạng thái đúng hạn. */

    EXT_FLASH_BUSY_TIMEOUT,
    /* W25Q32 giữ bit BUSY quá giới hạn cho phép. */

    EXT_FLASH_WRITE_ENABLE_FAILED,
    /* Lệnh Write Enable không làm bit WEL lên 1. */

    EXT_FLASH_JEDEC_ID_MISMATCH,
    /* JEDEC ID đọc được không phải W25Q32 đã chốt. */

    EXT_FLASH_VERIFY_FAILED
    /* FAIL */
} ExtFlash_Status_t;

/*
 * Khởi tạo GPIO/SPI1 và kiểm tra JEDEC ID.
 */
ExtFlash_Status_t W25Q32_Init(void);

/*
 * Đọc JEDEC ID 24-bit:
 *
 * [23:16] Manufacturer
 * [15:8]  Memory type
 * [7:0]   Capacity
 */
ExtFlash_Status_t W25Q32_ReadJedecId(uint32_t *jedec_id);

/*
 * Đọc Status Register 1.
 */
ExtFlash_Status_t W25Q32_ReadStatus1(uint8_t *status_register);

/*
 * Chờ bit BUSY về 0 với timeout hữu hạn.
 */
ExtFlash_Status_t W25Q32_WaitWhileBusy(void);

/*
 * Đọc một vùng liên tiếp theo địa chỉ tuyệt đối của W25Q32.
 */
ExtFlash_Status_t W25Q32_Read(
    uint32_t address,
    uint8_t *data,
    uint32_t length
);

/*
 * Ghi tối đa một page.
 *
 * API không tự erase và không cho phép đi qua biên page.
 */
ExtFlash_Status_t W25Q32_PageProgram(
    uint32_t address,
    const uint8_t *data,
    uint32_t length
);

/*
 * Xóa đúng một sector 4 KB.
 *
 * address phải sector-aligned.
 */
ExtFlash_Status_t W25Q32_SectorErase(uint32_t address);


/*
 * Đọc 1 block theo địa chỉ tuyệt đối
 */
ExtFlash_Status_t W25Q32_ReadBlock(
    uint32_t address,
    uint8_t *data,
    uint32_t length
);

/*
 * Ghi một block có thể đi qua nhiều page.
 *
 * Hàm tự chia dữ liệu thành các chunk không vượt biên page.
 * Hàm không tự erase sector.
 */
ExtFlash_Status_t W25Q32_WriteBlock(
    uint32_t address,
    const uint8_t *data,
    uint32_t length
);

/*
 * Đọc lại theo từng buffer nhỏ và so sánh với dữ liệu mong đợi.
 */
ExtFlash_Status_t W25Q32_VerifyBlock(
    uint32_t address,
    const uint8_t *expected,
    uint32_t length
);

#endif