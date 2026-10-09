# Phase 3 Crypto Regression

> **Tác dụng:** Build đúng source production của Phase 3 trên host, kiểm byte
> contract, negative crypto cases và luồng Python signer sang C verifier.

## Phạm vi kiểm tra

- nonce 12 byte và AAD 5 byte serialize đúng little-endian;
- ChaCha20-Poly1305 khớp golden vector tạo độc lập bằng Python `cryptography`;
- sửa ciphertext, AAD, TAG hoặc nonce đều bị từ chối;
- SignedHeader serialize đúng 32 byte;
- streaming CRC32 + SHA-512 + Ed25519ph verify hợp lệ;
- sửa header, image, signature hoặc public key đều bị từ chối;
- `tools/sign_firmware.py` tạo output deterministic;
- artifact do Python tạo được production C verifier chấp nhận;
- image rỗng/quá 46 KB và private-key permission không an toàn bị từ chối.

Test seed chỉ được tạo trong temporary directory khi test chạy. Thư mục này
không chứa private key thật và không ghi fixture bí mật vào repository.

## Lệnh chạy

```bash
cmake -S test/Test_to_Phase/Phase3_Crypto \
  -B build/phase3-host \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build/phase3-host
ctest --test-dir build/phase3-host --output-on-failure
```

Muốn xem số check C:

```bash
./build/phase3-host/phase3_crypto_tests
```

## Kết quả mong đợi

```text
PASS crypto_contract_serialization
PASS secure_transport_known_answers
PASS firmware_header_serialization
PASS firmware_authentication
Phase 3 C tests: 34 checks, 0 failures

100% tests passed, 0 tests failed out of 2
```

Đây là host acceptance. Phase 3 không cần AS32/W25Q32 để PASS; hardware
acceptance của Phase 1/2 vẫn được theo dõi riêng.
