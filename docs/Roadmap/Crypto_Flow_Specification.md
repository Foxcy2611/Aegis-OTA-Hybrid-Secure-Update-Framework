# Đặc tả luồng Crypto OTA

## 1. Mục đích và phạm vi

Tài liệu này giải thích hai lớp bảo vệ của Aegis-OTA theo dòng dữ liệu thực tế:

1. **ChaCha20-Poly1305 IETF** bảo vệ từng frame UART: che payload và phát hiện frame bị sửa.
2. **SHA-512 + Ed25519ph** xác thực toàn bộ firmware: chỉ image do máy phát hành ký mới được phép đi đến bước cài đặt.

Đây là **crypto contract mục tiêu của Phase 4**. Phase 3 hiện vẫn dùng payload rõ,
TAG toàn `0x00` và chưa có `SESSION_ID`/`FRAME_COUNTER`; không được nhầm wire format
Phase 3 với format dưới đây.

CRC32 vẫn tồn tại để bắt lỗi vật lý UART nhanh, nhưng không phải cơ chế bảo mật.

## 2. Bốn khái niệm cần phân biệt

| Khái niệm | Phạm vi | Ý nghĩa |
|---|---|---|
| `FIRMWARE_VERSION` | Cả image | Phiên bản Application đang được phát hành, dùng cho anti-downgrade |
| `SESSION_ID` | Một lượt OTA | Mã 8 byte mới cho mỗi lần update; nhận diện cuộc trao đổi hiện tại |
| `SEQ` | Một block DATA | Số thứ tự block firmware cho sliding window; resend block vẫn giữ nguyên `SEQ` |
| `FRAME_COUNTER` | Một lần truyền frame | Số tăng duy nhất cho crypto; resend là lần truyền mới nên dùng counter mới |

Ví dụ block firmware số 5 bị mất:

```text
Lần gửi đầu:   TYPE=DATA, SEQ=5, FRAME_COUNTER=20
Lần gửi lại:   TYPE=DATA, SEQ=5, FRAME_COUNTER=27
```

`SEQ` trả lời “đây là dữ liệu nào?”, còn `FRAME_COUNTER` trả lời “đây là lần truyền thứ
mấy?”. Hai số không thay thế nhau.

## 3. Wire format của frame bảo mật

```text
SYNC | SESSION_ID | FRAME_COUNTER | TYPE | SEQ | LEN | CIPHERTEXT | TAG | CRC32
```

| Trường | Kích thước | Có mã hóa? | Ý nghĩa |
|---|---:|---|---|
| `SYNC` | 2 byte | Không | Luôn `0xAA 0x55`; chỉ để parser đồng bộ |
| `SESSION_ID` | 8 byte | Không | Phiên OTA của frame; phải khớp phiên đang hoạt động |
| `FRAME_COUNTER` | 4 byte | Không | Counter crypto, little-endian |
| `TYPE` | 1 byte | Không | BEGIN, DATA, ACK, NACK, END... |
| `SEQ` | 2 byte | Không | Sequence của sliding window, little-endian |
| `LEN` | 2 byte | Không | Số byte ciphertext/payload, little-endian |
| `CIPHERTEXT` | `LEN` byte | Có | Payload sau ChaCha20; tối đa 256 byte |
| `TAG` | 16 byte | Không | MAC Poly1305 của frame |
| `CRC32` | 4 byte | Không | CRC-32/ISO-HDLC của frame vật lý, little-endian |

Frame dài `39 + LEN` byte, tối đa `295` byte khi `LEN=256`.

CRC32 được tính trên:

```text
SESSION_ID | FRAME_COUNTER | TYPE | SEQ | LEN | CIPHERTEXT | TAG
```

`SYNC` không nằm trong CRC. CRC pass không có nghĩa frame đáng tin; TAG mới là quyết
định bảo mật.

## 4. Nonce, AAD và quy tắc counter

### 4.1 Nonce 12 byte

