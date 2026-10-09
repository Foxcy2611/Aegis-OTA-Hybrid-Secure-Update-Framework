# Quy tắc viết Startup Code cho STM32F103C8T6

> **Phạm vi:** Startup Assembly/C cho Cortex-M3 từ vector table, Reset Handler
> đến lúc chuyển quyền cho `main()`.

> **Tác dụng:** Hướng dẫn tự viết và kiểm tra startup code STM32F103C8T6
> bare-metal, gồm cả trường hợp Application nằm sau Bootloader.

## 1. Startup code là gì?

Startup code là đoạn code đầu tiên chạy sau khi Cortex-M3 thoát reset.

Nhiệm vụ chính:

1. Cung cấp vector table.
2. Cung cấp `Reset_Handler`.
3. Copy `.data` từ Flash sang RAM.
4. Clear `.bss`.
5. Thiết lập môi trường runtime tối thiểu.
6. Gọi `SystemInit()` nếu sử dụng CMSIS convention.
7. Gọi C/C++ runtime init nếu cần.
8. Gọi `main()`.
9. Cung cấp default interrupt handlers.

Mental model:

```text
Power-on / Reset
       ↓
CPU đọc vector[0]
       ↓
MSP = initial stack pointer
       ↓
CPU đọc vector[1]
       ↓
PC = Reset_Handler
       ↓
copy .data
       ↓
clear .bss
       ↓
SystemInit()
       ↓
C/C++ runtime init
       ↓
main()
```

---

# 2. Cortex-M3 reset sequence

STM32F103 dùng Cortex-M3.

Tại reset, CPU đọc:

```text
0x08000000 : initial Main Stack Pointer
0x08000004 : address of Reset_Handler
```

Nếu vector table:

```asm
.word _estack
.word Reset_Handler
```

thì CPU thực hiện logic tương đương:

```text
MSP = _estack
PC  = Reset_Handler
```

Không phải startup code tự gọi `Reset_Handler`.

Hardware nhảy trực tiếp vào handler.

---

# 3. Vector table phải đặt ở section riêng

Startup assembly:

```asm
.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object

g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    ...
```

Linker:

```ld
.isr_vector :
{
    KEEP(*(.isr_vector))
} > FLASH
```

Do đó vector table nằm đầu Flash.

---

# 4. Vector table của Cortex-M3

Các exception lõi ARM:

```text
0   Initial MSP
1   Reset
2   NMI
3   HardFault
4   MemManage
5   BusFault
6   UsageFault
7   Reserved
8   Reserved
9   Reserved
10  Reserved
11  SVCall
12  Debug Monitor
13  Reserved
14  PendSV
15  SysTick
```

Sau đó mới tới IRQ của STM32F103.

Một phần vector table:

```asm
.word _estack
.word Reset_Handler
.word NMI_Handler
.word HardFault_Handler
.word MemManage_Handler
.word BusFault_Handler
.word UsageFault_Handler
.word 0
.word 0
.word 0
.word 0
.word SVC_Handler
.word DebugMon_Handler
.word 0
.word PendSV_Handler
.word SysTick_Handler
```

---

# 5. IRQ vector của STM32F103

Với STM32F103 medium-density class, vector table thường tiếp tục bằng các IRQ như:

```text
WWDG
PVD
TAMPER
RTC
FLASH
RCC
EXTI0
EXTI1
EXTI2
EXTI3
EXTI4
DMA1_Channel1
...
USART1
USART2
USART3
...
USB_LP_CAN1_RX0
CAN1_RX1
CAN1_SCE
EXTI9_5
...
```

Khi viết startup code thực tế, nên lấy danh sách IRQ chính xác từ:

- CMSIS device header của STM32F103xB.
- Reference startup file tương ứng.
- Vector table trong reference manual/device package.

Không nên tự nhớ toàn bộ số thứ tự IRQ bằng trí nhớ.

Rule quan trọng:

> Tên handler và thứ tự vector phải khớp chính xác với IRQ numbering của device.

---

# 6. Weak handler

Startup code thường cung cấp default handler:

```asm
.weak NMI_Handler
.thumb_set NMI_Handler,Default_Handler
```

Ý nghĩa:

- Nếu application không implement `NMI_Handler`, symbol sẽ trỏ vào `Default_Handler`.
- Nếu application viết:

```c
void NMI_Handler(void)
{
}
```

