#ifndef AEGIS_BOOT_METADATA_H
#define AEGIS_BOOT_METADATA_H

/*
 * Metadata A/B store của Bootloader.
 *
 * Module này chịu trách nhiệm:
 * - Đọc cả Metadata page A và B.
 * - Validate cấu trúc từng record bằng MetadataRecord_*.
 * - Chọn record mới nhất theo generation.
 * - Phân biệt FIRST_BOOT với metadata bị CORRUPT.
 * - Commit record mới sang page không active.
 * - Ghi body trước và commit marker cuối cùng.
 *
 * Module này không:
 * - Xác thực firmware bằng SHA-512/Ed25519ph.
 * - Đọc Staging hoặc Backup trên W25Q32.
 * - Quyết định START_APP/INSTALL/ROLLBACK.
 */

#include "metadata_struct.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum{
    BOOT_METADATA_OK = 0U,
    /* Đọc hoặc commit Metadata thành công. */

    BOOT_METADATA_FIRST_BOOT,
    /* Cả hai page Metadata đều còn trắng 0xFF. */

    BOOT_METADATA_NOT_INITIALIZED,
    /* Commit được gọi trước khi BootMetadata_Init thành công. */

    BOOT_METADATA_INVALID_ARGUMENT,
    /* Con trỏ NULL, context sai hoặc record đầu vào không hợp lệ. */

    BOOT_METADATA_CORRUPT,
    /* Cả hai record invalid và ít nhất một page không còn trắng. */

    BOOT_METADATA_GENERATION_AMBIGUOUS,
    /* Hai generation cách nhau đúng 0x80000000. */

    BOOT_METADATA_ERASE_ERROR,
    /* Không erase được page không active. */

    BOOT_METADATA_WRITE_ERROR,
    /* Không ghi được body hoặc commit marker. */

    BOOT_METADATA_VERIFY_ERROR
    /* Dữ liệu đọc lại không khớp hoặc record sau ghi invalid. */
} BootMetadata_Status_t;

/*
 * Trạng thái runtime của Metadata A/B store.
 *
 * Context chỉ tồn tại trong RAM, không phải persistent contract.
 */
typedef struct{
    uint32_t active_page_address;
    /* Địa chỉ page A/B đang chứa record active. */

    uint32_t active_generation;
    /* Generation của record active. */

    bool initialized;
    /* true sau khi Init thành công hoặc phát hiện FIRST_BOOT. */

    bool has_active_record;
    /* false ở FIRST_BOOT khi chưa có record nào trong Flash. */
} BootMetadata_Context_t;

/*
 * Đọc, validate và chọn Metadata record mới nhất.
 *
 * BOOT_METADATA_OK:
 * - out_record chứa record active.
 * - context sẵn sàng cho Commit.
 *
 * BOOT_METADATA_FIRST_BOOT:
 * - out_record chứa default record trong RAM.
 * - context sẵn sàng commit lần đầu vào page A.
 *
 * Các status lỗi:
 * - context không được dùng để Commit.
 */
BootMetadata_Status_t BootMetadata_Init(
    BootMetadata_Context_t *context,
    Metadata_Record_t *out_record
);

/*
 * Commit record sang page không active.
 *
 * Module tự cập nhật:
 * - magic;
 * - format_version;
 * - generation;
 * - reserved;
 * - metadata_crc32;
 * - commit_marker.
 *
 * in_out_record chỉ được cập nhật sau khi commit thành công.
 */
BootMetadata_Status_t BootMetadata_Commit(
    BootMetadata_Context_t *context,
    Metadata_Record_t *in_out_record
);

#endif /* AEGIS_BOOT_METADATA_H */