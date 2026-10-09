# Phase 5–8 Host Regression

> **Phạm vi:** Giao thức OTA, secure frame, sender ESP32, boot decision, Metadata
> invariant, Delta streaming và các công cụ artifact chạy không cần board.

> **Tác dụng:** Phát hiện contract lệch giữa source C và tool Python trước khi
> chạy FULL/Delta OTA trên W25Q32 và hai AS32 thật.

```bash
cmake -S test/Test_to_Phase/Phase5_8_Host \
      -B build/tests/phase5-8 -G Ninja
cmake --build build/tests/phase5-8
ctest --test-dir build/tests/phase5-8 --output-on-failure
```

Các test hiện tại kiểm tra logical codec, STATUS, AEAD/replay, Delta COPY/ADD,
boot decision, Metadata ổn định, signer/package/delta tool và cross-language
verification. PASS ở đây không thay thế W25Q32, Internal Flash, reset/power-loss,
ESP-IDF hoặc RF end-to-end trên phần cứng.
