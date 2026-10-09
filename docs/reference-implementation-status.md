# Reference Implementation Status

> **Phạm vi:** Source chính dùng làm layout/code mẫu khi khởi động lại dự án.

> **Tác dụng:** Phân biệt rõ phần đã có source/host test với phần vẫn cần phần cứng thật.

## Đã có trong source chính

- Bootloader: Metadata A/B, decision, Ed25519ph verify, install, Trial và rollback.
- Application: secure frame/session, OTA command, FULL/Delta receive, W25 storage,
  persistent session anti-replay journal, Backup, mailbox, Trial IWDG và health
  confirmation.
- Delta: signed manifest, patch store, SHA-512 gate và streaming `COPY/ADD/END`.
- ESP32: AS32 adapter/radio task, sender stop-and-wait/retry/resume/STATUS và
  `ota_gateway` nối secure frame trực tiếp với AS32.
- PC tools: ký FULL, tạo Delta, đóng transfer manifest/data, tạo factory Metadata,
  benchmark, release-evidence JSON và recovery ST-Link.
- CMake/Ninja: Debug và Release tạo Bootloader/Application đúng partition.
- Script: build Debug/Release và chạy toàn bộ software gate bằng một lệnh.

## Chưa được gọi là hardware PASS

- Phase 1: erase/program/reset thật trên STM32.
- Phase 2: W25Q32 JEDEC/read/write/commit-last/power-loss.
- Phase 4–5: ESP-IDF build và hai AS32 P2P thật; MTU/timeout/RF measurement.
- Phase 6–8: FULL/Delta E2E, Trial/Rollback, fault matrix và performance evidence.

Vì vậy source này là reference implementation hoàn chỉnh về layout và luồng,
không phải bằng chứng rằng board/RF đã nghiệm thu.

## Lệnh kiểm tra không cần board

```bash
cmake --preset debug
cmake --build --preset debug
cmake --preset release
cmake --build --preset release

cmake -S test/Test_to_Phase/Phase5_8_Host \
      -B build/host-phase5-8 -G Ninja
cmake --build build/host-phase5-8
ctest --test-dir build/host-phase5-8 --output-on-failure

# Hoặc chạy toàn bộ software gate canonical:
./scripts/check_all.sh
```

Key trong `common/src/device_keys.c` cố ý là weak zero-value và fail-closed.
Khi dựng project thật phải override bằng provisioning source/linker section;
không điền private signing key vào firmware hoặc repository.

`esp32_gateway/main/app_main.c` hiện là harness Phase 4 để đọc profile/đo link.
Production ingress Wi-Fi/PC phải cache artifact + ID rồi gọi
`OtaGateway_SendArtifact()`; ingress/cloud API cụ thể không thuộc trust/protocol
core và không được giả bằng private key hay artifact hard-code trong source.
