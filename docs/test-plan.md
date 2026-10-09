# Test Plan

> **Phạm vi:** Thứ tự test, mục đích của từng tầng và bằng chứng cần lưu để một checkpoint được coi là PASS.

> **Tác dụng:** Hướng dẫn tổ chức host, board, fault và RF test theo đúng acceptance.

## 1. Nguyên tắc

- Test tầng thấp PASS trước khi dùng nó làm dependency cho tầng cao.
- Host test kiểm logic thuần; board test kiểm driver/timing/phần cứng thật.
- Mock chỉ nằm trong test, không thay implementation production.
- Một test phải có input, expected output và điều kiện PASS quan sát được.
- `PASS`, `FAIL` và `PENDING HARDWARE` phải tách rõ; compile không thay board PASS.
- Lỗi ở một bước phải được truyền lên caller và không làm test tiếp tục như PASS.

## 2. Test pyramid

| Tầng | Test gì | Lý do cần tầng này | Evidence tối thiểu |
|---|---|---|---|
| Host unit | CRC, range, serialization, metadata A/B, state decision, parser, crypto vector | Bắt logic sai nhanh, deterministic, không phụ thuộc board | Command + số test + PASS log |
| STM32 primitive | Internal Flash, SPI/UART, timeout, alignment | Chứng minh code giao tiếp peripheral thật | Board/profile + marker/UART/GDB log |
| W25Q32 storage | JEDEC, erase/program/read-back, Staging, manifest/Backup, Session Journal | Chứng minh persistence, chống replay và boundary thật | Vùng test bị erase + result + read-back |
| AS32 link | Config read-back, AUX, P2P hai chiều, timeout | Chứng minh link radio thô trước secure protocol | Profile + payload TX/RX + khoảng cách |
| Secure transport | AEAD, session/counter, replay, ACK/NACK | Chứng minh frame giả/sửa/replay không đổi state | Positive/negative logs |
| FULL OTA E2E | Receive, verify, backup, install, trial, rollback | Baseline correctness toàn hệ thống | Artifact hash/version + final metadata/App |
| Delta OTA E2E | Receive patch, exact base, reconstruct, verify/install | Chứng minh optimization không phá baseline | Host target hash = Staging target hash |
| Fault/security | Reset, mất điện, corrupt, replay, wrong key/base | Chứng minh fail-closed và recovery deterministic | Fault point + state trước/sau + outcome |
| RF/performance | Bytes, packets, retry, time, throughput | Chứng minh Delta/timeout/profile có cơ sở đo | Dataset/profile + bảng số liệu |

```text
Host unit
 -> board primitives
 -> W25Q32
 -> AS32
 -> secure P2P
 -> FULL OTA
 -> DELTA OTA
 -> fault/security/RF acceptance
```

## 3. Mẫu một test case

```text
Test ID:
Mục tiêu:
Dependency:
Hardware/profile:
Input/fixture:
Command/cách chạy:
Fault point (nếu có):
Expected:
Actual:
Evidence path:
Result: PASS | FAIL | PENDING HARDWARE
```

Tên test nên mô tả hành vi, ví dụ
`metadata_reset_before_commit_marker_keeps_old_record`, không chỉ đặt `test_07`.

## 4. Quy tắc test storage/RX

- Ghi rõ sector/page nào sẽ bị erase trước khi chạy board test.
- Test first/last byte, boundary, overflow, zero length và cross-page/cross-sector.
- DATA ACK chỉ được quan sát sau read-back + progress commit.
- Drop ACK phải tạo duplicate idempotent, không tạo dữ liệu khác.
- Reset giữa manifest body và marker phải để record invalid.
- Không dùng vùng W25Q32 chứa artifact cần giữ làm vùng test phá hủy.

## 5. Quy tắc test install/trial

Tại mỗi persistent checkpoint, test ít nhất ba trường hợp:

1. Không fault: transaction hoàn thành.
2. Reset ngay trước commit: record cũ vẫn là source of truth.
3. Reset ngay sau commit: Bootloader tiếp tục đúng state mới.

Outcome cuối hợp lệ chỉ được là:

- App cũ hợp lệ + metadata `IDLE`;
- App mới đã confirm + metadata `IDLE`;
- Recovery vì không còn image an toàn hoặc không thể commit metadata.

Không chấp nhận boot image chưa verify, loop vô hạn hoặc half-committed record.

## 6. Regression order

Sau khi sửa shared format, memory map, crypto contract hoặc persistent state, chạy
lại từ tầng thấp nhất bị ảnh hưởng. Ví dụ đổi metadata schema phải chạy lại host
layout/CRC/A-B/state-machine trước board install; đổi frame header phải chạy parser
và crypto negative trước radio E2E.

## 7. Điều kiện đóng một Phase

Một Phase chỉ DONE khi:

- toàn bộ dependency ghi trong roadmap đã PASS;
- source production build với warning gate đã chốt;
- host regression PASS;
- board/RF test bắt buộc PASS hoặc được ghi rõ `PENDING HARDWARE` nếu roadmap cho
  phép đi tiếp;
- log, binary/hash, profile và expected marker đủ để người khác chạy lại;
- trạng thái trong `roadmap-status.md` và `validation-evidence.md` khớp evidence.
