# Threat Model

> **Phạm vi (Scope):** Liệt kê threat in-scope/out-of-scope và mitigation của bản P2P.

> **Tác dụng:** Giúp xác định các mối đe dọa cần xử lý và biện pháp bảo vệ tương ứng.


## In-scope

- sửa firmware/header/signature;
- replay frame;
- reuse session/counter;
- wrong target/version;
- corrupt/truncated FULL/DELTA;
- wrong Delta base;
- malformed patch;
- reset/mất điện giữa staging/backup/install/rollback;
- packet loss/duplicate/delay;
- reset ESP32/AS32/STM32.

## Mitigation

| Threat | Control |
|---|---|
| Firmware giả | Ed25519ph verify |
| Corruption | CRC32 + SHA-512 |
| Replay | random session + monotonic frame counter + W25 Session Journal chống mở lại ID qua reset |
| Delta sai base | exact base version/size/hash |
| Reset giữa transaction | metadata A/B + persistent state |
| Trial fail | backup + rollback |
| Gateway compromise | gateway không có install authority |

## Out-of-scope mặc định

- invasive physical attacks;
- compromised signing environment;
- secure bootloader self-update;
- secure element;
- RF jamming tuyệt đối.
