# Aegis-OTA — Docs Index

> **Tác dụng:** Là mục lục để tìm đúng đặc tả trước khi thiết kế, code hoặc kiểm thử.

> **Mục đích:** File này chỉ dùng để **phân loại và giới thiệu các file đặc tả trong `Docs/`**.
> Mỗi nhóm tài liệu được đặt trong **một bảng riêng**, để khi cần code/review một phần nào đó có thể mở đúng nhóm và đúng file.

> **Phạm vi:** Aegis-OTA chạy **LoRa P2P với ESP32 Gateway Controller + 2 AS32 + STM32F103 + W25Q32**.

---

# 1. Kiến trúc và thiết kế lõi

Nhóm này mô tả **project được chia thành những thành phần nào, mỗi thành phần chịu trách nhiệm gì, bộ nhớ nằm ở đâu và Bootloader quyết định boot/update ra sao**.

| Tài liệu | Nội dung của file đặc tả |
|---|---|
| [system-scope.md](system-scope.md) | Chốt phạm vi toàn project: STM32F103, ESP32 Gateway Controller, 2 AS32, W25Q32, Release/CI. File này nêu mục tiêu hệ thống, thành phần nào nằm trong scope, thành phần nào ngoài scope, authority cuối thuộc về ai và các bất biến như không patch trực tiếp Application, FULL luôn là fallback. |
| [architecture.md](architecture.md) | Mô tả kiến trúc end-to-end từ lúc firmware được build/ký ở PC/CI, đi qua ESP32, AS32 #1, link LoRa P2P, AS32 #2 tới STM32 và W25Q32. File này cũng phân chia ownership: Release làm gì, ESP32 làm gì, AS32 làm gì, STM32 làm gì. |
| [component-responsibilities.md](component-responsibilities.md) | Phân bổ chi tiết trách nhiệm và quyền hạn của Release/CI, ESP32, hai AS32, STM32 Application, Bootloader, W25Q32; đồng thời chỉ rõ bên nào được ghi Staging/Backup/Delta, mailbox, Metadata và Internal Application. |
| [system-flow-diagram.md](system-flow-diagram.md) | Bộ sơ đồ Mermaid dọc, tách riêng tổng quan, truyền LoRa, FULL, Delta, Backup, Boot decision, Trial Confirm và Rollback để dễ đọc từng luồng. |
| [hardware-connection-diagram.md](hardware-connection-diagram.md) | Sơ đồ phần cứng mức module và bảng nối chân cho PC/ESP32, hai AS32, STM32F103, W25Q32, ST-Link, UART debug, nguồn, GND và antenna; kèm trình tự bring-up an toàn. |
| [end-to-end-operation-runbook.md](end-to-end-operation-runbook.md) | Hướng dẫn chạy hệ thống hoàn chỉnh: build/provision, ESP32 nhận artifact, `BEGIN/DATA/END/STATUS/BOOT`, retry/resume khi mất gói, Trial Confirm, rollback, Delta fallback và lưu evidence. |
| [repository-structure.md](repository-structure.md) | Quy định cách tổ chức source code trong repository: `bootloader/`, `application/`, `shared/`, `driver/`, `libraries/`, `esp32_gateway/`, `tools/`, `test/`, `docs/`. File này giúp tránh việc code production, test mock và shared contract bị trộn lẫn. |
| [reference-implementation-status.md](reference-implementation-status.md) | Tóm tắt module/tool đã có trong source mẫu và các hardware gate vẫn phải để PENDING. |
| [memory-map.md](memory-map.md) | Chốt toàn bộ memory map: Bootloader 16 KB, Application 46 KB, Metadata A/B mỗi vùng 1 KB, SRAM 20 KB, W25Q32 4 MB với STAGING/BACKUP/DELTA và Session Journal trong RESERVED. Đây là file phải đọc trước khi sửa linker, Flash driver hoặc Delta storage. |
| [internal-flash-contract.md](internal-flash-contract.md) | Mô tả quy tắc erase/program/read-back Internal Flash của STM32: vùng nào được phép ghi, Application bắt đầu ở đâu, khi nào được erase App cũ, cách tránh ghi tràn sang Metadata và các điều kiện trước khi bắt đầu install. |
| [metadata-and-boot-decision.md](metadata-and-boot-decision.md) | Chốt Metadata v2 đúng 128 bytes, cách đọc/chọn A/B, generation/CRC/commit-last và lý do phải giữ active SignedHeader/signature để verify App sau reset. Internal state chỉ giữ install/rollback; receive/Delta state thuộc W25Q32. |
| [boot-state-machine.md](boot-state-machine.md) | Chốt hàm quyết định thuần với metadata status, App/Staging/Backup validity, UPDATE_READY, trial và action `START_APP`, `INSTALL_UPDATE`, `ROLLBACK`, `RECOVERY`. |
| [boot-request-mailbox.md](boot-request-mailbox.md) | Chốt mailbox 20 bytes trên 10 BKP register cho `UPDATE_READY` và `TRIAL_CONFIRM`, operation ID, CRC, commit-last và reset behavior. |
| [boot-install-recovery.md](boot-install-recovery.md) | Mô tả Application chuẩn bị Staging/Backup rồi reset; Bootloader verify độc lập, install, Trial Boot, Confirm hoặc Rollback. Recovery chỉ là last-resort. |
| [rollback-policy.md](rollback-policy.md) | Chốt riêng logic rollback. Rollback thành công phải restore BACKUP, verify App cũ, commit old version + `IDLE`, rồi boot App cũ. File này cũng ghi rõ những trường hợp nào mới được phép chuyển sang Recovery. |
| [trial-boot-and-iwdg.md](trial-boot-and-iwdg.md) | Mô tả Trial Boot, boot attempt counter, Application self-check, health confirmation và IWDG. File này giải thích vì sao firmware mới chưa được coi là active chỉ vì install xong. |
| [w25q32-storage.md](w25q32-storage.md) | Mô tả Staging/Backup/Delta manifest + data, Application/Bootloader ownership, progress bitmap, ACK committed, duplicate/resume, reset và validation policy. |

