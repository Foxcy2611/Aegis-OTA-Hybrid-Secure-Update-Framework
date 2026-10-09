# Bootloader Legacy Source Backup

> **Phạm vi:** Snapshot các file `.h` và `.c` của Bootloader trước khi repository
> được dọn về baseline Phase 0.

> **Tác dụng:** Dùng để đọc, so sánh và tham khảo khi viết lại từng checkpoint;
> không phải production source hoặc contract canonical.

## Nội dung

```text
Backup/
├── Inc/
│   ├── artifact_validator.h
│   ├── boot_mailbox_reader.h
│   ├── boot_metadata.h
│   ├── boot_state_machine.h
│   ├── crypto_service.h
│   ├── flash_program.h
│   └── ota_storage.h
└── Src/
    ├── artifact_validator.c
    ├── boot_mailbox_reader.c
    ├── boot_metadata.c
    ├── boot_state_machine.c
    ├── crypto_service.c
    ├── flash_program.c
    ├── main.c
    └── ota_storage.c
```

- `crypto_service` và `ota_storage` là thiết kế cũ khi Bootloader còn gánh cả
  secure transport và W25Q32 write.
- `artifact_validator` và `boot_mailbox_reader` là bản nháp sau khi bắt đầu tách
  ownership.
- Các file Flash/metadata/state machine là snapshot cuối ngay trước cleanup.

## Quy tắc sử dụng

- Không thêm `Bootloader/Backup` vào include path, `VPATH` hoặc source list.
- Không sửa file ở đây để triển khai tính năng mới; hãy viết trong `Inc/` và
  `Src/` theo roadmap.
- Code backup có thể không compile với Shared contract hiện hành.
- Khi đối chiếu có mâu thuẫn, roadmap canonical và `Shared/` luôn được ưu tiên.

Các file được phục hồi nguyên nội dung từ lịch sử thao tác cục bộ của workspace.
