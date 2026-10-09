#ifndef AEGIS__FLASH__PROGRAM__H
#define AEGIS__FLASH__PROGRAM__H

/*
1.1. Primitive internal flash

File:
  Bootloader/Inc/flash_program.h
  Bootloader/Src/flash_program.c

Làm: API unlock/lock, erase page, program half-word/buffer, range/alignment check
và read-back verify. Hàm trả status, không treo vô hạn; mọi wait có timeout.

Kiểm tra: build `-Wall -Wextra -Werror`; board test vùng flash dành riêng cho test.
Điểm dừng: erase/program/read-back đúng, lỗi range bị từ chối.

*/

#include <stdint.h>

/**
 * @brief Bảng trạng thái return về module Internal Flash
 * @note Mọi hàm trong module đều phải return về kiểu dữ liệu này
 * @note Vì ghi/xóa rất tốn điện, nên dù hiếm vẫn cần timeout
 */

typedef enum {
    FLASH_PROGRAM_OK = 0,             /* Thao tác hoàn tất thành công. */
    FLASH_PROGRAM_INVALID_ARGUMENT,   /* Con trỏ NULL hoặc length bằng 0. */
    FLASH_PROGRAM_OUT_OF_RANGE,       /* Range nằm ngoài Application + Metadata. */
    FLASH_PROGRAM_UNALIGNED,          /* Sai alignment page hoặc half-word. */
    FLASH_PROGRAM_BUSY,               /* Flash đang xử lý thao tác trước. */
    FLASH_PROGRAM_TIMEOUT,            /* Hết thời gian chờ Flash hoàn tất. */
    FLASH_PROGRAM_PROGRAM_ERROR,      /* Phần cứng báo lỗi erase/program. */
    FLASH_PROGRAM_WRITE_PROTECTED,    /* Vùng Flash đang được write-protect. */
    FLASH_PROGRAM_LOCK_FAILED,        /* Không unlock/lock được Flash như yêu cầu. */
    FLASH_PROGRAM_VERIFY_FAILED       /* Dữ liệu read-back không khớp mong đợi. */
} Flash_Status_t;

// API Quản lý khóa
Flash_Status_t FlashProgram_Unlock(void);
Flash_Status_t FlashProgram_Lock(void);

// API Xóa
Flash_Status_t FlashProgram_ErasePage(uint32_t page_addr);

// API Ghi
Flash_Status_t FlashProgram_WriteHalfWord(uint32_t address, uint16_t data);
Flash_Status_t FlashProgram_WriteBuffer(uint32_t address, const uint8_t *data, uint32_t length);

// API Xác thực
Flash_Status_t FlashProgram_Verify(uint32_t address, const uint8_t *expected, uint32_t length);

#endif /* AEGIS_FLASH_PROGRAM_H */
