# ESP32 Gateway Controller

> **Phạm vi (Scope):** Chốt vai trò ESP32 trong scope P2P.

> **Tác dụng:** Mô tả cách ESP32 nhận artifact, điều phối truyền và quản lý retry/resume.

> **Bên thực hiện:** Firmware trên ESP32 Gateway; phía STM32 Application chỉ là
> peer nhận/trả lời giao thức, còn Bootloader không chạy chức năng gateway.


ESP32 chịu:

- nhận/download artifact;
- cache FULL/DELTA;
- fragment;
- secure frame TX/RX;
- ACK/NACK;
- retry/backoff;
- query resume;
- progress/status bridge.

ESP32 không chịu:

- private firmware signing key;
- target authenticity decision;
- Internal Flash install;
- Trial confirm thay STM32/Application.

```text
Release
 -> ESP32 Gateway
 -> AS32 #1
 ))) P2P )))
 -> AS32 #2
 -> STM32 Application Update Agent
```

ESP32 nối AS32 #1 bằng UART `9600 8N1`; AS32 #2 nối USART1 STM32 cũng bằng
`9600 8N1`. Python/Cloud đưa artifact tới ESP32 qua Wi-Fi/network. USB-UART
Python–ESP32 ở `115200 8N1` chỉ là ingress cục bộ tùy chọn và phải có buffer
cùng backpressure khi bridge sang AS32 chậm hơn. ESP32 tạo session ID ngẫu nhiên,
quản lý/persist TX counter và dùng STATUS của STM32/W25Q32 làm source of truth
khi resume.

Sau BOOT/reset, gateway query lại STATUS để hiển thị riêng `STAGING_READY`,
`INSTALLED` và `CONFIRMED`; ACK của BOOT chỉ có nghĩa request reset đã được chấp
nhận, không phải bằng chứng candidate đã được confirm.

## Module production mẫu

- `components/ota_sender/ota_sender.c`: tạo secure frame, stop-and-wait,
  retry hữu hạn, resume theo `next_missing_offset`, kiểm STATUS persistent rồi
  mới gửi BOOT.
- `main/ota_gateway.c`: adapter thật nối callback sender với UART/AS32, tìm SYNC
  và đọc đúng frame theo LEN.
- `main/app_main.c`: harness bring-up/đo Phase 4; không chứa key hoặc artifact
  release giả. Ingress Wi-Fi/PC cung cấp manifest/data/ID cho
  `OtaGateway_SendArtifact()` sau khi cache bền vững.

Sau reboot ESP32 phải tạo `session_id` mới, khôi phục cùng artifact,
`operation_id` và `transfer_id` từ cache. BEGIN/STATUS sẽ lấy tiến độ committed
từ W25Q32; counter của session cũ tuyệt đối không được tái sử dụng.