Nonce không phải key và không cần giữ bí mật. Nó chỉ phải **không bao giờ lặp lại với
cùng ChaCha20 key**.

```text
NONCE[0..7]   = SESSION_ID[0..7]
NONCE[8..11]  = FRAME_COUNTER ở little-endian
```

Ví dụ:

```text
SESSION_ID    = 11 22 33 44 55 66 77 88
FRAME_COUNTER = 05 00 00 00
NONCE         = 11 22 33 44 55 66 77 88 05 00 00 00
```
> Nối đuôi nhau

`SESSION_ID` được PC/ESP32 sinh ngẫu nhiên bằng nguồn ngẫu nhiên an toàn khi mở
`CMD_BEGIN`. Nó được mang ở header của **mọi** crypto frame. Với `BEGIN`, STM32 dùng
session trong header để dựng nonce nhưng chỉ công nhận đó là session đang hoạt động sau
khi TAG của `BEGIN` hợp lệ. Từ frame tiếp theo, session trong header phải khớp session đã
được công nhận.

### 4.2 Counter hai chiều

Một ChaCha key được dùng cho hai chiều UART, nên hai chiều phải không bao giờ sinh cùng
nonce:

```text
PC/ESP32 → STM32 : 0x00000000 .. 0x7FFFFFFF
STM32 → PC/ESP32 : 0x80000000 .. 0xFFFFFFFF
```

Mỗi bên tăng counter cho **mỗi lần phát frame**, kể cả ACK, NACK và resend. Counter
không được quay vòng trong một session. UART giữ thứ tự byte, nên receiver chỉ chấp nhận
counter mới lớn hơn counter hợp lệ gần nhất trong đúng direction/session; frame cũ bị
loại. Receiver chỉ cập nhật `last_valid_counter` **sau khi TAG pass**. Nếu cập nhật ngay
khi mới đọc header, attacker có thể gửi một counter rất lớn với TAG giả để khóa mọi frame
hợp lệ còn lại. Quy tắc này ngăn replay frame cũ trong cùng phiên. DoS vẫn có thể xảy ra
nếu attacker chặn hoặc làm nghẽn UART; crypto không thể loại bỏ DoS vật lý.

### 4.3 AAD

AAD là header để lộ cho parser nhưng vẫn được Poly1305 bảo vệ:

```text
AAD = SESSION_ID | FRAME_COUNTER | TYPE | SEQ | LEN
```

Nếu ai đó đổi `TYPE=DATA` thành `TYPE=END`, đổi `SEQ`, `LEN`, session hoặc counter,
TAG sẽ không hợp lệ.

## 5. Luồng truyền frame UART

### 5.1 PC/ESP32 gửi DATA

1. Chọn `PAYLOAD` tối đa 256 byte và `SEQ` của block cần gửi.
2. Lấy `FRAME_COUNTER` mới thuộc dải PC/ESP32 → STM32.
3. Tạo `NONCE = SESSION_ID | FRAME_COUNTER`.
4. Tạo AAD từ `SESSION_ID | FRAME_COUNTER | TYPE | SEQ | LEN`.
5. Đưa key 32 byte, nonce, AAD và payload vào ChaCha20-Poly1305 IETF.
6. Nhận `CIPHERTEXT` và `TAG` 16 byte.
7. Ghép crypto frame, tính CRC32 ngoài cùng và gửi qua UART.

### 5.2 STM32 nhận DATA

1. UART DMA và parser nhận frame, kiểm tra length và CRC32.
2. Kiểm tra direction và miền `FRAME_COUNTER`. Với frame sau BEGIN, kiểm tra thêm
   `SESSION_ID` khớp session đang mở; chưa cập nhật counter ở bước này.
3. Tạo lại nonce và AAD từ header nhận được.
4. Đưa key trong chip, nonce, AAD, ciphertext và tag vào Monocypher.
5. Nếu `crypto_aead_read()` trả lỗi: tăng bộ đếm lỗi, bỏ frame. Không giải mã, không
   NACK theo dữ liệu chưa xác thực, không ghi staging.
