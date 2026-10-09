#ifndef AEGIS_CRC32_H
#define AEGIS_CRC32_H

/*
 * CRC32 dùng chung cho Metadata, Manifest, Mailbox và OTA frame.
 *
 * Thuật toán:
 * - CRC-32/ISO-HDLC.
 * - Polynomial phản chiếu: 0xEDB88320.
 * - Giá trị truyền vào lần đầu: 0.
 */

#include <stddef.h>
#include <stdint.h>

#define CRC32_REFLECTED_POLYNOMIAL    UINT32_C(0xEDB88320)
#define CRC32_INITIAL_VALUE           UINT32_C(0)

/*
 * Tiếp tục tính CRC từ kết quả của lần gọi trước.
 *
 * Ví dụ:
 * uint32_t crc = CRC32_INITIAL_VALUE;
 * crc = Crc32_Update(crc, block_1, block_1_size);
 * crc = Crc32_Update(crc, block_2, block_2_size);
 */
uint32_t Crc32_Update(
    uint32_t previous_crc,
    const void *data,
    size_t length
);

/* Tính CRC32 một lần cho toàn bộ buffer. */
uint32_t Crc32_Compute(
    const void *data,
    size_t length
);

#endif /* AEGIS_CRC32_H */