---

# 2. Bảo mật và ranh giới tin cậy

Nhóm này mô tả **firmware được tin bằng cách nào, secure frame được bảo vệ ra sao và key nào thuộc về ai**.

| Tài liệu | Nội dung của file đặc tả |
|---|---|
| [trust-model.md](trust-model.md) | Chốt trust boundary của project. Release/CI là nơi duy nhất có private signing key; ESP32 và AS32 chỉ vận chuyển dữ liệu; STM32 giữ public key và là authority cuối cho verify/install/boot. |
| [threat-model.md](threat-model.md) | Liệt kê tài sản cần bảo vệ, attack surface, threat in-scope và out-of-scope. Bao gồm firmware giả, replay, sửa frame, wrong base Delta, malformed patch, mất điện giữa transaction và reset thiết bị. |
| [secure-transport-chacha20-poly1305.md](secure-transport-chacha20-poly1305.md) | Đặc tả secure P2P frame do ESP32 và STM32 Application thực hiện; Bootloader không tham gia transport. File ghi rõ `SESSION_ID 8 bytes`, `FRAME_COUNTER 4 bytes`, `TAG 16 bytes`, nonce 12 bytes, AAD 5 bytes, transport key 32 bytes và fixed frame overhead 39 bytes. |
| [firmware-auth-ed25519ph-sha512.md](firmware-auth-ed25519ph-sha512.md) | Đặc tả Release/CI ký, Application kiểm tra sơ bộ và Bootloader kiểm tra độc lập với quyền quyết định cuối. `Firmware_SignedHeader_t` dài 32 bytes, SHA-512 digest/prehash 64 bytes, Ed25519ph signature 64 bytes, public key 32 bytes. |
| [key-and-session-lifecycle.md](key-and-session-lifecycle.md) | Phân biệt firmware signing key với transport session key. File mô tả private/public key dùng cho release, session key dùng cho ChaCha20-Poly1305, cách tạo `SESSION_ID`, quản lý TX/RX frame counter và lý do `SEQ` không được dùng thay nonce counter. |
| [security-negative-tests.md](security-negative-tests.md) | Danh sách các test bảo mật phải fail-closed: ciphertext bị sửa, AAD bị sửa, TAG sai, replay counter, wrong session, sửa firmware/header/signature, wrong base Delta, malformed patch, reconstructed target signature sai. |

---

# 3. Giao thức OTA và luồng cập nhật

Nhóm này mô tả **ESP32 và STM32 nói chuyện với nhau như thế nào, FULL OTA chạy ra sao và Delta OTA khác FULL ở đâu**.

