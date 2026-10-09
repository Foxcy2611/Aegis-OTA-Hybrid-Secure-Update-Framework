# Sơ đồ luồng tổng thể Aegis-OTA P2P

> **Phạm vi:** Các luồng production từ phát hành artifact, truyền P2P, lưu W25,
> install, Trial Boot đến rollback/recovery.

> **Tác dụng:** Cho phép nhìn riêng từng luồng hệ thống theo sơ đồ dọc, tránh
> trộn ownership và trạng thái của ESP32, Application và Bootloader.

## 1. Tổng quan các thành phần

```mermaid
flowchart TB
    RELEASE["Release / PC / CI<br/>Build, ký FULL và tạo Delta"]
    ESP["ESP32 Gateway Controller<br/>Cache, fragment, retry, resume"]
    AS1["AS32 #1<br/>UART 9600 sang LoRa"]
    AIR["LoRa P2P"]
    AS2["AS32 #2<br/>LoRa sang USART1 9600"]
    APP["STM32 Application Update Agent<br/>Secure RX và chuẩn bị artifact"]
    W25["W25Q32<br/>Staging, Backup, Delta, Session Journal"]
    REQUEST["UPDATE_READY<br/>Software reset"]
    BOOT["STM32 Bootloader<br/>Verify, decide, install, rollback"]
    INTERNAL["Internal Application<br/>tối đa 46 KB"]
    RUNTIME["Application runtime<br/>Self-check và Trial Confirm"]

    RELEASE --> ESP
    ESP --> AS1
    AS1 --> AIR
    AIR --> AS2
    AS2 --> APP
    APP --> W25
    APP --> REQUEST
    REQUEST --> BOOT
    W25 --> BOOT
    BOOT --> INTERNAL
    INTERNAL --> RUNTIME
    RUNTIME -->|"TRIAL_CONFIRM + reset"| BOOT
```

## 2. Phát hành và truyền artifact qua LoRa P2P

```mermaid
flowchart TB
    SOURCE["Source Application"]
    BUILD["CMake + Ninja<br/>Sinh Application BIN"]
    HEADER["Tạo SignedHeader 32 bytes"]
    HASH["SHA-512 prehash 64 bytes"]
    SIGN["Ed25519ph signature 64 bytes"]
    FULL["FULL artifact"]
    DELTA["Delta generator<br/>base + target"]
    SELF["Host self-apply và verify target"]
    PACKAGE["DELTA package"]
    CACHE["ESP32 cache artifact"]
    FRAGMENT["Chia fragment"]
    SECURE["ChaCha20-Poly1305 secure frame"]
    TX["ESP32 UART 9600"]
    AS1["AS32 #1"]
    RADIO["LoRa P2P"]
    AS2["AS32 #2"]
    RX["STM32 USART1 9600"]

    SOURCE --> BUILD
    BUILD --> HEADER
    HEADER --> HASH
    HASH --> SIGN
    SIGN --> FULL
    SIGN --> DELTA
    DELTA --> SELF
    SELF --> PACKAGE
    FULL --> CACHE
    PACKAGE --> CACHE
    CACHE --> FRAGMENT
    FRAGMENT --> SECURE
    SECURE --> TX
    TX --> AS1
    AS1 --> RADIO
    RADIO --> AS2
    AS2 --> RX
```

## 3. Nhận FULL vào Staging

