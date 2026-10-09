# Repository Structure

> **Phạm vi (Scope):** Chốt source tree production; file
> chưa có được tạo đúng 
> checkpoint roadmap, không dựng trước một cây thư mục song song.

> **Tác dụng:** Giúp đặt source, header, tool, test và tài liệu đúng vị trí thống nhất.


```text
bootloader/
├── inc/
├── src/
└── CMakeLists.txt

application/
├── inc/
├── src/
└── CMakeLists.txt

common/
├── inc/
├── src/
├── startup_stm32f103_md.s
└── CMakeLists.txt

shared/
├── memory_map.h
├── metadata_struct.h
├── firmware_format.h
├── crypto_config.h
├── artifact_manifest.h
├── ota_protocol.h
├── boot_mailbox.h
├── delta_format.h
├── session_journal.h
└── CMakeLists.txt

driver/
├── inc/{ext_flash,lora_as32,timebase,boot_mailbox_io,watchdog}.h
├── src/{ext_flash,lora_as32,lora_as32_protocol,timebase,boot_mailbox_io,watchdog}.c
└── CMakeLists.txt

libraries/
└── Third_Party/Monocypher/

esp32_gateway/
├── components/as32/
├── components/ota_sender/
├── main/
├── CMakeLists.txt
└── sdkconfig.defaults

tools/
├── sign_firmware.py
├── make_delta.py
├── package_transfer.py
├── make_factory_metadata.py
├── delta_benchmark.py
├── create_release_manifest.py
└── recovery_tool.py

scripts/
├── build_firmware.sh
├── test_host.sh
└── check_all.sh

test/Test_to_Phase/
├── Phase3_Crypto/
├── Phase4_LoRa/
└── Phase5_8_Host/

docs/
├── LoRa/
├── Roadmap/
├── Reference/
└── *.md
```

Production code không chứa fake/mock. Test double chỉ nằm trong `test/`.
`driver/inc` và `driver/src` là layout production canonical; không tạo thêm
`driver/uart`, `driver/as32` hoặc `driver/w25q32`; source mới phải được cập
nhật đồng thời trong CMake target và include path.

Application sở hữu `secure_transport`, `ota_transport`, `artifact_storage` và
Delta applier. Bootloader sở hữu metadata, mailbox consumer, W25 read-only
validator, Internal Flash installer/rollback và jump. `firmware_auth` là
verify-only source được compile cho cả hai image; nó không chứa ChaCha transport.

`esp32_gateway/` là project ESP-IDF độc lập. Root CMake ARM không gọi vào thư
mục này và ESP-IDF cũng không compile source STM32.

`shared/cu/` và `bootloader/Backup/` chỉ là bản đối chiếu lịch sử. Production
include/CMake chỉ được trỏ tới header canonical trực tiếp trong `shared/` và
source trực tiếp trong các thư mục `src/`.

Ba script chỉ gom các lệnh CMake/CTest canonical; chúng không tự flash board và
không biến software build thành hardware PASS.