| Tài liệu | Nội dung của file đặc tả |
|---|---|
| [ota-p2p-protocol.md](ota-p2p-protocol.md) | Đặc tả logical OTA protocol chạy bên trong secure frame: `BEGIN`, `DATA`, `END`, `ABORT`, `STATUS`, `BOOT` và ACK/NACK response. File giải thích field tối thiểu, ACK committed, duplicate, retry, stop-and-wait, resume; không có remote `CONFIRM` vì ESP32 không được confirm thay Application. |
| [full-ota.md](full-ota.md) | Mô tả FULL-image OTA end-to-end: signed FULL artifact → ESP32 → LoRa P2P → STM32 → W25Q32 STAGING → verify → BACKUP → install → Trial → Confirm/Rollback. Đây là correctness baseline trước khi làm Delta. |
| [delta-ota.md](delta-ota.md) | Mô tả kiến trúc Delta OTA: PC/CI so sánh old/new firmware và tạo patch; STM32 không tìm diff mà chỉ đọc App cũ + patch để reconstruct target vào STAGING. Sau đó target vẫn phải verify Ed25519ph như FULL. |
| [delta-package-format.md](delta-package-format.md) | Chốt các field trong Delta manifest/package: base version/size/hash, target version/size/hash, target SignedHeader 32 bytes, patch size/hash, algorithm/profile, target signature và Delta manifest signature. |
| [delta-streaming-applier.md](delta-streaming-applier.md) | Mô tả patch engine phía STM32 theo hướng bounded/streaming. File này nói về buffer nhỏ, kiểm overflow, malformed opcode, `COPY/ADD/END`, đọc base từ Internal App, đọc patch từ DELTA_DATA và ghi target sang STAGING. |
| [Delta/Delta_Tool_Selection.md](Delta/Delta_Tool_Selection.md) | Chốt source mẫu dùng patch `COPY/ADD/END`, trade-off và gate trước khi đổi sang Detools/format khác. |

---

# 4. ESP32 Gateway Controller và LoRa P2P

Nhóm này mô tả **phần truyền dữ liệu thực tế qua ESP32 và hai AS32**.

| Tài liệu | Nội dung của file đặc tả |
|---|---|
| [esp32-gateway-controller.md](esp32-gateway-controller.md) | Đặc tả vai trò ESP32. ESP32 nhận artifact từ PC/Release, lưu tạm firmware, chia artifact thành fragment, gửi qua AS32 #1, xử lý ACK/NACK, timeout, retry, query resume và báo progress. File cũng ghi rõ ESP32 không giữ private firmware signing key và không có quyền quyết định firmware hợp lệ. |
| [as32-lora-p2p.md](as32-lora-p2p.md) | Đặc tả hardware/profile của 2 module AS32-TTL-100-C: UART, MD0/MD1, AUX, config mode, normal mode, version/config read-back, transparent P2P link và những API production driver cần có. |
| [LoRa/AS32_Hardware_Profile.md](LoRa/AS32_Hardware_Profile.md) | Biểu mẫu inventory, wiring, command, config/version read-back và bảng số liệu của đúng hai module thật dùng để nghiệm thu Phase 4. |
| [radio-profile-and-measurement.md](radio-profile-and-measurement.md) | Quy định các thông số radio cần khóa và các metric cần đo: profile AS32, baud, payload size, packet loss, retry count, total time, throughput, success rate, khoảng cách và môi trường test. Đây là cơ sở để so FULL với DELTA bằng số liệu thực. |

---

# 5. Phát hành firmware và công cụ PC

Nhóm này mô tả **artifact được tạo ra như thế nào trước khi ESP32 gửi xuống node**.

| Tài liệu | Nội dung của file đặc tả |
|---|---|
| [release-and-artifact.md](release-and-artifact.md) | Mô tả pipeline release: build target → sign FULL → tạo Delta cho supported base → host self-apply → verify → publish immutable artifact. File này cũng quy định FULL luôn tồn tại và Delta chỉ được phát hành nếu self-check PASS. |
| [sign-firmware-tool.md](sign-firmware-tool.md) | Đặc tả `tools/sign_firmware.py`: input firmware/version/private-key; output `.aegisfw` = SignedHeader 32 bytes + firmware + Ed25519ph signature 64 bytes. |
| [recovery-tool.md](recovery-tool.md) | Chốt Recovery baseline bằng ST-Link/SWD connect-under-reset và trusted provisioning bundle; USART2 chỉ là debug cục bộ tùy chọn, không phải secure remote recovery. |
| [build-and-toolchain.md](build-and-toolchain.md) | Chốt build policy, linker contract, warning gate, code-size gate và artifact cần lưu. Đặc biệt theo dõi Bootloader 16 KB, Application 46 KB, SRAM 20 KB, ELF/BIN/MAP và toolchain version. |
| [cmake-file-relationships.md](cmake-file-relationships.md) | Giải thích dễ hiểu quan hệ giữa Preset, root CMake, toolchain, options, helper, các library target và hai firmware; kèm sơ đồ Mermaid, nơi thêm source mới và lệnh CMake/Ninja thường dùng. |

