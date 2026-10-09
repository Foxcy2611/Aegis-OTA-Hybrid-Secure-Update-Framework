# Terminology

> **Phạm vi (Scope):** Giải thích ngắn từ khóa để người đọc mới hiểu tài liệu.

> **Tác dụng:** Cung cấp nghĩa thống nhất cho các thuật ngữ được dùng xuyên suốt dự án.


| Thuật ngữ | Nghĩa |
|---|---|
| Artifact | Gói update được phát hành/truyền |
| FULL | Toàn bộ target firmware |
| DELTA/Patch | Dữ liệu khác biệt giữa base và target |
| Base | Firmware cũ |
| Target | Firmware mới cuối cùng |
| STAGING | Target hoàn chỉnh trước install |
| BACKUP | App cũ để rollback |
| DELTA_DATA | Patch bytes |
| Manifest | Metadata mô tả và commit artifact |
| SignedHeader | Header 32 bytes được ký cùng firmware |
| Trial Boot | Boot thử firmware mới |
| Confirm | Xác nhận firmware mới healthy |
| Rollback | Restore App cũ |
| Recovery | Last-resort khi không còn đường boot/rollback an toàn |
| Gateway Controller | ESP32 điều phối OTA P2P |
