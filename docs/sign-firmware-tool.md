# tools/sign_firmware.py

> **Phạm vi:** Contract của signer host-side cho FULL artifact.

> **Tác dụng:** Hướng dẫn công cụ Python tạo FULL artifact đúng header, hash và chữ ký.

> **Bên thực hiện:** Máy Release/CI chạy tool Python bằng private signing key.
> ESP32 và STM32 chỉ nhận artifact đầu ra; private key không được đưa lên thiết bị.


Input:

- target BIN;
- firmware version;
- private-key path.

Output `.aegisfw`:

```text
SignedHeader[32] || target_firmware[image_size] || Ed25519ph_signature[64]
```

Không có trailing binary manifest.

Requirements:

- private key không có default trong repo;
- Python/C serialize byte-exact;
- artifact Python tạo phải pass C verifier;
- deterministic ở field không ngẫu nhiên.
- reject empty image, image lớn hơn 46 KB, version/range sai và key permission
  không an toàn theo tool policy;
- không in private key/seed ra log.

## Cách dùng

Chạy từ root repository. Tool dùng đúng source Monocypher 4.0.3 đã pin để tạo
binding host tạm; máy Release/CI cần có Python 3 và host C compiler `cc`.

```bash
chmod 600 /secure/path/release_seed.hex

python3 tools/sign_firmware.py \
  --bin build/release/application/aegis_application.bin \
  --version 0x00000002 \
  --private-key /secure/path/release_seed.hex \
  --output dist/aegis_application_v2.aegisfw \
  --public-key-output dist/release_public_key.bin
```

Private key nhận một trong hai encoding:

- raw seed đúng 32 bytes;
- chuỗi hex đúng 64 ký tự, có thể có newline cuối file.

Trên POSIX, tool từ chối key mà group/other còn quyền truy cập; dùng
`chmod 600`. Không đặt private key thật trong repository hoặc trên phân vùng
chia sẻ có permission không đáng tin cậy.

Tool ghi output theo kiểu temporary-file rồi atomic replace, tự verify chữ ký
trước khi publish và chỉ chấp nhận FULL output có đuôi `.aegisfw`.

## Regression

```bash
cmake -S test/Test_to_Phase/Phase3_Crypto \
  -B build/phase3-host -G Ninja
cmake --build build/phase3-host
ctest --test-dir build/phase3-host --output-on-failure
```

Test tạo key tạm, ký hai lần để kiểm deterministic, rồi đưa artifact cho C
`FirmwareAuth_Verify()`. Test cũng sửa image và kiểm C verifier fail-closed.
