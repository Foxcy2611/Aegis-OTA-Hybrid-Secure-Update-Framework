# Metadata and Boot Decision

> **Phạm vi:** Persistent record trong Internal Flash và cách Bootloader chọn action sau reset.

> **Tác dụng:** Hướng dẫn lưu trạng thái bền vững và quyết định boot an toàn sau mọi reset.

> **Bên thực hiện:** STM32 Bootloader là bên duy nhất đọc/commit Metadata A/B và
> đưa ra boot action. Application, ESP32 và AS32 không được sửa metadata hoặc tự
> quyết định install/rollback.

## Metadata A/B

Metadata A và B nằm ở hai page Internal Flash riêng. Khi reset, Bootloader luôn
đọc và validate **cả hai record**, sau đó chọn record hợp lệ mới nhất theo
`generation`; nó không chọn cố định A hoặc B. Mỗi lần commit ghi record mới sang
page không active, verify body rồi program commit marker cuối.

Mỗi page rộng 1 KB vì đó là erase granularity của STM32F103, nhưng chỉ 128 bytes
đầu page chứa một record. Phần còn lại giữ erased; record 128 bytes có layout:

| Offset | Size | Field | Ý nghĩa |
|---:|---:|---|---|
| 0 | 4 | `magic` | Nhận diện record |
| 4 | 4 | `format_version` | Phiên bản schema persistent |
| 8 | 4 | `generation` | Chọn record mới hơn giữa A/B |
| 12 | 1 | `operation_state` | IDLE/INSTALL/ROLLBACK |
| 13 | 1 | `trial_boot_flag` | Candidate đang trong Trial Boot |
| 14 | 1 | `boot_attempt_count` | Số lần đã cho phép trial jump |
| 15 | 1 | `reserved` | Phải bằng 0 |
| 16 | 8 | `operation_id` | Bind transaction với đúng W25 manifest/request |
| 24 | 32 | `active_header` | SignedHeader mô tả App hiện nằm trong Internal Flash |
| 56 | 64 | `active_signature` | Ed25519ph signature xác thực `active_header \|\| App bytes` |
| 120 | 4 | `metadata_crc32` | CRC byte `[0, 120)` |
| 124 | 4 | `commit_marker` | Ghi cuối để publish record |

Mọi integer serialized little-endian. `operation_id` khác 0 khi INSTALL/ROLLBACK
đang dở hoặc khi candidate còn ở Trial Boot. IDLE ổn định sau confirm/rollback có
`operation_id == 0`. Record vừa khít 128 bytes và nằm gọn trong page 1 KB.

Giá trị khóa: magic `0x41454749`, format `2`, commit marker `0x434D4954`;
erased word là `0xFFFFFFFF`.

## Vì sao metadata phải giữ SignedHeader và chữ ký

Vùng Application chỉ chứa raw firmware bytes; chữ ký không được nối vào cuối App
slot. Vì vậy sau một reset, để tự xác thực App hiện tại mà không phụ thuộc
W25Q32, Bootloader cần lấy:

```text
active_header[32]      từ Metadata đã chọn
Application[image_size] từ Internal Flash
active_signature[64]   từ Metadata đã chọn
public_key[32]         cố định trong Bootloader
```

Bootloader tự tính `SHA-512(active_header || Application)` rồi verify
`active_signature` bằng public key. Chữ ký này **không phải chữ ký của record
metadata**; nó là chữ ký firmware do Release/CI tạo và được copy nguyên vẹn qua
Staging/Backup vào metadata khi image trở thành App hiện tại.

`metadata_crc32` và Ed25519ph không thay thế nhau:

| Cơ chế | Bảo vệ điều gì |
|---|---|
| Metadata CRC32 | Phát hiện record hỏng, ghi dở hoặc bit lỗi |
| Commit marker | Chứng minh record đã được publish hoàn chỉnh |
| Ed25519ph signature | Chứng minh header + App bytes đến từ Release/CI hợp lệ |

Public key không lưu trong metadata vì metadata là vùng có thể cập nhật. Private
key chỉ tồn tại ở Release/CI và không bao giờ được flash vào STM32.

Không được bỏ `active_signature` để rút record. Bỏ field này sẽ khiến Bootloader
chỉ còn CRC không có tính xác thực, hoặc phải phụ thuộc vào W25Q32 mỗi lần boot.
Giảm 64 bytes cũng không thu hồi được page Flash nào vì A/B vẫn phải dùng hai
erase page 1 KB để commit an toàn. Layout 128 bytes vì vậy được giữ cố định.

```mermaid
flowchart TB
    MA[Metadata A] --> V[Validate cả A và B]
    MB[Metadata B] --> V
    V --> S[Chọn generation mới nhất]
    S --> H[active_header + active_signature]
    APP[Internal Application bytes] --> AUTH[SHA-512 + Ed25519ph verify]
    H --> AUTH
    PK[Public key trong Bootloader] --> AUTH
    AUTH --> D[Assemble input và chọn boot action]
```

