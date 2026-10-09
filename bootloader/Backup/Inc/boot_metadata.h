#ifndef AEGIS_BOOT_METADATA_H
#define AEGIS_BOOT_METADATA_H
// 1.3

#include "metadata_struct.h"

typedef enum {
    BOOT_METADATA_OK = 0,                /* Đã đọc hoặc commit metadata thành công. */
    BOOT_METADATA_FIRST_BOOT,            /* Cả hai page metadata còn trắng 0xFF. */
    BOOT_METADATA_NOT_INITIALIZED,       /* Gọi Write trước khi Init thành công. */
    BOOT_METADATA_INVALID_ARGUMENT,      /* Con trỏ NULL hoặc field đầu vào sai. */
    BOOT_METADATA_CORRUPT,               /* Cả hai page invalid và có dữ liệu hỏng. */
    BOOT_METADATA_GENERATION_AMBIGUOUS,  /* Hai generation cách nhau đúng nửa vòng. */
    BOOT_METADATA_WRITE_ERROR,           /* Tầng Internal Flash erase/program lỗi. */
    BOOT_METADATA_VERIFY_ERROR           /* Dữ liệu đọc lại không khớp bản cần ghi. */
} BootMetadata_Status_t;

/**
 * @brief Khởi tạo module Metadata, quét Flash để tìm bản ghi mới nhất.
 * @param out_metadata Con trỏ trỏ tới biến RAM chứa cấu hình sẽ được dùng.
 * @return BOOT_METADATA_OK nếu đọc thành công, FIRST_BOOT nếu cả hai page trắng,
 *         CORRUPT nếu cả hai page invalid nhưng không cùng ở trạng thái erased.
 */
BootMetadata_Status_t BootMetadata_Init(Metadata_Struct *out_metadata);

/**
 * @brief Commit bản ghi xuống slot không active; body được verify trước marker.
 * @param metadata Dữ liệu cần lưu. Chỉ được cập nhật generation/CRC sau commit.
 * @return BOOT_METADATA_OK nếu commit thành công, status lỗi nếu transaction hỏng.
 */
BootMetadata_Status_t BootMetadata_Write(Metadata_Struct *metadata);

#endif /* AEGIS_BOOT_METADATA_H */
