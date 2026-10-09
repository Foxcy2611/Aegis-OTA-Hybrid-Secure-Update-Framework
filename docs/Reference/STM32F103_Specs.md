# Thông số kỹ thuật STM32F103

> **Phạm vi:** Thông tin tham khảo về họ STM32F103; contract Aegis chỉ dùng đúng
> STM32F103C8T6 64 KB Flash và 20 KB SRAM.

> **Tác dụng:** Giúp tra cứu giới hạn lõi, bộ nhớ và ngoại vi khi thiết kế driver,
> nhưng không được dùng để mở rộng memory map đã khóa.

> **Non-contract reference:** Aegis-OTA luôn target đúng **STM32F103C8T6, 64 KB Flash và 20 KB SRAM**. Không được dùng phần Flash dư trên chip clone/lỗi nhãn như một phần của memory contract.

STM32F103 có nhiều dòng Low/Medium/High/XL-density với dung lượng Flash/RAM khác nhau. Bảng dưới chỉ là tham chiếu phần cứng, kèm so sánh các dòng khác.

---

## 1. Tổng quan lõi

| Thông số | Giá trị |
|---|---|
| Lõi | ARM Cortex-M3 |
| Kiến trúc | ARMv7-M, 32-bit |
| Xung nhịp tối đa | 72 MHz |
| FPU | Không có |
| DMIPS | ~1.25 DMIPS/MHz (~90 DMIPS @72MHz) |
| Bộ nhớ đệm | Không có cache, có prefetch buffer |
| Điện áp hoạt động | 2.0V – 3.6V |
| Nhiệt độ hoạt động | -40°C đến +85°C (hoặc 105°C bản mở rộng) |

---

## 2. Bộ nhớ (Memory)

### STM32F103C8T6 (Medium-density, phổ biến nhất)

| Loại | Dung lượng |
|---|---|
| Flash | 64 KB theo contract Aegis-OTA; một số clone/lỗi nhãn có thể lộ 128 KB nhưng không được sử dụng |
| SRAM | 20 KB |
| Địa chỉ Flash | 0x0800 0000 – 0x0800 FFFF |
| Địa chỉ SRAM | 0x2000 0000 – 0x2000 4FFF |
| EEPROM | Không có (giả lập bằng Flash) |
| Bootloader (System Memory) | 2 KB, chứa ISP UART/USB/CAN |

### Bảng so sánh các dòng STM32F103 theo Density

| Dòng | Flash | RAM | Ví dụ chip |
|---|---|---|---|
| Low-density | 16–32 KB | 6–10 KB | STM32F103x4/x6 |
| `Medium-density` | 64–128 KB | 20 KB | STM32F103C8/CB |
| High-density | 256–512 KB | 48–64 KB | STM32F103RE/VE |
| XL-density | 768KB–1MB | 96 KB | STM32F103ZG |

---

## 3. Stack & Heap (mặc định theo file startup của SPL/HAL)

| Vùng | Kích thước mặc định | Ghi chú |
|---|---|---|
| Stack (`_Min_Stack_Size`) | 0x400 (1 KB) | Có thể chỉnh trong `startup_stm32f10x_xx.s` hoặc linker script |
| Heap (`_Min_Heap_Size`) | 0x200 (512 B) | Dùng cho `malloc()`, thường không cần nhiều nếu code không dùng dynamic allocation |
| Vị trí | Nằm trong vùng SRAM (0x2000 0000+) | Stack thường đặt cuối RAM, Heap ngay sau vùng `.bss`/`.data` |
| Kiểu Stack | Full Descending | Đặc trưng của ARM Cortex-M |

> Với STM32F103C8 (20KB RAM), nếu project dùng nhiều buffer tĩnh (mảng lớn, RTOS, DSP...) cần cân nhắc kỹ vì tổng `.data + .bss + heap + stack` phải nằm gọn trong 20KB.

Cách chỉnh trong `startup_stm32f10x_md.s` (Keil/GCC):
```asm
Stack_Size      EQU     0x00000400   ; 1KB
Heap_Size       EQU     0x00000200   ; 512B
```

---

## 4. Ngoại vi (Peripherals) — STM32F103C8T6