```mermaid
flowchart TB
    RX["USART1 nhận bytes từ AS32 #2"]
    FRAME["Tách một secure frame hoàn chỉnh"]
    AUTH["Kiểm session, counter, TAG<br/>rồi decrypt"]
    COMMAND["Parse BEGIN / DATA / END"]
    BEGIN["BEGIN<br/>Kiểm identity, size và chunk"]
    PREPARE["Erase Staging Manifest<br/>và các sector image cần dùng"]
    HEADER["Ghi manifest header body"]
    HEADERMARK["Ghi header commit marker cuối"]
    DATA["DATA fragment"]
    RANGE["Kiểm offset, length và duplicate"]
    PROGRAM["Program Staging Image"]
    READBACK["Read-back và so sánh"]
    PROGRESS["Clear progress bit 1 thành 0"]
    ACK["Gửi ACK committed"]
    COMPLETE{"Đã đủ fragment?"}
    VERIFY["Kiểm CRC và chữ ký target sơ bộ"]
    VALID["Ghi artifact-valid marker"]
    REJECT["Reject / NACK<br/>không publish artifact"]

    RX --> FRAME
    FRAME --> AUTH
    AUTH --> COMMAND
    COMMAND --> BEGIN
    BEGIN --> PREPARE
    PREPARE --> HEADER
    HEADER --> HEADERMARK
    HEADERMARK --> DATA
    DATA --> RANGE
    RANGE --> PROGRAM
    PROGRAM --> READBACK
    READBACK --> PROGRESS
    PROGRESS --> ACK
    ACK --> COMPLETE
    COMPLETE -->|"Chưa"| DATA
    COMPLETE -->|"Đủ"| VERIFY
    VERIFY -->|"PASS"| VALID
    VERIFY -->|"FAIL"| REJECT
```

## 4. Nhận và áp dụng Delta

```mermaid
flowchart TB
    RX["Nhận DELTA BEGIN / DATA / END"]
    MANIFEST["Ghi Delta Manifest"]
    PATCH["Ghi Delta Data theo fragment"]
    READBACK["Read-back rồi commit progress"]
    COMPLETE{"Patch đã nhận đủ?"}
    PATCHVERIFY["Verify Delta manifest,<br/>patch hash và Delta signature"]
    BASE["Kiểm exact base<br/>version, size và hash"]
    APPLYMARK["Commit apply-started marker"]
    ERASE["Erase Staging Image"]
    STREAM["Streaming reconstruct<br/>Internal App + Delta Data"]
    TARGET["Ghi target vào Staging"]
    TARGETVERIFY["Verify target CRC,<br/>SHA-512 và Ed25519ph"]
    PUBLISH["Publish Staging Manifest<br/>và artifact-valid marker"]
    RESTART["Reset giữa apply:<br/>erase Staging và reconstruct lại từ đầu"]
    REJECT["Reject Delta<br/>giữ App hiện tại"]

    RX --> MANIFEST
    MANIFEST --> PATCH
    PATCH --> READBACK
    READBACK --> COMPLETE
    COMPLETE -->|"Chưa"| PATCH
    COMPLETE -->|"Đủ"| PATCHVERIFY
    PATCHVERIFY -->|"PASS"| BASE
    PATCHVERIFY -->|"FAIL"| REJECT
    BASE -->|"Khớp"| APPLYMARK
    BASE -->|"Sai base"| REJECT
    APPLYMARK --> ERASE
    ERASE --> STREAM
    STREAM --> TARGET
    TARGET --> TARGETVERIFY
    TARGETVERIFY -->|"PASS"| PUBLISH
    TARGETVERIFY -->|"FAIL"| REJECT
    APPLYMARK -.->|"Phát hiện reset dở"| RESTART
    RESTART --> ERASE
```

## 5. Chuẩn bị Backup và chuyển quyền cho Bootloader

```mermaid
flowchart TB
    STAGING["Staging target đã complete<br/>và preliminary verify PASS"]
    INVALIDATE["Invalidate Backup Manifest"]
    ERASE["Erase đủ Backup Image sectors"]
    COPY["Đọc Internal App cũ<br/>và ghi Backup Image"]
    READBACK["Read-back + CRC"]
    MANIFEST["Ghi Backup Manifest body"]
    MARKER["Ghi Backup commit marker cuối"]
    REQUEST["Ghi UPDATE_READY mailbox<br/>đúng operation ID"]
    RESET["Software reset"]
    BOOT["CPU bắt đầu lại từ Bootloader"]

    STAGING --> INVALIDATE
    INVALIDATE --> ERASE
    ERASE --> COPY
    COPY --> READBACK
    READBACK --> MANIFEST
    MANIFEST --> MARKER
    MARKER --> REQUEST
    REQUEST --> RESET
    RESET --> BOOT
```

## 6. Bootloader quyết định sau reset