6. Nếu tag đúng: Monocypher giải mã ra payload gốc, sau đó mới cập nhật
   `last_valid_counter`. Nếu đây là BEGIN hợp lệ thì mới công nhận `SESSION_ID` mới.
7. Chỉ lúc này payload mới được đưa vào parser command/sliding window. `SEQ` quyết định
   slot RAM, ACK/NACK và xử lý resend như Phase 3.

Ví dụ window gửi sai thứ tự:

```text
DATA SEQ=2, COUNTER=1
DATA SEQ=0, COUNTER=2
DATA SEQ=3, COUNTER=3
DATA SEQ=1, COUNTER=4
```

Counter vẫn tăng theo lần truyền, còn sliding window dùng `SEQ` để xếp block về đúng slot.

## 6. Luồng xác thực firmware

### 6.1 Signed header 32 byte

Trước khi ký, release tool tạo chính xác 32 byte header sau. Mọi `uint32_t` là
little-endian và byte `0x00` cuối domain là một phần dữ liệu được ký.

```text
SIGNED_DOMAIN[16]       = "AEGIS-OTA-FW-V1\0"
CRYPTO_FORMAT_VERSION   = uint32_t
IMAGE_SIZE              = uint32_t
FIRMWARE_VERSION        = uint32_t
IMAGE_CRC32             = uint32_t
```

Sau header là toàn bộ `FIRMWARE_BYTES` chưa mã hóa:

```text
SIGNED_MESSAGE = SIGNED_HEADER[32] | FIRMWARE_BYTES[IMAGE_SIZE]
```

Không đưa vào signed message: `SESSION_ID`, `FRAME_COUNTER`, nonce, ciphertext, TAG,
CRC32 của frame vật lý hoặc signature itself. Các giá trị đó thay đổi tùy lượt truyền;
firmware gốc và metadata phát hành phải giữ nguyên.

### 6.2 Quá trình ký điện tử trên Cloud (Sử dụng Tool Python)

Giai đoạn này diễn ra trên máy chủ phát hành (PC/Cloud) trước khi truyền OTA, thực hiện thông qua script `sign_firmware.py`:

1. **Trích xuất thông tin:** Máy chủ biên dịch sinh ra file `firmware.bin`. Tool Python sẽ đọc file này để tự động tính toán kích thước (`IMAGE_SIZE`), phiên bản (`FIRMWARE_VERSION`) và mã kiểm tra tính toàn vẹn (`IMAGE_CRC32`).
2. **Tạo mặt nạ:** Tool lắp ráp các thông số trên thành **Mặt nạ 32 byte (Signed Header)** theo đúng chuẩn thứ tự byte Little-Endian.
3. **Băm dữ liệu:** Đưa Mặt nạ 32 byte cùng toàn bộ nội dung file Firmware qua bộ băm SHA-512, ép lại thành một **Mã Hash 64 byte** thống nhất.
4. **Đóng dấu chữ ký:** Sử dụng khóa bí mật **Private Key (Ed25519ph)** để ký lên Mã Hash, chính thức đẻ ra **Chữ ký điện tử 64 byte (Signature)**.
5. **Gắn vào luồng UART:** Đóng gói Chữ ký 64 byte này vào phần Payload của lệnh khởi tạo `CMD_BEGIN`. Lệnh này sau đó cũng sẽ được thuật toán ChaCha20-Poly1305 mã hóa bọc thép trước khi truyền xuống mạch để chống bị nghe lén.

### 6.3 STM32 verify sau END

1. Sliding window xác nhận đủ block; staging có đủ `IMAGE_SIZE` byte.
2. Kiểm tra CRC32 của toàn staging khớp `IMAGE_CRC32`. Đây là kiểm tra nhanh lỗi ghi/read.
3. Dựng lại signed header 32 byte từ metadata đã nhận.
4. `SHA512_Init`, update signed header trước.
5. Đọc W25Q32 từng block 256 byte, mỗi block gọi `SHA512_Update`.
6. `SHA512_Final` tạo hash 64 byte.
7. Gọi `Ed25519ph_Check(signature, public_key, hash)`.

