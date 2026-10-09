# Quy tắc viết Linker Script cho STM32F103C8T6

> **Phạm vi:** GNU linker script cho STM32F103C8T6 bare-metal, gồm Flash, RAM,
> vector table, section runtime, stack và kiểm tra tràn vùng nhớ.

> **Tác dụng:** Hướng dẫn đọc và tự viết linker script đúng địa chỉ cho
> Bootloader hoặc Application.

## 1. Linker script dùng để làm gì?

Trong một dự án bare-metal, compiler chỉ tạo ra các object file (`.o`).

Linker script quyết định:

- Code nằm ở đâu trong Flash.
- Biến nào nằm trong RAM.
- Vector table nằm ở địa chỉ nào.
- `.data` được load từ Flash nhưng chạy trong RAM như thế nào.
- `.bss` được cấp vùng RAM và được reset về 0 như thế nào.
- Stack bắt đầu ở đâu.
- Heap nằm ở đâu.
- Những section nào cần giữ lại dù linker nghĩ chúng không được dùng.
- Firmware có vượt quá Flash/RAM hay không.

Với STM32F103C8T6:

```text
CPU   : ARM Cortex-M3
Flash : 64 KB
SRAM  : 20 KB

FLASH origin = 0x08000000
RAM origin   = 0x20000000
```

> Lưu ý: một số board clone có chip thực tế lớn hơn 64 KB Flash, nhưng khi viết linker script cho đúng part number STM32F103C8T6 thì nên dùng 64 KB trừ khi bạn đã xác minh phần cứng cụ thể.

---

# 2. Mental model quan trọng nhất

Một firmware điển hình có các section sau:

```text
FLASH
0x08000000
+---------------------------+
| .isr_vector               |
| .text                     |
| .rodata                   |
| init/fini arrays          |
| initial values of .data   |
+---------------------------+

RAM
0x20000000
+---------------------------+
| .data                     |
| .bss                      |
| heap                      |
| ...                       |
| stack                     |
+---------------------------+
0x20005000   <- cuối 20 KB RAM
```

`.data` có hai địa chỉ quan trọng:

```text
LMA: địa chỉ dữ liệu khởi tạo nằm trong Flash
VMA: địa chỉ biến thực sự tồn tại khi chương trình chạy trong RAM
```

Startup code sẽ copy:

```text
Flash LMA of .data
        ↓
RAM VMA of .data
```

---

# 3. Cấu trúc tối thiểu của linker script

Một linker script bare-metal thường có 4 phần chính:

```ld
ENTRY(Reset_Handler)

MEMORY
{
    ...
}

SECTIONS
{
    ...
}
```

Ngoài ra thường định nghĩa symbol:

```ld
_estack
_sidata
_sdata
_edata
_sbss
_ebss
```

Startup code dùng trực tiếp các symbol này.

---

# 4. ENTRY()

```ld
ENTRY(Reset_Handler)
```

Ý nghĩa:

- Chỉ định entry symbol cho file ELF.
- Với Cortex-M, CPU thực tế lấy Reset Handler từ vector table khi reset.
- Tuy nhiên `ENTRY()` vẫn rất hữu ích cho ELF/debugger/tooling.

Tên này phải khớp startup code:

```asm
Reset_Handler:
```

hoặc symbol tương đương.

---

# 5. MEMORY

Template cho STM32F103C8T6:

```ld
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 20K
}
```

Ý nghĩa permission:

```text
r = read
w = write
x = executable
```

Thông thường:

```text
FLASH -> rx
RAM   -> xrw
```

RAM có thể chứa code nếu bạn cố tình copy code xuống RAM để chạy, nên nhiều template để `xrw`.

---

# 6. Stack pointer

Trên Cortex-M:

```text
vector_table[0] = initial MSP
vector_table[1] = Reset_Handler
```

Stack thường bắt đầu từ cuối RAM và tăng xuống dưới.

Ta định nghĩa:

```ld
_estack = ORIGIN(RAM) + LENGTH(RAM);
```

Với STM32F103C8T6:

```text
0x20000000 + 20 KB
= 0x20005000
```

Do đó:

```ld
_estack = 0x20005000;
```

nhưng nên dùng công thức thay vì hard-code.

---

# 7. Vector table

Section quan trọng nhất:

```ld
.isr_vector :
{
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
} > FLASH
```

## Tại sao cần `KEEP()`?

Nếu compile với:

```text
-ffunction-sections
-fdata-sections
```

và link với:

```text
-Wl,--gc-sections
```

linker sẽ loại section mà nó nghĩ không được tham chiếu.

Vector table có thể không được code C gọi trực tiếp.

Do đó phải:

```ld
KEEP(*(.isr_vector))
```

để ngăn garbage collection xóa vector table.