```mermaid
flowchart TB
    RESET["Reset_Handler"]
    META["Đọc và validate Metadata A/B"]
    MAILBOX["Đọc, validate và consume mailbox"]
    INPUT["Verify độc lập<br/>App, Staging và Backup"]
    DECIDE{"BootState_Decide"}
    START["BOOT_ACTION_START_APP"]
    INSTALL["BOOT_ACTION_INSTALL_UPDATE"]
    ROLLBACK["BOOT_ACTION_ROLLBACK"]
    RECOVERY["BOOT_ACTION_RECOVERY"]
    STARTEXEC["Commit trial attempt nếu cần<br/>Relocate VTOR và jump"]
    INSTALLEXEC["Commit INSTALL_IN_PROGRESS<br/>Install target từ Staging"]
    ROLLBACKEXEC["Commit ROLLBACK_IN_PROGRESS<br/>Restore App từ Backup"]
    RECOVERYEXEC["Không jump App<br/>Chờ recovery policy"]

    RESET --> META
    META --> MAILBOX
    MAILBOX --> INPUT
    INPUT --> DECIDE
    DECIDE --> START
    START --> STARTEXEC
    DECIDE --> INSTALL
    INSTALL --> INSTALLEXEC
    DECIDE --> ROLLBACK
    ROLLBACK --> ROLLBACKEXEC
    DECIDE --> RECOVERY
    RECOVERY --> RECOVERYEXEC
```

## 7. Install và Trial Boot

```mermaid
flowchart TB
    ACTION["Action = INSTALL_UPDATE"]
    VERIFY["Verify lại Staging và Backup"]
    STATE["Commit INSTALL_IN_PROGRESS"]
    ERASE["Erase Internal Application pages"]
    PROGRAM["Program target từ Staging"]
    READBACK["Read-back + CRC + signature"]
    TRIAL["Commit IDLE + trial flag<br/>attempt = 1 trước lần jump đầu"]
    ATTEMPT{"attempt nhỏ hơn 3?"}
    INCREASE["Commit attempt + 1"]
    WATCHDOG["Bật IWDG theo policy"]
    JUMP["Relocate VTOR và jump candidate"]
    SELF["Candidate self-check"]
    CONFIRM["Ghi TRIAL_CONFIRM mailbox"]
    RESET["Software reset"]
    CHECKCONFIRM["Bootloader validate<br/>TRIAL_CONFIRM + operation ID"]
    COMMIT["Bootloader commit confirmed<br/>IDLE, trial = 0, attempt = 0"]
    IWDG["Crash, hang hoặc không confirm<br/>IWDG reset"]
    RELOAD["Bootloader đọc lại Metadata<br/>và trial attempt"]
    ROLLBACK["Chọn action ROLLBACK"]

    ACTION --> VERIFY
    VERIFY --> STATE
    STATE --> ERASE
    ERASE --> PROGRAM
    PROGRAM --> READBACK
    READBACK --> TRIAL
    TRIAL --> ATTEMPT
    ATTEMPT -->|"Còn lượt"| INCREASE
    INCREASE --> WATCHDOG
    WATCHDOG --> JUMP
    JUMP --> SELF
    SELF -->|"Healthy"| CONFIRM
    CONFIRM --> RESET
    RESET --> CHECKCONFIRM
    CHECKCONFIRM --> COMMIT
    SELF -->|"Không healthy"| IWDG
    IWDG --> RELOAD
    RELOAD --> ATTEMPT
    ATTEMPT -->|"Hết lượt"| ROLLBACK
```

## 8. Rollback về Application cũ

```mermaid
flowchart TB
    ACTION["Action = ROLLBACK"]
    BACKUP{"Backup hợp lệ?"}
    STATE["Commit ROLLBACK_IN_PROGRESS"]
    ERASE["Erase Internal Application"]
    RESTORE["Program App cũ từ Backup Image"]
    VERIFY["Read-back + CRC + signature"]
    METADATA["Commit metadata App cũ<br/>IDLE, trial = 0, attempt = 0"]
    JUMP["Relocate VTOR và jump App cũ"]
    RECOVERY["RECOVERY<br/>không còn image an toàn"]

    ACTION --> BACKUP
    BACKUP -->|"Có"| STATE
    STATE --> ERASE
    ERASE --> RESTORE
    RESTORE --> VERIFY
    VERIFY -->|"PASS"| METADATA
    METADATA --> JUMP
    BACKUP -->|"Không"| RECOVERY
    VERIFY -->|"FAIL"| RECOVERY
```