```text
PASS → firmware đã xác thực; Phase 5 mới được phép backup/install/commit.
FAIL → receiver vào ERROR; Application hiện tại tuyệt đối không bị sửa.
        Phase 5 có thể xóa vùng staging để dọn dữ liệu thất bại.
```

## 7. Cơ chế chìa khóa (Key)

Dựa theo mục 4 của [Key Ownership & Management](./Security.md)

### 7.1 Cơ chế khóa đối xứng (Transport Key)

- Về nguyên lý, ESP32 (người gửi) và STM32 (người nhận) chia sẻ chung một chiếc chìa khóa giống hệt nhau gọi là `transport key` (khóa ChaCha20).
- ESP32 không mã hóa cả một Window cùng lúc. Nó dùng `transport key` để đóng gói và khóa **từng bưu kiện nhỏ** (từng frame/block). Khi STM32 nhận được từng bưu kiện, nó sẽ ngay lập tức dùng `transport key` đó để mở khóa và cất vào kho.

### 7.2 Cơ chế khóa bất đối xứng (Public/Private Key):

- Cloud giữ riêng cho mình 1 `private key` và nó là duy nhất. Đồng thời nó có 1 bản sao thiết kế gọi là `publick key` được chép thẳng vào STM32
- Server sẽ không đóng dấu lên toàn bộ 46KB Firmware, Firmware sẽ đi qua SHA-512 để tạo ra 64-byte mã Hash
- Bằng cách dùng `private key`, nó tạo ra 1 chữ ký điện tử gọi là `Signature` 64-byte

- Khi STM32 nhận đủ Firmware được chép vào `Staging Area` của W25Q32, nó cũng sẽ cho vào SHA-512 để tạo ra mã Hash thực tế
- Khi này nó lấy bản scan `Public key` và `Signature` nhận được đem ra so sánh cấu trúc có trùng 100% so với `private key` tạo ra hay không
   - Đúng thì lấy làm bản chính, sao lưu bản hiện tại vào `Backup Slot`
   - Sai thì khước từ

## 8. Vì sao có SHA-512, CRC32, TAG và signature cùng lúc?

| Cơ chế | Phạm vi | Có key? | Bảo vệ gì? |
|---|---|---|---|
| CRC32 | Một frame/staging | Không | Lỗi ngẫu nhiên đường truyền hoặc Flash |
| Poly1305 TAG | Một crypto frame | ChaCha key | Frame bị sửa/chèn/giả |
| ChaCha20 | Một crypto frame | ChaCha key | Bí mật payload UART |
| SHA-512 | Signed message | Không | Hash streamed 64 byte của metadata + firmware |
| Ed25519ph | Toàn firmware | Private/public key | Xác thực đúng nhà phát hành |

SHA-512 không thay Ed25519ph: hash không chứng minh ai tạo dữ liệu. Ed25519ph ký hash
SHA-512 để STM32 có thể kiểm tra firmware lớn theo block mà không cần giữ cả image trong RAM.

## 9. Key ownership

| Key | Ai giữ | Quy tắc |
|---|---|---|
| Ed25519ph private key | Release server/máy ký | Không vào Git, STM32 hoặc ESP32 |
| Ed25519 public key | Bootloader STM32 | Có thể công khai; chỉ đổi qua quy trình nạp Bootloader tin cậy |
| ChaCha20-Poly1305 key | STM32 và ESP32 | Key riêng được provision, không truyền qua OTA/UART |

## 10. Trạng thái triển khai

Tài liệu này chốt target contract cho Phase 4.1. `Shared/crypto_config.h` đã được tạo
nhưng còn phải compile-check và đồng bộ đủ frame-size/counter macro. Tiếp theo cần cập
nhật `Shared/ota_commands.h` để parser/Python/ESP32 dùng cùng wire format; phần code đó
chỉ thực hiện sau khi review contract hoàn tất.