Đây là một rule rất quan trọng.

---

# 8. `.text`

Section chứa machine code:

```ld
.text :
{
    . = ALIGN(4);

    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)

    KEEP(*(.init))
    KEEP(*(.fini))

    . = ALIGN(4);
    _etext = .;
} > FLASH
```

Vì dùng:

```text
-ffunction-sections
```

compiler có thể tạo:

```text
.text.main
.text.GPIO_Init
.text.UART_Init
```

Nên phải match:

```ld
*(.text*)
```

chứ không chỉ:

```ld
*(.text)
```

---

# 9. `.rodata`

Constant data:

```ld
.rodata :
{
    . = ALIGN(4);

    *(.rodata)
    *(.rodata*)

    . = ALIGN(4);
} > FLASH
```

Ví dụ:

```c
const char msg[] = "Hello";
```

thường nằm trong `.rodata`.

---

# 10. ARM exception metadata

GCC ARM toolchain có thể sinh:

```text
.ARM.extab
.ARM.exidx
```

Template:

```ld
.ARM.extab :
{
    *(.ARM.extab* .gnu.linkonce.armextab.*)
} > FLASH

.ARM.exidx :
{
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
} > FLASH
```

Với firmware C đơn giản có thể ít thấy tác dụng, nhưng giữ đúng template sẽ giúp tương thích C++/unwind/toolchain tốt hơn.

---

# 11. `.preinit_array`, `.init_array`, `.fini_array`

Đặc biệt quan trọng nếu dùng C++ hoặc constructor.

```ld
.preinit_array :
{
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
} > FLASH

.init_array :
{
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
} > FLASH

.fini_array :
{
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
} > FLASH
```

Nếu project chỉ dùng C thuần, vẫn có thể giữ các section này.

---

# 12. `.data`

`.data` chứa các biến global/static có giá trị khởi tạo khác 0.

Ví dụ:

```c
uint32_t counter = 10;
```

Giá trị `10` phải nằm trong Flash vì RAM mất dữ liệu khi reset.

Runtime cần:

```text
Flash:
10
 ↓ copy
RAM:
counter = 10
```

Linker:

```ld
_sidata = LOADADDR(.data);

.data :
{
    . = ALIGN(4);
    _sdata = .;

    *(.data)
    *(.data*)

    . = ALIGN(4);
    _edata = .;
} > RAM AT > FLASH
```

Các symbol:

```text
_sidata = source address trong Flash
_sdata  = start destination trong RAM
_edata  = end destination trong RAM
```

Startup code:

```c
src = &_sidata;
dst = &_sdata;

while (dst < &_edata)
{
    *dst++ = *src++;
}
```

### Rule

Nếu dùng:

```ld
> RAM AT > FLASH
```

thì phải có startup code copy `.data`.

---

# 13. `.bss`

`.bss` chứa:

```c
uint32_t x;
static uint8_t buffer[1024];
```

Các biến zero-initialized không cần lưu cả một block toàn số 0 trong Flash.

Linker chỉ cấp địa chỉ RAM:

```ld
.bss (NOLOAD) :
{
    . = ALIGN(4);
    _sbss = .;

    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;
} > RAM
```

Startup code phải clear:

```c
for (dst = &_sbss; dst < &_ebss; ++dst)
{
    *dst = 0;
}
```

---

# 14. Heap và Stack

Bare-metal đơn giản có thể không dùng heap.

Nhưng newlib hoặc `malloc()` có thể cần heap.

Một template:

```ld
._user_heap_stack :
{
    . = ALIGN(8);

    PROVIDE(end = .);
    PROVIDE(_end = .);

    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;

    . = ALIGN(8);
} > RAM
```

Trước đó:

```ld
_Min_Heap_Size  = 0x200;
_Min_Stack_Size = 0x400;
```

Mục đích:

- Link-time check rằng RAM còn đủ tối thiểu cho heap + stack.
- Không nhất thiết tự động cấu hình runtime heap hoàn chỉnh.

Nếu không dùng dynamic allocation, có thể:

```ld
_Min_Heap_Size = 0;
```

---

# 15. Alignment

Cortex-M3 có thể xử lý một số access unaligned, nhưng firmware nên giữ alignment rõ ràng.

Thông thường:

```ld
. = ALIGN(4);
```

cho `.text`, `.data`, `.bss`.

Stack theo AAPCS thường nên align ít nhất 8 byte tại public interface.

Có thể dùng:

```ld
. = ALIGN(8);
```

cho vùng heap/stack.

---

# 16. Một linker script hoàn chỉnh mẫu

