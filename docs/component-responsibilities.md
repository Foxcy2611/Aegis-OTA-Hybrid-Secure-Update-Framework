# Phân bổ trách nhiệm các thành phần

> **Phạm vi:** Phân chia công việc và quyền hạn giữa Release/CI, ESP32, hai AS32,
> STM32 Application, STM32 Bootloader và W25Q32 trong hệ thống Aegis-OTA P2P.

> **Tác dụng:** Giúp đặt code đúng phía, tránh đưa giao thức LoRa vào Bootloader
> hoặc trao quyền install/confirm firmware cho thành phần chỉ làm nhiệm vụ vận chuyển.

> **Bên thực hiện:** Mọi thành phần trong hệ thống phải tuân theo ranh giới trách
> nhiệm này; STM32 Bootloader giữ authority cuối cho verify/install/boot.

## Nguyên tắc phân quyền

Application làm phần việc dài và phức tạp của OTA như nhận LoRa, retry/resume,
ghi artifact và Delta reconstruct. Bootloader giữ phần việc tối thiểu nhưng có
quyền cao nhất: xác thực độc lập, thay đổi Internal Application, Trial Boot và
rollback.

```text
Application chuẩn bị và đề nghị update
                ↓
Bootloader tự kiểm tra lại và quyết định
                ↓
Chỉ Bootloader được thay đổi Internal Application
```

## Trách nhiệm theo thành phần

| Thành phần | Chịu trách nhiệm | Không được làm |
|---|---|---|
| Release/CI | Build firmware, tạo SignedHeader, SHA-512/Ed25519ph signature, tạo Delta, self-apply test và phát hành artifact bất biến | Không đưa private signing key xuống ESP32 hoặc STM32 |
| Cloud/host tùy chọn | Chuyển artifact đã phát hành tới ESP32 Gateway | Không quyết định firmware có hợp lệ với STM32 hay không |
| ESP32 Gateway Controller | Cache artifact, chia fragment, tạo secure frame, gửi, timeout, retry, resume và báo tiến độ | Không ký firmware, không `TRIAL_CONFIRM`, không ra lệnh STM32 bỏ qua verify |
| AS32 #1 | Chuyển UART của ESP32 thành LoRa P2P và chuyển phản hồi LoRa về UART | Không parse frame, không giữ OTA state, không xác thực firmware |
| AS32 #2 | Chuyển LoRa P2P thành USART1 bytes cho STM32 và chuyển ACK/NACK ngược lại | Không ghi W25Q32, không hiểu Staging/Backup/Delta |
| STM32 Application Update Agent | Nhận UART, parse và authenticate secure frame, xử lý BEGIN/DATA/END, duplicate, progress và ACK committed | Không ghi Metadata A/B, không tự erase/program Internal Application, không tự confirm candidate |
| STM32 Application storage | Quản lý Staging/Delta, chuẩn bị Backup, kiểm range, erase đúng sector, program/read-back và commit manifest/progress | Không quyết định artifact trusted chỉ dựa vào marker hoặc CRC |
| STM32 Delta applier | Verify exact base, đọc patch, streaming reconstruct target sang Staging và kiểm target hoàn chỉnh | Không patch trực tiếp Internal Application |
| STM32 Application mới | Chạy self-check và gửi `TRIAL_CONFIRM` đúng operation ID khi thực sự healthy | Không ghi thẳng metadata để tự công nhận chính nó |
| STM32 Bootloader | Đọc Metadata A/B và mailbox, verify App/Staging/Backup độc lập, quyết định action, install, Trial Boot, rollback và jump App | Không xử lý normal LoRa receive, fragment retry/resume hoặc Delta reconstruct |
| W25Q32 | Lưu bền vững Staging, Backup, Delta và Session Journal qua reset | Không tự xác thực hoặc tự di chuyển firmware; đây chỉ là bộ nhớ do STM32 điều khiển |
| Internal Flash STM32 | Chứa Bootloader, Application và Metadata A/B | Application không được program vùng này trong luồng OTA production |
| ST-Link/SWD | Provision ban đầu, debug và recovery cục bộ khi không còn image an toàn | Không thuộc đường OTA P2P bình thường |

## Ownership bên trong STM32