---

# 6. Kiểm thử, fault và bằng chứng nghiệm thu

Nhóm này dùng để **chứng minh project thực sự chịu lỗi và hoạt động đúng**, không chỉ mô tả kiến trúc.

| Tài liệu | Nội dung của file đặc tả |
|---|---|
| [test-plan.md](test-plan.md) | Mô tả mục đích/evidence của từng tầng test, mẫu test case, quy tắc test storage/RX/install/trial, regression order và điều kiện đóng Phase; phân biệt rõ host PASS, board PASS và `PENDING HARDWARE`. |
| [fault-injection.md](fault-injection.md) | Chốt toàn bộ fault matrix: reset/cắt nguồn khi staging, Delta receive, reconstruct, metadata write, backup, install, Trial và rollback; đồng thời test reset ESP32/AS32, packet loss, duplicate, delay. File nhấn mạnh `FAULT` không phải production state. |
| [validation-evidence.md](validation-evidence.md) | Quy định một claim PASS phải lưu những gì: Test ID, command, hardware/profile, expected output, actual log, metadata cuối, firmware hash/version và trạng thái PASS/FAIL/PENDING HARDWARE. |
| [benchmark-and-metrics.md](benchmark-and-metrics.md) | Định nghĩa bảng đo FULL vs DELTA gồm artifact bytes, radio payload bytes, packet count, retry count, total time, throughput và success rate để chứng minh Delta có thực sự có lợi. |
| [claim-evidence-map.md](claim-evidence-map.md) | Map các claim dùng trong README/CV sang test/evidence tương ứng. Ví dụ claim “secure transport” phải có crypto negative test; claim “rollback” phải có log restore App cũ + metadata IDLE. |

---

# 7. Theo dõi project và portfolio

Nhóm này phục vụ **đọc nhanh, theo dõi tiến độ và trình bày project cho người khác**.

| Tài liệu | Nội dung của file đặc tả |
|---|---|
| [roadmap-status.md](./Roadmap/roadmap-status.md) | Tóm tắt project đang đứng ở Phase/checkpoint nào, dependency của các Phase P2P và bước tiếp theo cần làm. |
| [terminology.md](terminology.md) | Giải thích các từ dễ gây rối như Artifact, FULL, DELTA, Base, Target, STAGING, BACKUP, Manifest, Trial Boot, Confirm, Rollback và Recovery. |
| [known-limits.md](known-limits.md) | Ghi rõ những gì project không claim: LoRaWAN/FUOTA, fleet scheduling, physical tamper resistance, secure-element storage, guaranteed RF range khi chưa benchmark. |
| [portfolio-summary.md](portfolio-summary.md) | Tóm tắt kỹ thuật toàn project trong một trang để recruiter/interviewer hiểu nhanh hardware, security, OTA, recovery và validation. |
| [portfolio-demo.md](portfolio-demo.md) | Kịch bản demo project: show version → query state → gửi FULL/DELTA → ACK/retry → verify → backup/install → Trial → Confirm, sau đó inject failure để show Rollback. |

---

# Đọc nhanh theo mục tiêu

### Muốn hiểu kiến trúc

```text
system-scope.md
    ↓
architecture.md
    ↓
component-responsibilities.md
    ↓
system-flow-diagram.md
    ↓
memory-map.md
    ↓
metadata-and-boot-decision.md
    ↓
boot-state-machine.md
```

### Muốn hiểu bảo mật

```text
trust-model.md
    ↓
secure-transport-chacha20-poly1305.md
    ↓
firmware-auth-ed25519ph-sha512.md
    ↓
security-negative-tests.md
```

### Muốn hiểu OTA

```text
ota-p2p-protocol.md
    ↓
full-ota.md
    ↓
delta-ota.md
    ↓
delta-package-format.md
    ↓
delta-streaming-applier.md
```

### Muốn code radio/gateway

```text
esp32-gateway-controller.md
    ↓
as32-lora-p2p.md
    ↓
radio-profile-and-measurement.md
```

### Muốn nghiệm thu project

```text
test-plan.md
    ↓
fault-injection.md
    ↓
validation-evidence.md
    ↓
benchmark-and-metrics.md
    ↓
claim-evidence-map.md
```
