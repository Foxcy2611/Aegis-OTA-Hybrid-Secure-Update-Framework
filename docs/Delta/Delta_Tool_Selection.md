# Delta Tool Selection

> **Phạm vi:** Thuật toán patch được dùng trong source mẫu STM32F103C8T6.

> **Tác dụng:** Ghi rõ vì sao project dùng format `COPY/ADD/END` và cách benchmark trước khi đổi thuật toán.

## Quyết định

Source mẫu dùng `DELTA_ALGORITHM_COPY_ADD_V1`:

| Opcode | Payload | Tác dụng |
|---|---|---|
| `COPY (0x01)` | `base_offset u32 + length u16` | Copy một đoạn từ Internal Application cũ sang target Staging. |
| `ADD (0x02)` | `length u16 + bytes` | Chèn byte mới từ patch vào target Staging. |
| `END (0xFF)` | Không có | Kết thúc; patch và target phải hết đúng biên đã khai báo. |

STM32 đọc theo buffer 128 byte, không malloc và không ghi trực tiếp Internal
Application. `tools/make_delta.py` tạo patch, tự apply trên host và chỉ xuất
package khi output giống target byte-for-byte.

Format này ưu tiên dễ đọc, dễ test lỗi và code-size nhỏ. Thuật toán host hiện chỉ
giữ prefix/suffix giống nhau rồi ADD phần giữa; nó là baseline đúng, chưa phải
thuật toán tối ưu patch size.

## Khi nào được đổi

Chạy `tools/delta_benchmark.py old.bin:new.bin` trên dataset thật. Chỉ chuyển sang
Detools/thuật toán khác nếu có evidence rằng patch/airtime tốt hơn và Application
vẫn nằm trong 46 KB Flash, 20 KB RAM. Mọi format mới phải có algorithm ID mới,
license/version pin và host/STM32 golden test riêng.
