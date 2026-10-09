# CMSIS/CM3 — Giới thiệu các file

Thư mục này chứa các file thuộc **CMSIS (Cortex Microcontroller Software Interface Standard)** dành cho lõi ARM Cortex-M3, dùng trong dự án STM32F10x.

## 📁 CoreSupport

### `core_cm3.h`
Header chuẩn của ARM cung cấp các định nghĩa, macro và hàm inline để truy cập trực tiếp các thành phần lõi Cortex-M3: NVIC (bộ điều khiển ngắt), SysTick, SCB (System Control Block), FPU (nếu có)... Đây là lớp trừu tượng giúp code truy cập phần cứng lõi CPU mà không cần viết assembly.

### `core_cm3.c`
File nguồn hiện thực một số hàm hỗ trợ được khai báo trong `core_cm3.h`, ví dụ các hàm liên quan đến cấu hình SysTick, đọc/ghi thanh ghi đặc biệt của lõi.

## 📁 DeviceSupport/ST

### `stm32f10x.h`
Header đặc tả cho riêng dòng chip **STM32F10x**, định nghĩa địa chỉ các thanh ghi ngoại vi (GPIO, USART, RCC, TIM, ADC...), cấu trúc struct ánh xạ thanh ghi, các bit định nghĩa, và enum IRQ. Đây là file quan trọng nhất để lập trình trực tiếp thanh ghi (register-level) trên STM32F10x.

### `system_stm32f10x.c`
File nguồn cấu hình hệ thống lúc khởi động: thiết lập xung clock (HSE/HSI, PLL), cấu hình bus AHB/APB1/APB2, cập nhật biến `SystemCoreClock`. Hàm `SystemInit()` trong file này được gọi tự động trước `main()`.

### `system_stm32f10x.h`
Header khai báo các hàm và biến toàn cục dùng trong `system_stm32f10x.c`, như `SystemInit()`, `SystemCoreClockUpdate()`, và biến `SystemCoreClock`.

---

**Tóm lại:** nhóm `CoreSupport` lo phần chuẩn hoá lõi ARM Cortex-M3 (dùng chung cho mọi chip Cortex-M3), còn nhóm `DeviceSupport/ST` lo phần đặc thù cho chip STM32F10x (thanh ghi ngoại vi + cấu hình clock khởi động).