```ld
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size  = 0x000;
_Min_Stack_Size = 0x400;

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 20K
}

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);

        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)

        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;

        *(.data)
        *(.data*)

        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;

        *(.bss)
        *(.bss*)
        *(COMMON)

        . = ALIGN(4);
        _ebss = .;
    } > RAM

    ._user_heap_stack :
    {
        . = ALIGN(8);

        PROVIDE(end = .);
        PROVIDE(_end = .);

        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;

        . = ALIGN(8);
    } > RAM
}
```

---

# 17. Liên hệ linker script với startup code

Linker định nghĩa:

```ld
_sidata
_sdata
_edata
_sbss
_ebss
_estack
```

Startup sử dụng:

```asm
ldr r0, =_sidata
ldr r1, =_sdata
ldr r2, =_edata
```

hoặc bằng C:

```c
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
```

Do đó linker và startup là một cặp.

Nếu đổi tên linker symbol:

```ld
__data_start__
```

thì startup cũng phải đổi theo.

---

# 18. Liên hệ linker với CMake

CMake phải truyền linker script:

```cmake
target_link_options(firmware PRIVATE
    -T${LINKER_SCRIPT}
)
```

và nên bật:

```cmake
-Wl,--gc-sections
```

Nếu linker dùng:

```ld
KEEP(*(.isr_vector))
```

thì vector table vẫn tồn tại dù bật garbage collection.

---

# 19. Map file

Rất nên sinh map file:

```text
firmware.map
```

CMake:

```cmake
-Wl,-Map=${PROJECT_NAME}.map
```

Map file giúp kiểm tra:

- Function nào chiếm Flash.
- Variable nào chiếm RAM.
- Section nằm ở đâu.
- Library nào kéo thêm code.
- Linker symbol có địa chỉ nào.

---

# 20. Các lỗi linker phổ biến

## 20.1 Sai Flash origin

Sai:

```ld
FLASH : ORIGIN = 0x00000000
```

Nếu firmware được flash ở:

```text
0x08000000
```

thì vector table/link address không đúng.

---

## 20.2 Quên `.isr_vector`

CPU reset sẽ đọc:

```text
0x08000000 -> MSP
0x08000004 -> Reset_Handler
```

Nếu vector table không ở đầu Flash, firmware có thể không boot.

---

## 20.3 Quên `KEEP()`

Khi có:

```text
--gc-sections
```

vector table có thể bị linker loại bỏ.

---

## 20.4 `.data` đặt RAM nhưng không có `AT > FLASH`

Sai:

```ld
.data :
{
    ...
} > RAM
```

Điều này không cung cấp load image đúng cho giá trị khởi tạo trong firmware.

Thông thường cần:

```ld
} > RAM AT > FLASH
```

---

## 20.5 Startup không copy `.data`

Kết quả:

```c
uint32_t x = 10;
```

có thể không trở thành `10` sau reset.

---

## 20.6 Startup không clear `.bss`

C standard yêu cầu static-storage variables không khởi tạo phải bắt đầu bằng 0.

---

## 20.7 RAM/Flash LENGTH sai

Có thể build được nhưng flash/runtime lỗi khó đoán.

Luôn kiểm tra datasheet/reference part number thực tế.

---

# 21. Checklist linker script STM32F103C8T6

Trước khi coi linker script là ổn:

- [ ] `ENTRY(Reset_Handler)` đúng tên.
- [ ] `FLASH ORIGIN = 0x08000000`.
- [ ] `FLASH LENGTH = 64K`.
- [ ] `RAM ORIGIN = 0x20000000`.
- [ ] `RAM LENGTH = 20K`.
- [ ] `_estack` nằm ở cuối RAM.
- [ ] `.isr_vector` đứng đầu Flash.
- [ ] `.isr_vector` có `KEEP()`.
- [ ] `.text*` nằm trong Flash.
- [ ] `.rodata*` nằm trong Flash.
- [ ] `.data*` chạy trong RAM và load từ Flash.
- [ ] Có `_sidata`, `_sdata`, `_edata`.
- [ ] `.bss*` nằm trong RAM.
- [ ] Có `_sbss`, `_ebss`.
- [ ] Các section được align phù hợp.
- [ ] Startup code sử dụng đúng linker symbols.
- [ ] CMake truyền `-T<linker_script>`.
- [ ] Build tạo `.map`.
- [ ] Có kiểm tra Flash/RAM usage.

---

# 22. Rule tổng quát

Khi tự viết linker script cho bất kỳ MCU nào, hãy trả lời 6 câu hỏi:

```text
1. Flash bắt đầu ở đâu, dài bao nhiêu?
2. RAM bắt đầu ở đâu, dài bao nhiêu?
3. CPU cần vector table ở đâu?
4. Code/const data nằm ở đâu?
5. Initialized RAM data được load từ đâu?
6. Startup code cần những linker symbols nào?
```

Nếu trả lời đúng 6 câu này, bạn đã nắm phần cốt lõi của linker script bare-metal.
