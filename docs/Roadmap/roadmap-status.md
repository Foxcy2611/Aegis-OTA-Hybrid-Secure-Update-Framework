# Roadmap Status

> **Phạm vi:** Quy ước theo dõi tiến độ của roadmap P2P final-first.

> **Tác dụng:** Cung cấp nơi duy nhất để ghi trạng thái và điều kiện PASS của từng Phase.

Roadmap canonical là `Roadmap/tutorial_lora_delta_p2p_final_synced.txt`; bản `.md`
là mirror. Tiến độ không nằm trong README tổng thể.

| Phase | Phạm vi | Trạng thái hiện tại |
|---:|---|---|
| 0 | Contract final-first và feasibility | PASS |
| 1 | Internal Flash, Metadata v2, decision, mailbox | PENDING HARDWARE |
| 2 | W25Q32 driver, manifests, Session Journal và storage | PENDING HARDWARE |
| 3 | Crypto, signer và golden vectors | PASS |
| 4 | AS32/UART và link LoRa P2P thô | PENDING HARDWARE |
| 5 | Reliable OTA protocol và ESP32 Gateway | PENDING HARDWARE — source/host PASS |
| 6 | FULL OTA, install, Trial Boot và rollback | PENDING HARDWARE — source/build PASS |
| 7 | Delta package và Application streaming applier | PENDING HARDWARE — source/host PASS |
| 8 | Fault, security, RF/performance và release acceptance | PENDING HARDWARE — tool/build gates PASS |

Status hợp lệ:

```text
NOT STARTED
IN PROGRESS
PASS
PENDING HARDWARE
BLOCKED
```

Một checkpoint chỉ PASS khi source, test và evidence đúng acceptance của chính
checkpoint đó. Compile không thay thế board/RF test.

## Phase 0 checkpoints

| Checkpoint | Kết quả | Evidence chính |
|---:|---|---|
| 0.1 | PASS | Ownership và trust boundary có đúng một owner cho mỗi nhiệm vụ |
| 0.2 | PASS | Internal/W25 memory map, FULL và Delta byte contract có static assert |
| 0.3 | PASS | Metadata v2, receive state và boot action đã khóa |
| 0.4 | PASS | Chunk/progress/duplicate/resume contract đã khóa |
| 0.5 | PASS | Secure frame và logical payload offset/size đã khóa |
| 0.6 | PASS | Mailbox 20 bytes, CRC và commit-last đã khóa |
| 0.7 | PASS | Pinout, AS32 `9600` và optional debug `115200` đã tách |
| 0.8 | PASS | Representative Boot/App fit linker và ownership audit |
| 0.9 | PASS | Build tái lập, artifact/evidence và tài liệu đã audit |

Log đo cụ thể nằm tại
[`test/Test_to_Phase/Phase0_Board/Evidence_Phase0.md`](../../test/Test_to_Phase/Phase0_Board/Evidence_Phase0.md).
Phase 0 chỉ nghiệm thu contract và feasibility; board/RF behavior vẫn phải PASS ở
phase triển khai tương ứng trước khi project được claim hoàn chỉnh.

## Phase 3 checkpoints

| Checkpoint | Kết quả | Evidence chính |
|---:|---|---|
| 3.1 | PASS | Monocypher 4.0.3 pin/hash gate; compile host và ARM |
| 3.2 | PASS | Static assert; nonce/AAD và SignedHeader serialization vectors |
| 3.3 | PASS | IETF ChaCha20-Poly1305 KAT; ciphertext/AAD/TAG/nonce negative cases |
| 3.4 | PASS | Streaming CRC32 + SHA-512 + Ed25519ph; header/image/signature/public-key negative cases |
| 3.5 | PASS | Python signer deterministic; artifact Python được production C verifier chấp nhận |

Evidence và lệnh tái lập nằm tại
[`test/Test_to_Phase/Phase3_Crypto/README.md`](../../test/Test_to_Phase/Phase3_Crypto/README.md).
Kết quả hiện tại: 34 C checks, 0 failures; CTest 2/2 PASS; ARM Debug build PASS.
Phase 3 không có hardware gate. Phase 1/2 vẫn giữ `PENDING HARDWARE`, không bị
host crypto PASS che khuất.

## Phase 4 checkpoints

| Checkpoint | Kết quả | Evidence chính |
|---:|---|---|
| 4.1 | PENDING HARDWARE | Wiring/profile template đã khóa; còn thiếu inventory và config/version read-back của hai module thật |
| 4.2 | PENDING HARDWARE | STM32 adapter, timeout, ring RX và board echo đã build ARM; còn thiếu UART/AUX/AS32 board run |
| 4.3 | PENDING HARDWARE | ESP32 adapter, radio task/queue và link runner đã có source; máy hiện tại chưa có ESP-IDF và chưa chạy AS32 #1 |
| 4.4 | PENDING HARDWARE | Packet/config host regression PASS; còn thiếu P2P hai chiều, MTU, RTT, loss, throughput và soak test |

Lệnh tái lập cùng thứ tự test nằm tại
[`test/Test_to_Phase/Phase4_LoRa/README.md`](../../test/Test_to_Phase/Phase4_LoRa/README.md).
Kết quả hiện tại: host 52 checks/0 failures; STM32 Debug dùng 3,784 byte Flash
và 592 byte RAM; STM32 Release dùng 3,416 byte Flash và 592 byte RAM.
Phase 5–8 đã được triển khai trước làm source/layout đối chiếu. Việc này không
thay đổi dependency nghiệm thu: Phase 4 phải có RF evidence, Phase 1/2 phải chạy
board thật và Phase 5–8 chỉ được đổi thành `PASS` sau các bài test hardware/E2E.

## Phase 5–8 source checkpoints

| Phạm vi | Kết quả hiện có | Phần còn thiếu để PASS |
|---|---|---|
| Secure/reliable transport | Logical codec, STATUS 32 byte, secure frame/session, persistent anti-replay journal, stop-and-wait sender, AS32 gateway adapter và host regression | Session Journal trên W25 thật, hai AS32 thật, timeout/MTU đo thật, loss/retry/resume RF |
| FULL OTA | Application Update Agent, Staging verify, Backup, mailbox, Boot install/trial/rollback | W25Q32 + Internal Flash + reset/power-loss end-to-end |
| Delta OTA | Package/tool, Delta store, SHA-512/signature gate, COPY/ADD streaming applier | Apply trên W25Q32 thật và Delta P2P end-to-end |
| Final acceptance | Debug/Release build fit, host tools/tests, signed release evidence và recovery tool | ESP-IDF build, fault matrix, security/RF/performance evidence |

Host regression mới:

```text
cmake -S test/Test_to_Phase/Phase5_8_Host -B build/host-phase5-8 -G Ninja
cmake --build build/host-phase5-8
ctest --test-dir build/host-phase5-8 --output-on-failure
```

Kết quả hiện tại: 40 C checks, 0 failures; CTest 2/2 PASS. ARM Debug và Release
đều dùng 15,376 byte Flash cho Bootloader, 25,056 byte Flash cho Application.
Cả hai linker partition đều fit.
