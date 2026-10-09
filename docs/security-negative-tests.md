# Security Negative Tests

> **Phạm vi (Scope):** Liệt kê negative tests bắt buộc để chứng minh crypto/policy fail-closed.

> **Tác dụng:** Hướng dẫn kiểm tra hệ thống từ chối dữ liệu giả, sửa đổi, replay hoặc sai policy.


## Secure frame

- ciphertext bị sửa;
- AAD bị sửa;
- TAG sai;
- replay counter;
- session sai;
- replay BEGIN/STATUS của session đã commit, kể cả sau reset Application;
- Session Journal corrupt/full phải fail-closed;
- length overflow.

## Firmware target

- SignedHeader bị sửa;
- image bị sửa;
- signature sai;
- wrong public key fixture;
- wrong target/version;
- oversized image.

## Delta

- wrong base version/hash;
- patch hash sai;
- manifest signature sai;
- truncated patch;
- malformed opcode;
- reconstructed target signature sai.

Expected: **không install và không boot image chưa target-verify**.
