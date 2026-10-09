# Upstream record — Monocypher

## Mục đích

Thư mục này dùng để vendor nguyên trạng thư viện crypto Monocypher cho Phase 3.
Khi tới Phase 3, code dự án chỉ gọi thư viện qua `secure_transport.*` và
`firmware_auth.*`; không sửa hoặc sao chép thuật toán ChaCha20-Poly1305, SHA-512
hay Ed25519ph vào source của dự án.

## Nguồn đã chốt

| Trường | Giá trị |
|---|---|
| Upstream project | Monocypher |
| Maintainer repository | https://github.com/LoupVaillant/Monocypher |
| Release được pin | 4.0.3 |
| Official archive | https://monocypher.org/download/monocypher-4.0.3.tar.gz |
| Official SHA-512 file | https://monocypher.org/download/monocypher-4.0.3.tar.gz.sha512 |
| License | BSD-2-Clause OR CC0-1.0; giữ nguyên `LICENCE.md` từ upstream |
| Local modifications | None — source vendor phải giữ nguyên |

Không dùng nhánh `master` của GitHub để build firmware: upstream ghi rõ đó là bản đang
phát triển, chưa phát hành. Không dùng Monocypher 4.0.2 hoặc cũ hơn; release 4.0.3 có
bản vá timing leak cho EdDSA/Ed25519.

## Cách import khi bắt đầu 3

1. Tải archive release ở URL trên, không tải ZIP tự sinh từ nhánh `master`.
2. Đối chiếu kết quả `sha512sum` của archive với file checksum chính thức trước khi giải nén.
3. Copy nguyên trạng các file sau vào đúng thư mục này:

```text
monocypher.c
monocypher.h
monocypher-ed25519.c
monocypher-ed25519.h
LICENCE.md
```

4. Ghi checksum SHA-512 đã đối chiếu thực tế vào bảng dưới.
5. Chạy upstream test suite ít nhất một lần trên host trước khi thêm source vào Makefile.
6. Chỉ thêm include path/source file vào build; không format, không đổi tên, không sửa
   source upstream. Nếu phải vá upstream, ghi patch và lý do ở mục Local patches.

## Receipt của lần kiểm chứng feasibility

| Trường | Giá trị sau khi import |
|---|---|
| Archive SHA-512 đã kiểm | `40904ada5c7ee4f7741733e38b69a30a4b0561cbffba5ffe7c2dce16136d540251ec0d9056ff606510d3b5b708fb8a40db7e0870d4a0b2dc17ba2bfb880f8965` |
| Ngày import/kiểm tra | 2026-09-19 |
| Người import | Nguyễn Ngọc Chiến; Phase 3.1 đã đối chiếu lại byte-for-byte với archive chính thức |
| Upstream test suite | PASS — `make tis-ci` và `make test`, gồm IETF AEAD, SHA-512 và Ed25519ph |
| Files đã dùng để kiểm chứng | `monocypher.c`, `monocypher.h`, `monocypher-ed25519.c`, `monocypher-ed25519.h`, `LICENCE.md` |

SHA-512 của từng source đã import:

| File | SHA-512 |
|---|---|
| `monocypher.c` | `9618a9261b9a2295a3729efbc3f1c1146abbe3f1c627766262c9099879495652c74582a3ffe355f274707ea574131852d5b69f1a14a6a2be77b165251fcc87a5` |
| `monocypher.h` | `09a2d4015d4b8ef319a11beeed5470b49455eeec5056b96720da9ba3d6108dae1566358b56f87001de2f493f248ae1014fe402000c3e4eea7a80a639f2c8f453` |
| `monocypher-ed25519.c` | `6f37e57f02d4aa8eb475f6fdb1d127d066a8dd86b0868b31c58e5682ca685c6d1e2e3c9e4dc58cad655518d3ce97c298321b9f49f7608d0ee7ef2b2fbc065fab` |
| `monocypher-ed25519.h` | `18daec3d437ae86bff6726aed1b3170a89342f27b863ca38f44cce23d39ad64945900149f5c73f481f7b7e0c5320955e4724b1b680b091b94bdd392f8c66d8f4` |

Archive tải từ URL chính thức có SHA-512 trùng với file checksum do upstream công bố.
Các source Monocypher đã được dùng cho feasibility rồi xóa khỏi baseline Phase 0.
Tới Phase 3 phải import lại đúng bốn file code từ archive 4.0.3 và kiểm checksum
trước khi build. `LICENCE.md`, `VERSION` và file này được giữ làm provenance.

## Local patches

Không có. Mọi thay đổi source vendor trong tương lai phải được ghi ở đây cùng lý do,
file bị sửa và cách test lại.
