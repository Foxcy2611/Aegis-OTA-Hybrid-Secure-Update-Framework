#ifndef AEGIS_METADATA_RECORD_H
#define AEGIS_METADATA_RECORD_H

/*
 * Thao tác dùng chung trên Metadata_Record_t trong RAM.
 *
 * Module này chịu trách nhiệm:
 * - Khởi tạo default record.
 * - Tính và cập nhật CRC32.
 * - Kiểm tra cấu trúc và tính nhất quán của các field.
 *
 * Module này không:
 * - Đọc hoặc ghi Metadata page A/B.
 * - Chọn record theo generation.
 * - Xác thực raw Application.
 * - Tính SHA-512 hoặc verify Ed25519ph.
 */

#include "metadata_struct.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Tính lại CRC32 và cập nhật record->metadata_crc32.
 *
 * Trả về false nếu record là NULL.
 */
bool MetadataRecord_RefreshCrc32(Metadata_Record_t *record);

/*
 * Kiểm tra:
 * - Magic, format và commit marker.
 * - Operation state, trial flag và reserved.
 * - Tính nhất quán giữa state, operation_id và active image.
 * - CRC32 của metadata record.
 *
 * Một record pass hàm này vẫn chưa đồng nghĩa Application là hợp lệ.
 */
bool MetadataRecord_IsStructurallyValid(const Metadata_Record_t *record);

/*
 * Tạo default record hợp lệ về cấu trúc nhưng chưa có active image.
 *
 * Default record không được dùng để boot Application.
 */
bool MetadataRecord_InitializeDefault(Metadata_Record_t *record);

#endif /* AEGIS_METADATA_RECORD_H */
