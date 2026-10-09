# Build and Toolchain

> **Phạm vi (Scope):** Build gates, linker contract, warning/size policy.

> **Tác dụng:** Hướng dẫn build firmware và kiểm tra các giới hạn Flash, SRAM, stack.


Build outputs:

- Bootloader ELF/BIN/MAP;
- Application ELF/BIN/MAP;
- size report;
- host test log;
- signed artifact;
- toolchain version.

ESP32 Gateway dùng ESP-IDF project riêng trong `esp32_gateway/`. Output/build
tree ESP32 không được trộn với `build/debug` hoặc `build/release` của ARM.

Gates:

```text
-Wall -Wextra -Werror
 -> linker memory contract
 -> Bootloader <= 16 KB
 -> Application <= 46 KB
 -> host regression
 -> board regression
```

SRAM total: **20 KB**.

## Representative-link rule

Bootloader size chỉ có ý nghĩa khi executable gọi toàn bộ đường production mà
Bootloader sở hữu. Skeleton rỗng hoặc symbol bị `--gc-sections` loại không được
dùng làm bằng chứng fit.

Representative Bootloader phải giữ/call:

- Metadata A/B và boot mailbox;
- W25Q32 manifest/image read;
- SHA-512 + Ed25519ph target/Backup verify;
- state decision và executor;
- Internal Flash install/rollback/read-back;
- Trial Boot, vector validation và Application jump.

Không link normal OTA receiver, ChaCha transport, retry/resume hoặc Delta applier
vào Bootloader. Debug dùng `-Os -g3`, Release dùng `-Os`; cả hai bật section GC
và LTO nhất quán để representative Bootloader vẫn nằm trong 16 KB. Debug không
dùng `-Og` vì chính phần code xác thực thật sẽ làm image vượt partition.
Map file phải chứng minh symbol nào thực sự được giữ.

## Feasibility acceptance

```text
clean build
 -> warning-free
 -> representative call graph retained
 -> FLASH <= 16 KB
 -> static RAM + measured stack <= 20 KB
 -> ELF/BIN/MAP/disassembly archived
```

Phase 0 dùng `.su` để chặn static frame vượt stack reserve 4 KB và linker chặn
`.bss` chạm vùng stack. Board high-water cho call chain crypto + storage/install
được lưu ở phase triển khai đường chạy đó; không được chỉ nhìn `.bss` để nghiệm
thu production runtime.

## Build Phase 4

```bash
# Host contract/cross-codec
cmake -S test/Test_to_Phase/Phase4_LoRa/host \
      -B build/phase4-host -G Ninja
cmake --build build/phase4-host
ctest --test-dir build/phase4-host --output-on-failure

# STM32 standalone echo image
cmake --preset debug
cmake --build build/debug --target aegis_phase4_lora_board_test

# ESP32, chỉ sau khi đã export môi trường ESP-IDF
cd esp32_gateway
idf.py set-target esp32
idf.py build
```

Board/RF evidence vẫn bắt buộc dù cả ba build đều thành công.

## Build source mẫu Phase 5–8

```bash
cmake --preset debug
cmake --build --preset debug

cmake --preset release
cmake --build --preset release

cmake -S test/Test_to_Phase/Phase5_8_Host \
      -B build/host-phase5-8 -G Ninja
cmake --build build/host-phase5-8
ctest --test-dir build/host-phase5-8 --output-on-failure
```

Host gate compile logical protocol, secure frame, boot decision, Delta applier,
ESP32 sender và chạy tool flow. Nó không thay cho W25Q32/Internal Flash/AS32
board test, fault injection hoặc ESP-IDF build.

## Script tiện dụng

```bash
./scripts/build_firmware.sh all
./scripts/test_host.sh
./scripts/check_all.sh
```

`check_all.sh` build Debug/Release, compile board image Phase 4, sinh báo cáo
size/disassembly và chạy toàn bộ host regression. Script không nạp firmware và
không tự ghi trạng thái hardware/RF thành PASS.
