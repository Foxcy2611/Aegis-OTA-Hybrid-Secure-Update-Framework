# AS32 Hardware Profile

> **Tác dụng:** Ghi lại đúng phần cứng, wiring và cấu hình read-back của hai
> module AS32 để phép thử P2P có thể tái lập.

> **Phạm vi:** AS32 #1 nối ESP32 Gateway và AS32 #2 nối STM32F103 trong Phase 4.

## Trạng thái

PENDING HARDWARE: source và biểu mẫu đã sẵn sàng; các ô “chưa đo” chỉ được điền
sau khi đọc từ đúng module thật. Không dùng giá trị mặc định in trên datasheet làm
bằng chứng read-back.

## 1. Inventory phải ghi trước khi cấp nguồn

| Thuộc tính | AS32 #1 — Gateway | AS32 #2 — Node | Vì sao cần ghi |
|---|---|---|---|
| Model/marking chính xác | Chưa đo | Chưa đo | Xác định đúng command set, band và pinout. |
| Serial/nhãn tự đặt | Chưa đo | Chưa đo | Không tráo hai module giữa các lần test. |
| Band ghi trên module | Chưa đo | Chưa đo | Chọn đúng antenna và kênh hợp pháp. |
| Logic UART | Chưa đo | Chưa đo | Không đưa mức áp sai vào ESP32/STM32. |
| Nguồn và dòng peak hỗ trợ | Chưa đo | Chưa đo | Tránh reset/brownout lúc AS32 phát. |
| Antenna/connector | Chưa đo | Chưa đo | Không phát khi chưa gắn đúng antenna. |
| Hardware/version response | Chưa đo | Chưa đo | Chứng minh đúng firmware module đã test. |

## 2. Wiring đã khóa trong source

### ESP32 ↔ AS32 #1

| AS32 signal | ESP32 mặc định | Hướng | Tác dụng |
|---|---:|---|---|
| RX | GPIO17 / UART2 TX | ESP32 → AS32 | Gửi command cấu hình và packet P2P. |
| TX | GPIO16 / UART2 RX | AS32 → ESP32 | Nhận response cấu hình và packet P2P. |
| MD0 | GPIO25 | ESP32 → AS32 | Cùng MD1 chọn Normal/Configuration. |
| MD1 | GPIO26 | ESP32 → AS32 | Cùng MD0 chọn Normal/Configuration. |
| AUX | GPIO27 | AS32 → ESP32 | Ready/busy; mọi lần chờ đều có timeout. |
| GND | GND chung | — | Mốc điện áp chung bắt buộc. |

Các GPIO này là baseline cho ESP32 classic, khai báo trong
esp32_gateway/sdkconfig.defaults. Nếu board thật dùng chân khác, đổi bằng
idf.py menuconfig, rồi ghi đúng pin đã dùng vào evidence.

### STM32F103 ↔ AS32 #2

| AS32 signal | STM32 | Hướng | Tác dụng |
|---|---|---|---|
| RX | PA9 / USART1_TX | STM32 → AS32 | Gửi command và packet P2P. |
| TX | PA10 / USART1_RX | AS32 → STM32 | Nhận response và packet P2P. |
| MD0 | PB0 | STM32 → AS32 | Chọn mode. |
| MD1 | PB1 | STM32 → AS32 | Chọn mode. |
| AUX | PB10 | AS32 → STM32 | Ready/busy. |
| GND | GND chung | — | Mốc điện áp chung bắt buộc. |

Contract STM32 nằm tại shared/pinout_hw.h. Debug UART tùy chọn dùng USART2
PA2/PA3 ở 115200, không dùng chung USART1 của AS32.

## 3. Mode và command bring-up

| Mục | Byte/mức | Tác dụng |
|---|---|---|
| Normal | MD0=0, MD1=0 | UART transparent ↔ LoRa P2P. |
| Configuration | MD0=1, MD1=1 | Đọc/ghi config, đọc version, reset module. |
| Read config | C1 C1 C1 | Module trả C0 ADDH ADDL SPEED CHAN OPTION. |
| Write persistent | C0 + 5 config byte | Ghi config giữ qua reset; chỉ chạy có chủ đích. |
| Write volatile | C2 + 5 config byte | Ghi config tạm thời. |
| Read version | C3 C3 C3 | Lấy response version để định danh module. |
| Reset | C4 C4 C4 | Reset module; caller vẫn phải chờ AUX có timeout. |

Bring-up lần đầu chỉ đọc config/version. Không tự động ghi persistent config khi
boot vì một wiring/profile sai có thể làm mất liên lạc với module.

## 4. Profile candidate và read-back thật

Baseline source khóa UART host ở 9600 8N1. Bộ byte dưới đây chỉ là candidate
ban đầu, phải so với read-back của cả hai module:

~~~text
ADDH ADDL SPEED CHAN OPTION
00   00   1A    17   40
~~~

| Field | AS32 #1 read-back | AS32 #2 read-back | Tác dụng |
|---|---|---|---|
| ADDH | Chưa đo | Chưa đo | Byte cao địa chỉ module. |
| ADDL | Chưa đo | Chưa đo | Byte thấp địa chỉ module. |
| SPEED | Chưa đo | Chưa đo | Parity, UART baud và air data rate. |
| CHAN | Chưa đo | Chưa đo | Kênh RF; hai đầu phải cùng profile. |
| OPTION | Chưa đo | Chưa đo | Mode truyền, IO, wake time, FEC/công suất tùy model. |

Không ghi C0 cho tới khi đã đối chiếu đúng model manual, band, giới hạn công
suất và quy định RF tại nơi thử nghiệm.

## 5. Checklist nguồn và wiring

- [ ] Antenna đúng band đã gắn trước khi phát.
- [ ] Nguồn đúng điện áp và đủ dòng peak theo module thật.
- [ ] TX/RX nối chéo; MD0/MD1/AUX đúng chân.
- [ ] ESP32, AS32, STM32 và debugger có GND chung khi liên kết.
- [ ] Không có chân input-only được dùng làm TX/MD.
- [ ] AUX lên high ổn định; AUX stuck-low trả timeout, không treo CPU.
- [ ] Hai response config/version được lưu vào log evidence.
- [ ] Hai config read-back giống profile cần test.

## 6. Bảng số liệu nghiệm thu

| Ngày/địa điểm | Khoảng cách/môi trường | Payload | Số packet | PASS | Mất | RTT min/avg/max | Throughput | Ghi chú |
|---|---|---:|---:|---:|---:|---|---|---|
| Chưa đo | Chưa đo | 0/1/255/256 byte | Chưa đo | Chưa đo | Chưa đo | Chưa đo | Chưa đo | PENDING HARDWARE |

RSSI/SNR chỉ ghi nếu đúng model/firmware cung cấp được số đo đó; không suy đoán từ
việc packet nhận thành công.