| Ngoại vi | Số lượng | Ghi chú |
|---|---|---|
| GPIO | 37 pin (Port A, B, C, D, E — thường dùng A,B,C) | 5V-tolerant hầu hết chân |
| Timer (General Purpose) | TIM2, TIM3, TIM4 (16-bit) | PWM, Input Capture, Output Compare |
| Timer (Advanced) | TIM1 (16-bit) | Motor control, dead-time |
| USART/UART | 3 (USART1, USART2, USART3) | Có hỗ trợ IrDA, LIN, Smartcard |
| SPI | 2 (SPI1, SPI2) | Tối đa 18Mbit/s |
| I2C | 2 (I2C1, I2C2) | Chuẩn/Fast mode |
| ADC | 2× ADC 12-bit | 16 kênh, tốc độ 1µs/kênh |
| DAC | Không có (chip High-density trở lên mới có) | |
| USB | 1× USB 2.0 Full-Speed (Device) | |
| CAN | 1× bxCAN 2.0B | |
| DMA | 1 controller, 7 kênh | |
| RTC | Có, chạy bằng LSE 32.768kHz hoặc LSI | |
| Watchdog | IWDG (Independent) + WWDG (Window) | |
| SysTick | 24-bit, tích hợp trong Cortex-M3 | |

---

## 5. Clock System

| Nguồn | Tần số |
|---|---|
| HSI (nội) | 8 MHz (±1%) |
| HSE (thạch anh ngoài) | 4–16 MHz (thường dùng 8MHz) |
| PLL | Nhân từ HSI/2 hoặc HSE, max 72MHz |
| LSI (nội, cho IWDG/RTC) | ~40 kHz |
| LSE (thạch anh ngoài, cho RTC) | 32.768 kHz |
| AHB max | 72 MHz |
| APB1 max | 36 MHz |
| APB2 max | 72 MHz |

Công thức PLL phổ biến (thạch anh 8MHz):
```
SYSCLK = HSE × PLL_MUL = 8MHz × 9 = 72MHz
```

---

## 6. Đóng gói (Package) & số chân theo dòng

| Chip | Package | Số chân | Flash | RAM |
|---|---|---|---|---|
| STM32F103C8T6 | LQFP48 | 48 | 64KB | 20KB |
| STM32F103RBT6 | LQFP64 | 64 | 128KB | 20KB |
| STM32F103RET6 | LQFP64 | 64 | 512KB | 64KB |
| STM32F103VET6 | LQFP100 | 100 | 512KB | 64KB |
| STM32F103ZET6 | LQFP144 | 144 | 512KB | 64KB |

---

## 7. Chế độ tiết kiệm năng lượng (Power Modes)

| Mode | Mô tả | Dòng tiêu thụ |
|---|---|---|
| Run | Hoạt động bình thường | ~36mA @72MHz |
| Sleep | CPU dừng, ngoại vi vẫn chạy | Thấp hơn Run |
| Stop | Tắt hầu hết clock, giữ nội dung RAM | ~2.4µA (regulator on) |
| Standby | Tắt gần hết, chỉ giữ backup domain | ~2µA |

---

## 8. Bảng vector ngắt (NVIC) — tóm tắt

- STM32F103 hỗ trợ tối đa **68 kênh ngắt ngoại vi (IRQ)** tuỳ dòng (medium-density ~60 kênh).
- 16 mức ưu tiên (4-bit priority, có thể chia group/sub-priority).
- Ngắt hệ thống cố định: Reset, NMI, HardFault, MemManage, BusFault, UsageFault, SVCall, DebugMon, PendSV, SysTick.

---

## 9. Ghi chú thực dụng khi lập trình

- **Vector table offset**: mặc định tại `0x0800 0000`, nếu dùng bootloader cần set lại `VECT_TAB_OFFSET` trong `system_stm32f10x.c`.
- **Debug interface**: SWD (2 dây: SWDIO, SWCLK) hoặc JTAG (5 dây) — ST-Link dùng SWD là chủ yếu.
- **Boot pins (BOOT0, BOOT1)**:
  | BOOT0 | BOOT1 | Chế độ boot |
  |---|---|---|
  | 0 | X | Main Flash |
  | 1 | 0 | System Memory (bootloader ISP) |
  | 1 | 1 | Embedded SRAM |

---

*Nguồn tổng hợp: STM32F103xx Reference Manual (RM0008), Datasheet STM32F103C8/CB, Cortex-M3 Technical Reference Manual.*