| Module hoặc dữ liệu | Bên ghi/chủ sở hữu | Bên đọc/sử dụng |
|---|---|---|
| `ext_flash` primitive | Driver dùng chung; không giữ OTA policy | Application storage và Bootloader validator/installer |
| Staging Manifest/Image | Application Update Agent | Application verifier và Bootloader |
| Delta Manifest/Data | Application Update Agent | Application Delta verifier/applier |
| Backup Manifest/Image | Application Update Agent chuẩn bị trước reset | Bootloader dùng khi rollback |
| `UPDATE_READY` mailbox | Application ghi và commit-last | Bootloader consume, validate rồi clear |
| `TRIAL_CONFIRM` mailbox | Candidate Application ghi sau self-check | Bootloader consume, validate rồi clear |
| Metadata A/B | Chỉ Bootloader được commit | Bootloader đọc sau mọi reset |
| Internal Application | Chỉ Bootloader được erase/program trong OTA | CPU thực thi; Application/Bootloader có thể đọc để hash hoặc backup |
| Boot state machine | Bootloader | Nhận input đã kiểm chứng và trả về một action thuần |
| Firmware public key | Application có thể dùng để preliminary verify; Bootloader giữ bản tin cậy để verify độc lập | Không thành phần nào được suy ra hoặc giữ private signing key từ public key |
| Transport session key | ESP32 và STM32 Application | Bootloader không tham gia secure LoRa transport bình thường |

## Trách nhiệm theo vùng W25Q32

| Vùng | Producer | Consumer | Ý nghĩa |
|---|---|---|---|
| Staging Manifest | Application Update Agent hoặc Delta applier | Application verifier và Bootloader | Mô tả FULL candidate, identity, signature, progress và marker |
| Staging Image | Application nhận FULL hoặc Delta applier reconstruct | Application verifier và Bootloader installer | Raw target firmware hoàn chỉnh trước install |
| Backup Manifest | Application Update Agent | Bootloader rollback | Mô tả chính xác App cũ tương ứng Backup Image |
| Backup Image | Application Update Agent copy từ Internal App | Bootloader rollback | Đường quay về nếu install/trial thất bại |
| Delta Manifest | Application Update Agent | Application Delta verifier/applier | Mô tả base, target, patch và tiến độ nhận |
| Delta Data | Application Update Agent | Application Delta applier | Raw patch; không phải firmware có thể boot |
| Session Journal trong Reserved | Application secure-session gate | Application secure-session gate | Lưu session ID append-only để chống replay qua reset |
| Future Reserved | Không thành phần production nào | Không thành phần production nào | Giữ nguyên cho mở rộng sau này |

## Luồng quyền hạn qua reset

1. Application chỉ ghi artifact vào W25Q32 và gửi `UPDATE_READY`.
2. Reset chuyển quyền điều khiển về Bootloader.
3. Bootloader không tin kết quả kiểm tra sơ bộ của Application; nó đọc và verify
   lại Metadata, mailbox, Staging và Backup.
4. Chỉ sau khi điều kiện an toàn PASS, Bootloader commit
   `INSTALL_IN_PROGRESS` rồi mới erase/program Internal Application.
5. Candidate được chạy ở Trial Boot, chưa được coi là confirmed.
6. Candidate healthy gửi `TRIAL_CONFIRM`; Bootloader mới commit trạng thái ổn định.
7. Candidate treo, sai hoặc hết attempt dẫn tới rollback bằng Backup hợp lệ.

## Các bất biến không được phá vỡ

- ESP32 và AS32 chỉ vận chuyển; chúng không có firmware-install authority.
- Application không ghi Metadata A/B và không tự cài firmware vào Internal Flash.
- Bootloader không tin marker, CRC hoặc kết quả preliminary verify một cách đơn lẻ.
- ACK DATA chỉ được gửi sau khi data và progress tương ứng đã bền vững.
- Delta luôn reconstruct sang Staging, không patch trực tiếp App đang chạy.
- Commit marker luôn được ghi cuối sau khi body đã program và read-back đúng.
- Private signing key chỉ tồn tại trong Release/CI.
- Recovery chỉ được chọn khi không còn đường boot/install/rollback an toàn.
