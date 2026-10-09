# Trust Model

> **Phạm vi (Scope):** Xác định ai được ký firmware, ai chỉ vận chuyển và ai quyết định install/boot.

> **Tác dụng:** Làm rõ quyền hạn và dữ liệu được tin cậy tại từng thành phần.


```mermaid
flowchart TB
    CI["Release / CI<br/>PRIVATE SIGNING KEY"]
    ESP["ESP32<br/>Transport only"]
    A1["AS32 #1"]
    A2["AS32 #2"]
    AG["STM32 Application Update Agent<br/>Transport PSK + preliminary verify"]
    W["W25Q32<br/>Untrusted workspace"]
    STM["STM32 Bootloader<br/>PUBLIC VERIFY KEY + install authority"]
    APP["Application"]

    CI -->|"Signed Artifact"| ESP
    ESP --> A1
    A1 -->|"LoRa P2P"| A2
    A2 --> AG
    AG --> W
    AG -->|UPDATE_READY + reset| STM
    STM -->|Independent read/verify| W
    STM -->|"Verified target only"| APP
```

- Private signing key: Release/CI only.
- Public verify key: Bootloader và Application preliminary verifier; quyết định
  cuối vẫn thuộc Bootloader.
- Per-device transport PSK: ESP32 và STM32 Application, chỉ bảo vệ transport.
- ESP32/AS32 không có firmware install authority.
- W25Q32 không phải trust anchor; mọi manifest/image phải được kiểm trước khi dùng.