strong symbol từ application override weak symbol.

Đây là pattern cực kỳ hữu ích.

---

# 7. Default_Handler

Tối thiểu:

```asm
Default_Handler:
    b Default_Handler
```

Hoặc C:

```c
void Default_Handler(void)
{
    while (1)
    {
    }
}
```

Trong debug, loop vô hạn giúp debugger dừng tại interrupt chưa được implement.

Có thể nâng cấp:

```c
void Default_Handler(void)
{
    __BKPT(0);

    while (1)
    {
    }
}
```

nhưng phải cân nhắc behavior khi chạy không có debugger.

---

# 8. Reset_Handler phải làm gì?

Một Reset Handler tối thiểu:

```text
1. Copy .data
2. Clear .bss
3. SystemInit()
4. __libc_init_array() nếu cần
5. main()
6. Nếu main return -> loop
```

---

# 9. Copy `.data`

Linker định nghĩa:

```ld
_sidata = LOADADDR(.data);

.data :
{
    _sdata = .;
    *(.data*)
    _edata = .;
} > RAM AT > FLASH
```

Startup dùng:

```text
source = _sidata
destination = _sdata
until destination == _edata
```

Assembly:

```asm
ldr r0, =_sidata
ldr r1, =_sdata
ldr r2, =_edata

.L_copy_data:
    cmp r1, r2
    bcs .L_data_done

    ldr r3, [r0], #4
    str r3, [r1], #4

    b .L_copy_data

.L_data_done:
```

C version:

```c
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;

uint32_t *src = &_sidata;
uint32_t *dst = &_sdata;

while (dst < &_edata)
{
    *dst++ = *src++;
}
```

---

# 10. Clear `.bss`

Linker:

```ld
.bss :
{
    _sbss = .;
    *(.bss*)
    _ebss = .;
} > RAM
```

Assembly:

```asm
ldr r0, =_sbss
ldr r1, =_ebss
movs r2, #0

.L_zero_bss:
    cmp r0, r1
    bcs .L_bss_done

    str r2, [r0], #4
    b .L_zero_bss

.L_bss_done:
```

C:

```c
extern uint32_t _sbss;
extern uint32_t _ebss;

for (uint32_t *dst = &_sbss; dst < &_ebss; ++dst)
{
    *dst = 0;
}
```

---

# 11. Vì sao phải initialize `.data` và `.bss`?

C language runtime kỳ vọng:

```c
uint32_t a = 123;
uint32_t b;
static uint8_t buffer[64];
```

sau reset:

```text
a == 123
b == 0
buffer[] == 0
```

Hardware không tự đảm bảo các giá trị này.

Startup code phải làm.

---

# 12. `SystemInit()`

CMSIS convention thường dùng:

```c
SystemInit();
```

Function này thường:

- Cấu hình low-level clock state.
- Thiết lập FPU nếu MCU có FPU.
- Relocate vector table nếu project cần.
- Reset hoặc chuẩn hóa một số clock configuration.

Với STM32F103 Cortex-M3 không có FPU.

Một project bare-metal hoàn toàn tự viết có thể không dùng CMSIS `SystemInit()`, nhưng cần tự đảm bảo clock/system state đúng.

---

# 13. `SystemCoreClock`

CMSIS thường cung cấp:

```c
uint32_t SystemCoreClock;
```

Nếu clock thay đổi, cần cập nhật:

```c
SystemCoreClockUpdate();
```

hoặc tự gán đúng trong implementation của bạn.

Nhiều driver delay/SysTick phụ thuộc giá trị này.

---

# 14. C runtime và `__libc_init_array()`

Nếu dùng C đơn giản:

```c
int main(void)
{
}
```

có thể project rất nhỏ vẫn chạy mà không cần nhiều runtime initialization.

Nhưng nếu dùng C++:

```cpp
class Foo
{
public:
    Foo();
};

Foo foo;
```

global constructor phải chạy trước `main()`.

GCC/newlib thường dùng:

```c
__libc_init_array();
```

Startup flow:

```text
initialize .data
clear .bss
SystemInit()
__libc_init_array()
main()
```

Do đó nếu dự án có C++ thì phần này rất quan trọng.

---

# 15. Thumb mode

STM32F103 Cortex-M3 chạy Thumb instruction set.

Assembly startup nên có:

```asm
.syntax unified
.cpu cortex-m3
.thumb
```

Handler function symbol thường:

```asm
.thumb_func
.type Reset_Handler, %function
Reset_Handler:
```

---

# 16. Startup code assembly hoàn chỉnh mẫu

```asm
.syntax unified
.cpu cortex-m3
.thumb

.global g_pfnVectors
.global Reset_Handler

.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object

g_pfnVectors:
    .word _estack
    .word Reset_Handler

    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler

    .word 0
    .word 0
    .word 0
    .word 0

    .word SVC_Handler
    .word DebugMon_Handler
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler

    /*
     * Các STM32 IRQ phải được thêm tiếp theo đúng thứ tự
     * của device STM32F103xB.
     */

.size g_pfnVectors, .-g_pfnVectors


.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
.thumb_func

Reset_Handler:

    /* Copy .data from FLASH to RAM */

    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata

.L_copy_data:
    cmp r1, r2
    bcs .L_data_done

    ldr r3, [r0], #4
    str r3, [r1], #4

    b .L_copy_data

.L_data_done:

    /* Clear .bss */

    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0

.L_zero_bss:
    cmp r0, r1
    bcs .L_bss_done

    str r2, [r0], #4

    b .L_zero_bss

.L_bss_done:

    /* CMSIS system initialization */

    bl SystemInit

    /* C/C++ runtime initialization if required */

    bl __libc_init_array

    /* Enter application */

    bl main

.L_hang:
    b .L_hang

.size Reset_Handler, .-Reset_Handler


.section .text.Default_Handler,"ax",%progbits
Default_Handler:
.L_default_loop:
    b .L_default_loop


.macro WEAK_DEFAULT_HANDLER handler
    .weak \handler
    .thumb_set \handler, Default_Handler
.endm

WEAK_DEFAULT_HANDLER NMI_Handler
WEAK_DEFAULT_HANDLER HardFault_Handler
WEAK_DEFAULT_HANDLER MemManage_Handler
WEAK_DEFAULT_HANDLER BusFault_Handler
WEAK_DEFAULT_HANDLER UsageFault_Handler
WEAK_DEFAULT_HANDLER SVC_Handler
WEAK_DEFAULT_HANDLER DebugMon_Handler
WEAK_DEFAULT_HANDLER PendSV_Handler
WEAK_DEFAULT_HANDLER SysTick_Handler
```

Lưu ý: đoạn trên cố ý chưa liệt kê toàn bộ STM32 peripheral IRQ để tránh tạo startup file sai device variant. Khi dùng thực tế, hãy thêm toàn bộ IRQ theo `STM32F103xB`.

---

# 17. Có thể viết startup bằng C không?

Có, nhưng vector table cần đảm bảo:

- đúng section;
- đúng order;
- đúng type;
- đúng address;
- linker giữ lại;
- initial stack pointer được encode đúng.

Ví dụ conceptual:

```c
typedef void (*isr_handler_t)(void);

extern uint32_t _estack;

__attribute__((section(".isr_vector")))
const isr_handler_t vector_table[] =
{
    (isr_handler_t)&_estack,
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
};
```

Tuy nhiên entry 0 thực chất là data word chứ không phải function pointer.

Vì vậy để tránh type trick và toolchain corner cases, startup assembly vẫn là cách phổ biến và rõ ràng.

---

# 18. Vector relocation

Mặc định firmware boot từ Flash:

```text
VTOR = 0x08000000
```

Nếu có bootloader:

```text
Bootloader:
0x08000000

Application:
0x08004000
```

application vector table nằm:

```text
0x08004000
```

Khi đó cần cấu hình:

```c
SCB->VTOR = 0x08004000;
```

và linker script application cũng phải đổi Flash origin.

Đây là rule quan trọng:

```text
linker origin
vector table location
SCB->VTOR
bootloader jump address
```

phải thống nhất.

---

# 19. Interrupt handler naming

CMSIS/header có prototype:

```c
void USART1_IRQHandler(void);
```

Vector table phải dùng chính xác:

```asm
.word USART1_IRQHandler
```

Nếu viết nhầm:

```c
void USART1_Handler(void)
```

thì vector vẫn trỏ vào weak default handler.

Kết quả:

```text
IRQ xảy ra
↓
Default_Handler
↓
firmware đứng
```

---

# 20. HardFault debugging

