# Radio Profile and Measurement

> **Phạm vi (Scope):** Quy định profile/test measurement cho P2P thay vì hard-code theo cảm tính.

> **Tác dụng:** Hướng dẫn khóa cấu hình radio và thu thập số liệu P2P có thể tái lập.


Cần lưu:

- AS32 config bytes/profile;
- UART ESP32–AS32 #1: baseline 9600 8N1;
- UART AS32 #2–STM32 USART1: baseline 9600 8N1;
- USB-UART Python–ESP32 nếu dùng: 115200 8N1, đo riêng và không tính là air link;
- RF frequency/channel profile;
- packet payload size;
- retry timeout;
- RSSI/SNR nếu module expose;
- packet loss;
- retry count;
- total time;
- throughput;
- khoảng cách/môi trường.

Profile read-back cụ thể của hai module được điền tại
[LoRa/AS32_Hardware_Profile.md](LoRa/AS32_Hardware_Profile.md). Không ghi
“default theo datasheet” thay cho response đọc từ module.

## Phép đo Phase 4

| Metric | Cách tính/ghi | Tác dụng |
|---|---|---|
| Packet count | Tổng request ESP32 đã phát | Mẫu số cho success/loss. |
| Success count | Echo đúng counter, length, payload và CRC | Chỉ đếm dữ liệu byte-for-byte đúng. |
| Packet loss | (sent - success) / sent | Đánh giá link thô, chưa gồm retry Phase 5. |
| RTT | Thời gian từ trước send đến echo hợp lệ | Là cơ sở chọn timeout/retry sau này. |
| Payload throughput | Tổng payload thành công / tổng RTT | So sánh các profile bằng dữ liệu hữu ích. |
| MTU candidate | Payload lớn nhất pass ổn định | Phase 4 thử 0, 1, 255, 256 và từ chối 257. |
| RSSI/SNR | Chỉ ghi nếu module thật expose | Không suy đoán nếu model không hỗ trợ. |

Mỗi phép đo phải ghi tối thiểu:

| Field | Ví dụ dạng dữ liệu | Tác dụng |
|---|---|---|
| Module/profile | Hai response config/version | Có thể tái lập cấu hình radio. |
| Nguồn/antenna | Điện áp, nguồn cấp, loại antenna | Tách lỗi RF khỏi brownout/wiring. |
| Môi trường | Trong nhà/ngoài trời, vật cản, khoảng cách | Đặt số liệu vào đúng điều kiện vật lý. |
| Dataset | Payload, số packet, pattern binary | Biết chính xác workload. |
| Result | Success/loss, RTT min/avg/max, throughput | Dùng để chốt timeout/fragment sau này. |

Không dùng một packet ping thành công để chốt profile. Sau boundary test phải chạy
burst, reset một đầu, AUX stuck/disconnect và soak đủ dài để phát hiện
overflow/race.

FULL và DELTA phải đo cùng dataset để so:

```text
artifact bytes
radio payload bytes
packet count
retry count
total time
throughput
success rate
```
