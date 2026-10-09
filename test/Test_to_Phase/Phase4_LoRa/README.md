# Phase 4 — AS32 LoRa P2P Bring-up

> **Tác dụng:** Build, chạy và lưu evidence cho adapter AS32 hai đầu cùng link P2P
> thô. Test này chưa truyền firmware và chưa phải secure OTA.

> **Trạng thái:** Host/ARM source PASS; ESP-IDF build và RF/board test
> PENDING HARDWARE/ENVIRONMENT.

## Thành phần

| File/target | Tác dụng |
|---|---|
| host/phase4_lora_host_tests.c | Test config codec, packet boundary, endian, CRC và input lỗi trên host. |
| phase4_packet.* | Packet thô tạm của STM32 board test. Không dùng lại làm protocol Phase 5. |
| board_main.c | STM32 đọc config/version rồi echo packet hợp lệ về ESP32. |
| aegis_phase4_lora_board_test | Firmware test độc lập đặt tại 0x08000000. |
| esp32_gateway/components/as32/ | Adapter UART/MD0/MD1/AUX phía ESP32. |
| esp32_gateway/main/ | Radio task/queue và chương trình gửi/đo echo. |

## 1. Host regression

~~~bash
cmake -S test/Test_to_Phase/Phase4_LoRa/host \
      -B build/phase4-host -G Ninja
cmake --build build/phase4-host
ctest --test-dir build/phase4-host --output-on-failure
~~~

Expected: phase4_lora_contract PASS; kết quả hiện tại là 52 checks, 0 failures,
bao gồm cross-test packet ESP32 ↔ STM32.

## 2. Build STM32 board echo

~~~bash
cmake --preset debug
cmake --build build/debug --target aegis_phase4_lora_board_test
~~~

Nạp:

~~~bash
cmake --build build/debug --target aegis_phase4_lora_board_test_flash
~~~

Firmware test dùng vector/linker ở 0x08000000 để chạy độc lập, vì vậy lệnh flash
sẽ ghi đè Bootloader đang có trên board. Sau phép thử phải flash lại Bootloader
production trước khi quay về test OTA.

Các biến quan sát qua GDB:

| Biến | Ý nghĩa |
|---|---|
| phase4_read_config | 5 config byte đọc thật từ AS32 #2. |
| phase4_version[], phase4_version_length | Response version của AS32 #2. |
| phase4_valid_rx_count | Packet ESP32 → STM32 hợp lệ. |
| phase4_invalid_rx_count | Packet sai format/CRC/direction hoặc lỗi RX. |
| phase4_tx_count | Echo STM32 → ESP32 đã gửi. |
| phase4_timeout_count | Số lần chờ packet hết timeout. |
| phase4_last_lora_status | Status gần nhất của adapter STM32. |
| phase4_last_packet_status | Status decode/encode packet gần nhất. |

## 3. Build ESP32 Gateway

ESP-IDF là build tree riêng, không chạy bằng preset ARM ở root:

~~~bash
cd esp32_gateway
idf.py set-target esp32
idf.py menuconfig
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
~~~

Trước khi flash, kiểm lại GPIO trong menu Aegis AS32 hardware. UART0 giữ cho
flash/log; baseline AS32 dùng UART2 ở 9600.

## 4. Thứ tự chạy trên phần cứng

1. Gắn đúng antenna, kiểm nguồn, TX/RX chéo và GND chung.
2. Chạy riêng từng MCU; lưu config/version read-back, chưa ghi persistent config.
3. So hai profile; chỉ sửa có chủ đích nếu channel/air-rate khác nhau.
4. Cho STM32 chạy echo, sau đó chạy ESP32 với payload 0, 1, 255, 256.
5. Lưu số PASS/FAIL, RTT, throughput, khoảng cách, môi trường và nguồn.
6. Test thêm AUX stuck, tắt/reset một module, packet corrupt, burst và soak.

## 5. Điều kiện đổi sang PASS

- Adapter STM32 và ESP32 chạy được trên hardware thật.
- Hai AS32 có inventory/profile read-back tái lập.
- Hai chiều binary pass, gồm byte 0x00.
- Có MTU thực tế, latency, loss và throughput đo được.
- Timeout/disconnect/reset không treo hai MCU.

Cho đến khi đủ các bằng chứng trên, Phase 4 giữ PENDING HARDWARE; compile và
host test không thay thế phép đo RF.