Startup code production thường nên có HardFault handler riêng.

Tối thiểu:

```c
void HardFault_Handler(void)
{
    while (1)
    {
    }
}
```

Nâng cao hơn có thể đọc:

```text
CFSR
HFSR
MMFAR
BFAR
stacked PC
stacked LR
```

để biết lý do fault.

Không nên để mọi exception vào cùng một default handler khi project đã phát triển lớn.

---

# 21. Không bật interrupt quá sớm

Nếu startup/system init bật interrupt trước khi:

- `.data` initialized;
- `.bss` cleared;
- peripherals ready;

ISR có thể truy cập state chưa hợp lệ.

Rule:

> Tránh enable application interrupts cho đến khi runtime và peripheral state đã sẵn sàng.

---

# 22. `main()` không nên return

Bare-metal:

```c
int main(void)
{
    while (1)
    {
    }
}
```

Nếu `main()` return, không có OS để quay về.

Startup nên phòng thủ:

```asm
bl main

hang:
    b hang
```

---

# 23. Liên hệ startup với linker

Startup cần:

```text
_estack
_sidata
_sdata
_edata
_sbss
_ebss
```

Linker phải export đúng symbol.

Ví dụ:

```ld
_estack = ORIGIN(RAM) + LENGTH(RAM);
_sidata = LOADADDR(.data);
```

Đổi tên linker symbol thì startup phải đổi.

---

# 24. Liên hệ startup với CMake

CMake phải compile startup:

```cmake
project(firmware LANGUAGES C ASM)
```

và:

```cmake
target_sources(firmware PRIVATE
    startup/startup_stm32f103xb.s
)
```

Cần:

```text
ASM language enabled
```

Nếu startup file dùng preprocessor:

```text
.S
```

uppercase `.S` thường được preprocess bởi GCC.

Nếu là:

```text
.s
```

thường assembler trực tiếp.

---

# 25. Startup `.s` hay `.S`?

## `.s`

Assembly thuần:

```text
startup.s
```

không dùng C preprocessor.

## `.S`

Assembly được preprocess:

```text
startup.S
```

có thể dùng:

```c
#ifdef
#define
#include
```

Nếu startup có conditional logic theo MCU, `.S` tiện hơn.

---

# 26. Rule viết startup code

Một startup code tốt nên:

1. Tối thiểu và deterministic.
2. Không phụ thuộc heap.
3. Không phụ thuộc global state chưa initialize.
4. Không gọi code application trước khi `.data/.bss` hợp lệ.
5. Không bật interrupt không cần thiết quá sớm.
6. Có vector table đúng thứ tự.
7. Có weak default handlers.
8. Dùng đúng linker symbols.
9. Không cho `main()` return vào vùng không xác định.
10. Có cấu trúc dễ debug.

---

# 27. Checklist startup STM32F103C8T6

- [ ] `.syntax unified`.
- [ ] `.cpu cortex-m3`.
- [ ] `.thumb`.
- [ ] Vector table nằm trong `.isr_vector`.
- [ ] Vector entry 0 là `_estack`.
- [ ] Vector entry 1 là `Reset_Handler`.
- [ ] Core exception order đúng.
- [ ] STM32 peripheral IRQ order đúng device.
- [ ] Handler naming khớp CMSIS.
- [ ] Có weak handlers.
- [ ] Có `Default_Handler`.
- [ ] Reset handler copy `.data`.
- [ ] Reset handler clear `.bss`.
- [ ] Gọi `SystemInit()` nếu project dùng convention này.
- [ ] Gọi `__libc_init_array()` nếu cần C++/constructors.
- [ ] Gọi `main()`.
- [ ] Có infinite loop nếu `main()` return.
- [ ] Startup symbol names khớp linker script.
- [ ] CMake bật `ASM`.
- [ ] Startup file được add vào target.

---

# 28. Mental model cuối cùng

Startup code không phải “một file assembly thần bí”.

Nó chỉ là bridge giữa:

```text
Cortex-M reset hardware
        ↓
memory layout do linker tạo
        ↓
C/C++ runtime assumptions
        ↓
main()
```

Nếu bạn hiểu:

```text
vector table
→ Reset_Handler
→ .data copy
→ .bss clear
→ SystemInit
→ runtime init
→ main
```

thì bạn đã hiểu phần cốt lõi của startup code bare-metal.