## Persistent operation state

```text
METADATA_OPERATION_IDLE
METADATA_OPERATION_INSTALL_IN_PROGRESS
METADATA_OPERATION_ROLLBACK_IN_PROGRESS
```

Receive FULL/Delta, progress bitmap, Delta apply và Backup preparation được lưu
trong W25Q32 manifest. Internal metadata chỉ checkpoint thao tác có thể phá hủy
Internal Application.

## Chọn record

```mermaid
flowchart TB
    A[Read Metadata A] --> VA{A valid?}
    B[Read Metadata B] --> VB{B valid?}
    VA --> C{A/B result}
    VB --> C
    C -->|Cả 2 đều trống| F[FIRST_BOOT]
    C -->|Chỉ A hợp lệ| SA[Select A]
    C -->|Chỉ B hợp lệ| SB[Select B]
    C -->|Cả A và B hợp lệ| G[So sánh generation với Wrap Rule]
    C -->|Có dữ liệu nhưng sai cả 2| X[Boot sẽ nhảy vào trạng thái lỗi]
```

- Generation bằng nhau chọn A ổn định.
- Chênh đúng `0x80000000` là ambiguous và fail-closed.
- `FIRST_BOOT` không tự động có nghĩa Application bootable.
- Board provision ban đầu phải ghi metadata đúng cho factory Application; nếu
  chưa provision thì FIRST_BOOT đi Recovery/provisioning.

## Định nghĩa validity

`app_valid` chỉ true khi:

- MSP/reset vector nằm trong SRAM/App range hợp lệ;
- `active_header.image_size` nằm trong `APPLICATION_SIZE`;
- CRC của App khớp SignedHeader;
- SHA-512 + Ed25519ph đúng theo public key/policy;
- identity/version khớp metadata được chọn.

Metadata được chọn chỉ cung cấp persistent state và danh tính/bằng chứng của App.
Boot action cuối còn kết hợp kết quả verify App, mailbox, Staging/Backup validity
và Trial policy; chỉ đọc `operation_state` chưa đủ để quyết định boot.

`staging_valid` và `backup_valid` yêu cầu manifest committed, operation/artifact
identity đúng, range hợp lệ, image CRC và Ed25519ph đều PASS.

## Boot priority

```mermaid
flowchart LR
    %% Định nghĩa node đầu vào
    M{Metadata Status}
    
    %% Nhánh lỗi tĩnh
    M -->|CORRUPT / FIRST_BOOT| REC((RECOVERY))
    
    %% Nhánh hợp lệ
    M -->|VALID| OP{Operation State}

    %% Xử lý Rollback
    OP -->|ROLLBACK| B1{Backup Valid?}
    B1 -->|Yes| RB((ROLLBACK))
    B1 -->|No| REC

    %% Xử lý Install
    OP -->|INSTALL| S1{Staging Valid?}
    S1 -->|Yes| IN((INSTALL))
    S1 -->|No, Backup OK| RB
    S1 -->|No, Backup Fail| REC

    %% Xử lý Idle (Trạng thái bình thường)
    OP -->|IDLE| T1{Trial Timeout?}
    T1 -->|Yes, Backup OK| RB
    T1 -->|Yes, Backup Fail| REC
    T1 -->|No| A1{App Valid?}

    A1 -->|No, Backup OK| RB
    A1 -->|No, Backup Fail| REC
    A1 -->|Yes, có UPDATE_READY| VAL[Validate -> INSTALL]
    A1 -->|Yes, không có request| START((START_APP))

    %% Phủ màu sắc để dễ nhìn (Red: Lỗi, Yellow: Đang xử lý, Green: Chạy)
    classDef stateError fill:#d9534f,stroke:#fff,stroke-width:2px,color:#fff,font-weight:bold;
    classDef stateAction fill:#f0ad4e,stroke:#fff,stroke-width:2px,color:#fff,font-weight:bold;
    classDef stateSuccess fill:#5cb85c,stroke:#fff,stroke-width:2px,color:#fff,font-weight:bold;

    class REC stateError;
    class RB stateAction;
    class IN,VAL stateAction;
    class START stateSuccess;
```

Target update sai không làm thiết bị vào Recovery khi App hiện tại vẫn valid.
Bootloader consume request sai, invalidate/reject candidate theo policy rồi boot
App hiện tại.

## Invariant và acceptance

- Không erase record valid duy nhất trước khi record mới commit.
- Không boot default metadata có `active_header.image_size == 0`.
- Không xử lý update mới khi trial chưa confirm/rollback xong.
- Attempt được commit trước mỗi trial jump.
- Reset trước/sau mọi commit chọn được một record hoàn chỉnh.
- Table test bao phủ FIRST_BOOT, CORRUPT, normal, update request, install resume,
  rollback resume, trial còn lượt và trial hết lượt.
