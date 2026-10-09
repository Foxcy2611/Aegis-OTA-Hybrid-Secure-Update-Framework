# Validation Evidence

> **Phạm vi:** Evidence bắt buộc để mỗi claim có thể review và tái lập.

> **Tác dụng:** Hướng dẫn lưu log, artifact và trạng thái để chứng minh kết quả nghiệm thu.

```text
Claim
 -> Test ID
 -> Source revision
 -> Hardware/profile
 -> Command
 -> Expected
 -> Actual log
 -> Artifact/hash
 -> PASS / FAIL / PENDING
```

## Evidence cần lưu

- compiler/OpenOCD/Python/library version;
- clean-build ELF/BIN/MAP/disassembly và size report;
- stack-usage report và RAM high-water mark;
- FULL/Delta artifact SHA-512, SignedHeader và signature result;
- AS32 read-back profile, wiring và supply;
- ESP32/STM32 logs với transfer/session/artifact identity đã redact key;
- injected fault point và reset cause;
- final Metadata/manifest state, active App hash/version;
- test count, failed ID và timestamp.

## Evidence isolation

Kết quả từ schema, ownership hoặc wire contract khác được lưu trong mục
`Historical/` và không được dùng để đánh PASS cho roadmap hiện hành. Private key,
transport PSK và plaintext secret không được đưa vào log/evidence.

## Acceptance

- Mỗi feature trong claim-evidence map trỏ được đến test và artifact cụ thể.
- PENDING HARDWARE luôn ghi rõ thiếu board/module/profile nào.
- Representative Bootloader size evidence giữ toàn bộ final call graph.
- Fault PASS phải chứng minh state/image cuối, không chỉ chứng minh thiết bị reset.

## Evidence hiện tại của Phase 4

| Hạng mục | Kết quả | Trạng thái |
|---|---|---|
| AS32 config + packet codec host | 52 checks, 0 failures; có cross-test ESP32 ↔ STM32 | PASS |
| STM32 board image Debug | 3,784 byte Flash, 592 byte RAM | BUILD PASS |
| STM32 board image Release | 3,416 byte Flash, 592 byte RAM | BUILD PASS |
| ESP32 adapter/radio task | Source đã có; môi trường hiện tại chưa cài ESP-IDF | PENDING ENVIRONMENT |
| Hai AS32 thật | Chưa có config/version read-back và link hai chiều | PENDING HARDWARE |
| RF metrics | Chưa có MTU, RTT, loss, throughput và soak log | PENDING HARDWARE |

Build/host PASS chỉ là evidence phần mềm. Phase 4 tổng thể vẫn PENDING HARDWARE
cho tới khi bảng profile và measurement được điền từ đúng hai module.
