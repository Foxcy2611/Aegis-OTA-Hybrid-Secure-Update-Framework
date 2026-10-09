# Portfolio Summary

> **Phạm vi (Scope):** Tóm tắt 1 trang đúng scope P2P.

> **Tác dụng:** Cung cấp bản giới thiệu cô đọng để trình bày dự án nhanh và đúng trọng tâm.


| Kỹ thuật | Nội dung |
|---|---|
| MCU | STM32F103C8T6 |
| Flash/RAM | 64 KB / 20 KB |
| Bootloader/App | 16 KB / max 46 KB |
| Gateway | ESP32 |
| Radio | 2 x AS32 LoRa P2P |
| Storage | W25Q32 4 MB |
| Firmware Auth | Ed25519ph + SHA-512 |
| Secure Transport | ChaCha20-Poly1305 |
| OTA | FULL + DELTA |
| Reliability | ACK/NACK + retry + resume |
| Recovery | Trial + IWDG + rollback + recovery |
| Validation | fault/security negative |

```text
Release/CI
 -> ESP32
 -> AS32 #1
 ))) P2P )))
 -> AS32 #2
 -> STM32 Application Update Agent
 -> W25Q32 receive/reconstruct/Backup
 -> UPDATE_READY + reset
 -> STM32 Bootloader verify/install/trial/rollback
```
