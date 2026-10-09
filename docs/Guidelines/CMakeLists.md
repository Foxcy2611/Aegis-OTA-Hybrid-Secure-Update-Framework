# Hướng dẫn viết CMake cho STM32F103C8T6 bare-metal

> **Mục tiêu:** đủ để tự viết và đọc một project CMake bare-metal cho
> STM32F103C8T6 mà không biến tài liệu thành danh sách flag dài.
>
> **Phạm vi:** ARM GNU Toolchain, C + Assembly, CMSIS/SPL, linker script,
> Ninja, ELF/BIN/HEX/MAP và OpenOCD.

> **Tác dụng:** Hướng dẫn cách tổ chức và tự viết CMake cho một project
> STM32F103C8T6 bare-metal, không phải CMake riêng của một phase Aegis.

---

## I. Cách nghĩ trước khi viết CMake

Đừng bắt đầu bằng việc nhớ flag. Hãy nhìn firmware theo bốn lớp:

```text
Firmware target
    |
    +-- Compile cái gì?
    |      source, startup, system source
    |
    +-- Compile như thế nào?
    |      include, define, CPU/ABI flags, Debug/Release
    |
    +-- Link như thế nào?
    |      linker script, runtime library, map file
    |
    +-- Sau khi link làm gì?
           ELF -> BIN / HEX / size / flash
```

Một `CMakeLists.txt` tốt phải cho người đọc trả lời được:

1. Firmware gồm những source nào?
2. Compiler nhận include/define/flags nào?
3. Linker dùng linker script và library nào?
4. Sau link sinh artifact gì?

---

## II. CMake, Ninja và toolchain làm gì?

| Thành phần | Vai trò |
|---|---|
| `CMakeLists.txt` | Mô tả build graph |
| CMake | Đọc project rồi sinh build system |
| Ninja | Thực thi compile/link và incremental build |
| `arm-none-eabi-gcc` | Compile C/ASM và gọi linker |
| Linker script | Quyết định layout Flash/RAM |
| `objcopy` | ELF -> BIN/HEX |
| `size` | Báo kích thước firmware |
| OpenOCD | Flash/debug qua probe |

```text
CMakeLists.txt + toolchain + linker script
                    |
                    v
              CMake configure
                    |
                    v
               build.ninja
                    |
                    v
                Ninja
                    |
              C/ASM -> .o
                    |
                    v
                   ELF
               /    |    \
             BIN   HEX   MAP
```

Không sửa `build.ninja`; đó là file CMake sinh tự động.

---

## III. Bảng tra nhanh các lệnh CMake thường dùng

> Trong CMake, các thứ như `set()`, `project()`, `target_sources()` thường được
> gọi chung là **commands**.

| Lệnh | Dùng để làm gì? | Trong bare-metal thường dùng cho |
|---|---|---|
| `cmake_minimum_required()` | Chọn version CMake tối thiểu | Khóa behavior theo version |
| `project()` | Tạo project, bật C/C++/ASM | `LANGUAGES C ASM` |
| `set()` | Gán biến/list | `MCU_FLAGS`, linker script |
| `find_program()` | Tìm executable trên máy | GCC, objcopy, OpenOCD |
| `message()` | In thông báo khi configure | Debug CMake |
| `add_executable()` | Tạo executable target | Firmware ELF |
| `add_library()` | Tạo library target | Driver/module/config |
| `target_sources()` | Thêm source vào target | `.c`, startup `.s/.S` |
| `target_include_directories()` | Thêm include path | `Inc/`, CMSIS headers |
| `target_compile_definitions()` | Thêm macro `-D...` | device/library macro |
| `tarrget_compile_features()` | Một target cần các tính năng của một chuẩn C/C++ nhất định | Cấu trúc C dùng chung |
| `target_compile_options()` | Thêm compiler flags | CPU, warning, optimization |
| `target_link_options()` | Thêm linker flags | `-T`, `--gc-sections`, map |
| `target_link_libraries()` | Link library/target khác | `c`, `m`, `gcc`, driver lib |
| `set_target_properties()` | Đặt property target | `.elf`, `LINK_DEPENDS` |
| `add_custom_command()` | Chạy lệnh gắn với build | ELF -> BIN/HEX, size |
| `add_custom_target()` | Tạo target thủ công | `flash`, `erase` |
| `add_subdirectory()` | Nạp `CMakeLists.txt` con | Project lớn |
| `include()` | Nạp CMake module/file | `CheckIPOSupported` |
| `if()/else()/endif()` | Điều kiện | Có/không có OpenOCD |
| `function()` | Tạo command của riêng project | Giảm lặp nhiều firmware |
| `set_property()` | Đặt property | LTO/IPO, target settings |

### 1. Từ khóa, toán tử và cú pháp thường gặp

Các từ như `FORCE`, `STREQUAL` hay `PRIVATE` không phải command độc lập. Ý nghĩa
của chúng phụ thuộc command đang chứa chúng.

#### a. Biến thường và Cache

| Cú pháp/từ khóa | Ý nghĩa |
|---|---|
| `${NAME}` | Lấy giá trị biến CMake `NAME` |
| `$ENV{NAME}` | Lấy biến môi trường của hệ điều hành |
| `CACHE` | Lưu biến bền vững trong `CMakeCache.txt` của build directory |
| `STRING` | Cache entry dạng chuỗi |
| `BOOL` | Cache entry dạng `ON/OFF` |
| `FILEPATH` | Cache entry là đường dẫn tới một file |
| `PATH` | Cache entry là đường dẫn tới một thư mục |
| `INTERNAL` | Cache entry nội bộ, không hiện như option thông thường |
| `FORCE` | Ghi đè cache entry đã tồn tại |

Ví dụ:

```cmake
set(CMAKE_BUILD_TYPE
    Debug
    CACHE STRING "Debug hoặc Release"
)
```

Nếu `CMAKE_BUILD_TYPE` đã được người dùng truyền bằng `-D`, lệnh trên không ghi
đè nó. Thêm `FORCE` mới bắt buộc thay giá trị cũ:

```cmake
set(CMAKE_BUILD_TYPE
    Debug
    CACHE STRING "Debug hoặc Release"
    FORCE
)
```

Chỉ dùng `FORCE` khi project thực sự phải chiếm quyền quyết định. Lạm dụng nó sẽ
làm option người dùng truyền vào bị vô hiệu hóa.

#### b. Toán tử trong `if()`

| Từ khóa | Kiểm tra |
|---|---|
| `STREQUAL` | Hai chuỗi giống hệt nhau |
| `MATCHES` | Chuỗi khớp regular expression |
| `EQUAL` | Hai số bằng nhau |
| `LESS`, `GREATER` | So sánh hai số |
| `VERSION_LESS` | So sánh version |
| `IN_LIST` | Một giá trị có nằm trong list không |
| `DEFINED` | Biến đã được định nghĩa chưa |
| `EXISTS` | File hoặc thư mục có tồn tại không |
| `TARGET` | CMake target đã được tạo chưa |
| `COMMAND` | CMake command có tồn tại không |
| `NOT` | Phủ định điều kiện |
| `AND`, `OR` | Kết hợp nhiều điều kiện |

Ví dụ so sánh generator:

```cmake
if(CMAKE_GENERATOR STREQUAL "Ninja")
    message(STATUS "Đang dùng Ninja")
endif()
```

Ví dụ kết hợp điều kiện:

```cmake
if(DEFINED CMAKE_BUILD_TYPE AND
   CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release build")
endif()
```

Ví dụ kiểm tra target:

```cmake
if(TARGET firmware)
    message(STATUS "Firmware target đã được tạo")
endif()
```

`STREQUAL` so sánh chuỗi, còn `EQUAL` so sánh số. Không dùng `EQUAL` để so sánh
`Debug`, `Release` hoặc `Ninja`.

#### c. Phạm vi truyền cấu hình giữa các target

| Từ khóa | Target hiện tại dùng? | Target link nó kế thừa? |
|---|---:|---:|
| `PRIVATE` | Có | Không |
| `PUBLIC` | Có | Có |
| `INTERFACE` | Không | Có |

- PRIVATE là cấu hình (như file .h, cờ biên dịch) chỉ nội bộ thư viện đó tự dùng và giấu kín.
    - Ví dụ: Các lib `.h`, `.c` tự viết trong `bootloader/`, `application/`.
- PUBLIC là thư viện vừa tự dùng cho nó, vừa chia sẻ cho các dự án khác gọi đến nó. 
    - Ví dụ: Các lib trong `common/`.
- Còn INTERFACE là bản thân thư viện không dùng cấu hình đó, mà chỉ để truyền lại cho các dự án khác khi họ liên kết với thư viện này.
    - Ví dụ: Các header trong `shared/`.

Ví dụ:

```cmake
target_include_directories(uart_driver PUBLIC
    Inc
)
```

`uart_driver` cần header trong `Inc`, và firmware link `uart_driver` cũng cần
include path này nên dùng `PUBLIC`.

#### d. Loại library

| Từ khóa | Output/behavior |
|---|---|
| `STATIC` | Tạo archive `.a` |
| `OBJECT` | Tạo tập object `.o`, chưa archive/link thành image |
| `SHARED` | Tạo shared library; thường không dùng trong bare-metal nhỏ |
| `INTERFACE` | Không compile source, chỉ mang usage requirements |

Ví dụ:

```cmake
add_library(crc STATIC Src/crc32.c)
add_library(project_options INTERFACE)
```

#### e. Từ khóa của custom command/target

| Từ khóa | Ý nghĩa |
|---|---|
| `COMMAND` | Command-line cần chạy |
| `DEPENDS` | Chỉ chạy sau khi dependency đã sẵn sàng |
| `POST_BUILD` | Chạy sau khi target link xong |
| `BYPRODUCTS` | Khai báo file phụ do command tạo |
| `VERBATIM` | Để CMake escape argument đúng cho generator/platform |
| `USES_TERMINAL` | Cho command dùng trực tiếp terminal, phù hợp flash/monitor |
| `ALL` | Đưa custom target vào build mặc định |
| `EXCLUDE_FROM_ALL` | Không build target/subdirectory trong build mặc định |

Ví dụ:

```cmake
add_custom_command(TARGET firmware POST_BUILD
    COMMAND ${CMAKE_OBJCOPY}
        -O binary
        $<TARGET_FILE:firmware>
        firmware.bin
    BYPRODUCTS firmware.bin
    VERBATIM
)
```

Trong ví dụ trên:

- `TARGET firmware`: command gắn với target `firmware`;
- `POST_BUILD`: chạy sau khi ELF được link;
- `COMMAND`: gọi `objcopy`;
- `BYPRODUCTS`: báo cho Ninja rằng `firmware.bin` được sinh ra;
- `VERBATIM`: bảo vệ cách truyền argument, nhất là khi đường dẫn có khoảng trắng.

#### f. Generator expression

Biểu thức sinh `$<...>`: Giống như một "giấy hẹn". CMake không dịch ngay mà đợi đến tận **lúc build** mới đánh giá và điền giá trị thật. Dùng cho những thông tin mà lúc cấu hình CMake chưa thể biết trước.

Cú pháp chung
```cmake
$<$<ĐIỀU_KIỆN>:GIÁ_TRỊ_NẾU_ĐÚNG>
```
- Nếu điều kiện thỏa mãn, `Cmake` sẽ lấy `GIÁ_TRỊ_NẾU_DDUNSG` nhét vào file build

Ví dụ:
```cmake
target_compile_options(my_target PRIVATE 
    $<$<CONFIG:Debug>:-g>
)
```
- Nếu cấu hình hiện tại là (`CONFIG`) là `Debug`, thì áp dụng cờ `-g` vào `Target`.

#### g. Từ khóa thường gặp trong `project()` và property

| Từ khóa | Ý nghĩa |
|---|---|
| `VERSION` | Version của project |
| `DESCRIPTION` | Mô tả project |
| `LANGUAGES` | Các ngôn ngữ cần bật như `C`, `CXX`, `ASM` |
| `PROPERTY` | Chỉ ra property đang được đặt |
| `STRINGS` | Danh sách giá trị gợi ý cho một cache property |
| `REQUIRED` | Không tìm thấy dependency/tool thì configure thất bại |

Ví dụ:

```cmake
project(
    MyFirmware
    VERSION 1.0.0
    DESCRIPTION "STM32 bare-metal firmware"
    LANGUAGES C ASM
)
```

### 2. `find_program()` thực chất làm gì?

Ví dụ:

```cmake
find_program(ARM_GCC arm-none-eabi-gcc REQUIRED)
```

CMake tìm executable `arm-none-eabi-gcc` trong môi trường hệ thống và lưu đường
 dẫn tìm được vào biến `ARM_GCC`.

```text
find_program(ARM_GCC arm-none-eabi-gcc REQUIRED)
                     |
                     v
ARM_GCC = .../bin/arm-none-eabi-gcc
```

`REQUIRED` nghĩa là không tìm thấy thì configure fail ngay.

Các tool thường gặp:

| Tool | Công dụng |
|---|---|
| `arm-none-eabi-gcc` | Compile C/ASM và link |
| `arm-none-eabi-ar` | Tạo/quản lý static library `.a` |
| `arm-none-eabi-objcopy` | ELF -> BIN/HEX |
| `arm-none-eabi-objdump` | Disassembly/inspect ELF |
| `arm-none-eabi-size` | Xem `.text/.data/.bss` |
| `openocd` | Flash/debug MCU |

---

## IV. Layout project nên dùng

```text
stm32-project/
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/
│   └── arm-none-eabi-toolchain.cmake
├── linker/
│   └── stm32f103c8.ld
├── startup/
│   └── startup_stm32f103.s
├── Inc/
├── Src/
└── Libraries/
```

Build đặt ngoài source:

```text
build/
├── debug/
└── release/
```

Không commit thư mục `build/`.

---

## V. Toolchain file

Compiler phải được chọn **trước khi `project()` chạy**. Vì vậy hãy để compiler
trong toolchain file.

`cmake/arm-none-eabi-toolchain.cmake`:

```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

find_program(ARM_GCC arm-none-eabi-gcc REQUIRED)
find_program(ARM_AR arm-none-eabi-ar REQUIRED)
find_program(ARM_OBJCOPY arm-none-eabi-objcopy REQUIRED)
find_program(ARM_OBJDUMP arm-none-eabi-objdump REQUIRED)
find_program(ARM_SIZE arm-none-eabi-size REQUIRED)

set(CMAKE_C_COMPILER "${ARM_GCC}")
set(CMAKE_ASM_COMPILER "${ARM_GCC}")
set(CMAKE_AR "${ARM_AR}")

set(CMAKE_OBJCOPY "${ARM_OBJCOPY}" CACHE FILEPATH "Objcopy")
set(CMAKE_OBJDUMP "${ARM_OBJDUMP}" CACHE FILEPATH "Objdump")
set(CMAKE_SIZE "${ARM_SIZE}" CACHE FILEPATH "Size tool")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
```

Ý nghĩa quan trọng:

- `Generic`: target không chạy desktop OS.
- C và ASM đều dùng ARM GCC driver.
- `STATIC_LIBRARY`: lúc kiểm tra compiler, CMake không cố link một executable
  bare-metal chưa có startup/linker script.

Không nên làm:

```cmake
project(MyFirmware)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
```

Đến `project()`, CMake đã bắt đầu detect compiler rồi.

---

## VI. Root `CMakeLists.txt`

Khởi đầu tối thiểu:

```cmake
cmake_minimum_required(VERSION 3.22)

project(STM32Firmware LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(firmware)
```

Chỉ bật C++ khi có `.cpp`:

```cmake
project(STM32Firmware LANGUAGES C CXX ASM)
```

### 1. Thứ tự nên viết trong file

```text
1. cmake_minimum_required()
2. project()
3. language settings
4. add_executable()/add_library()
5. target_sources()
6. target_include_directories()
7. target_compile_definitions()
8. target_compile_options()
9. target_link_options()
10. target_link_libraries()
11. target properties
12. post-build
13. flash/debug helper
```

Đây không phải luật cú pháp cứng; nó là layout giúp file dễ đọc.

---

## VII. Source, include và define

### 1. Source nào được compile?

```cmake
target_sources(firmware PRIVATE
    Src/main.c
    Src/uart.c
    startup/startup_stm32f103.s
)
```

`target_sources()` trả lời câu hỏi: **file implementation nào thuộc firmware?**

Nên khai báo source production tường minh thay vì:

```cmake
file(GLOB SOURCES Src/*.c)
```

### 2. Header được tìm ở đâu?

```cmake
target_include_directories(firmware PRIVATE
    Inc
    Libraries/CMSIS/CM3/CoreSupport
    Libraries/CMSIS/CM3/DeviceSupport
    Libraries/STM32F10x_StdPeriph_Driver/inc
)
```

Nó tương đương các option `-I...`.

Nhớ phân biệt:

```text
target_sources()               -> compile file nào
target_include_directories()   -> tìm header ở đâu
```

### 3. Macro nào phải được define?

Với SPL cũ:

```cmake
target_compile_definitions(firmware PRIVATE
    STM32F10X_MD
    USE_STDPERIPH_DRIVER
    HSE_VALUE=8000000UL
)
```

Tương đương:

```text
-DSTM32F10X_MD
-DUSE_STDPERIPH_DRIVER
-DHSE_VALUE=8000000UL
```

Nếu dùng STM32Cube CMSIS/HAL, device macro có thể khác, ví dụ `STM32F103xB`.
Không trộn macro của hai hệ library.

---

## VIII. CPU/ABI và compile flags

STM32F103C8T6 dùng Cortex-M3, Thumb, không có FPU phần cứng:

```cmake
set(MCU_FLAGS
    -mcpu=cortex-m3
    -mthumb
    -mfloat-abi=soft
)
```

Áp dụng lúc compile:

```cmake
target_compile_options(firmware PRIVATE
    ${MCU_FLAGS}
    -ffunction-sections
    -fdata-sections
    -fno-common
    -Wall
    -Wextra
)
```

Và lúc link:

```cmake
target_link_options(firmware PRIVATE
    ${MCU_FLAGS}
)
```

Không copy FPU flags từ STM32F4 như:

```text
-mfpu=fpv4-sp-d16
-mfloat-abi=hard
```

### 1. Debug và Release

```cmake
target_compile_options(firmware PRIVATE
    $<$<CONFIG:Debug>:-Os> # giữ firmware đúng partition; vẫn có -g3
    $<$<CONFIG:Debug>:-g3>

    $<$<CONFIG:Release>:-Os>
    $<$<CONFIG:Release>:-g3>
)
```

| Build | Mục tiêu |
|---|---|
| Debug | Debug dễ, ít phá cấu trúc code |
| Release | Tối ưu kích thước |

Giữ debug info trong Release ELF không làm tăng BIN được flash vào MCU.

---

## IX. Startup và system source

Startup phải nằm trong target:

```cmake
target_sources(firmware PRIVATE
    startup/startup_stm32f103.s
)
```

Nếu dùng CMSIS/SPL:

```cmake
target_sources(firmware PRIVATE
    Libraries/CMSIS/CM3/DeviceSupport/system_stm32f10x.c
    Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_rcc.c
    Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_gpio.c
)
```

Không cần compile toàn bộ SPL nếu firmware chỉ dùng vài peripheral.

Startup và linker script phải thống nhất các symbol như:

```text
_sidata
_sdata
_edata
_sbss
_ebss
_estack
```

---

## X. Linker script

```cmake
set(LINKER_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32f103c8.ld
)

target_link_options(firmware PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -nostartfiles
    -static
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/firmware.map
)

set_target_properties(firmware PROPERTIES
    SUFFIX ".elf"
    LINK_DEPENDS "${LINKER_SCRIPT}"
)
```

Các option quan trọng:

| Option | Ý nghĩa |
|---|---|
| `-T...` | Chọn linker script |
| `-nostartfiles` | Không dùng startup mặc định của runtime |
| `nano.specs` | Dùng newlib-nano |
| `nosys.specs` | Stub syscall cho bare-metal |
| `--gc-sections` | Loại section không dùng |
| `-Map=...` | Sinh map file |
| `LINK_DEPENDS` | Sửa `.ld` thì firmware được relink |

Với STM32F103C8T6, linker script phải phản ánh đúng memory layout của project.
Nếu có bootloader, vùng Flash của application có thể nhỏ hơn toàn bộ chip.

CMake **không thay linker script** trong việc kiểm tra memory layout.

---

## XI. Standard/runtime libraries

```cmake
target_link_libraries(firmware PRIVATE
    "-Wl,--start-group"
    c
    m
    gcc
    "-Wl,--end-group"
)
```

| Library | Vai trò |
|---|---|
| `c` | C standard library |
| `m` | `sqrtf`, `sinf`, ... |
| `gcc` | GCC runtime helpers |

Nếu thực sự cần float `printf`:

```cmake
target_link_options(firmware PRIVATE
    -u _printf_float
)
```

Không bật mặc định vì có thể làm firmware lớn lên đáng kể.

---

## XII. Sinh BIN, HEX và size report

ELF là artifact chính. BIN và HEX là artifact phái sinh để nạp firmware.

```cmake
add_custom_command(TARGET firmware POST_BUILD
    COMMAND ${CMAKE_OBJCOPY}
        -O binary
        $<TARGET_FILE:firmware>
        ${CMAKE_CURRENT_BINARY_DIR}/firmware.bin

    COMMAND ${CMAKE_OBJCOPY}
        -O ihex
        $<TARGET_FILE:firmware>
        ${CMAKE_CURRENT_BINARY_DIR}/firmware.hex

    COMMAND ${CMAKE_SIZE}
        $<TARGET_FILE:firmware>

    BYPRODUCTS
        ${CMAKE_CURRENT_BINARY_DIR}/firmware.bin
        ${CMAKE_CURRENT_BINARY_DIR}/firmware.hex

    VERBATIM
)
```

`$<TARGET_FILE:firmware>` lấy đúng path của ELF target.

---

## XIII. Tạo target flash

```cmake
find_program(OPENOCD openocd)

if(OPENOCD)
    add_custom_target(flash
        COMMAND ${OPENOCD}
            -f interface/stlink.cfg
            -f target/stm32f1x.cfg
            -c "adapter speed 400"
            -c "program $<TARGET_FILE:firmware> verify reset exit"
        DEPENDS firmware
        USES_TERMINAL
        VERBATIM
    )
endif()
```

Chạy:

```bash
cmake --build build/release --target flash
```

Build và flash nên là hai hành động khác nhau. Không để build mặc định tự động
đụng phần cứng.

---

## XIV. Target-based CMake

Tránh cấu hình global nếu không cần:

```cmake
include_directories(...)
add_definitions(...)
add_compile_options(...)
```

Ưu tiên:

```cmake
target_sources(...)
target_include_directories(...)
target_compile_definitions(...)
target_compile_options(...)
target_link_options(...)
target_link_libraries(...)
```

Tư duy chính:

```text
firmware target
   |
   +-- sources
   +-- include paths
   +-- definitions
   +-- compile options
   +-- link options
   +-- libraries
```

Mỗi target chỉ nhận cấu hình nó thực sự cần.

---

## XV. `PRIVATE`, `PUBLIC`, `INTERFACE`

| Từ khóa | Target hiện tại dùng? | Target link nó có kế thừa? |
|---|---:|---:|
| `PRIVATE` | Có | Không |
| `PUBLIC` | Có | Có |
| `INTERFACE` | Không | Có |

Ví dụ driver có public header:

```cmake
add_library(uart_driver STATIC
    Src/uart.c
)

target_include_directories(uart_driver PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
)
```

Firmware link driver:

```cmake
target_link_libraries(firmware PRIVATE
    uart_driver
)
```

### 1. Các loại library hay gặp

| Loại | Khi nên dùng |
|---|---|
| `INTERFACE` | Chỉ chứa include/define/options |
| `OBJECT` | Muốn tái sử dụng object trực tiếp |
| `STATIC` | Module đóng thành `.a` |
| `EXECUTABLE` | Firmware ELF |

Đừng tách project thành nhiều library chỉ để “trông chuyên nghiệp”. Tách khi
module thực sự có source, dependency hoặc ownership riêng.

---

## XVI. Project lớn và nhiều firmware

Root có thể chỉ điều phối:

```cmake
add_subdirectory(BSP)
add_subdirectory(Drivers)
add_subdirectory(App)
```

Bootloader và application nên là hai executable khác nhau:

```cmake
add_executable(bootloader ...)
add_executable(application ...)
```

Mỗi target có linker script riêng vì vector base và memory region khác nhau.

Có thể dùng function để giảm lặp:

```cmake
function(configure_firmware target linker_script)
    set_target_properties(${target} PROPERTIES
        SUFFIX ".elf"
        LINK_DEPENDS "${linker_script}"
    )

    target_link_options(${target} PRIVATE
        -T${linker_script}
        -Wl,--gc-sections
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${target}.map
    )
endfunction()
```

Function nên giảm lặp nhưng không che mất dependency quan trọng.

---

## XVII. CMake Presets

`CMakePresets.json` giúp không phải gõ lại generator, toolchain và build dir.

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "generator": "Ninja",
      "toolchainFile": "${sourceDir}/cmake/arm-none-eabi-toolchain.cmake"
    },
    {
      "name": "debug",
      "inherits": "base",
      "binaryDir": "${sourceDir}/build/debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    },
    {
      "name": "release",
      "inherits": "base",
      "binaryDir": "${sourceDir}/build/release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "debug",
      "configurePreset": "debug"
    },
    {
      "name": "release",
      "configurePreset": "release"
    }
  ]
}
```

Chạy:

```bash
cmake --preset release
cmake --build --preset release
```

Không commit password, private key hoặc path riêng của máy vào preset.

---

## XVIII. Workflow tự viết CMake từ đầu

Đây là phần nên dùng khi bắt đầu một project mới.

### 1. Chốt đầu vào

Trước khi viết CMake, phải biết:

```text
MCU             STM32F103C8T6
CPU             Cortex-M3
startup         startup_stm32f103.s
linker script   stm32f103c8.ld
library         CMSIS/SPL hoặc hệ khác
compiler        arm-none-eabi-gcc
```

### 2. Làm toolchain trước

Xác nhận ARM GCC được tìm thấy và được chọn trước `project()`.

### 3. Tạo firmware target

```cmake
project(... LANGUAGES C ASM)
add_executable(firmware)
```

### 4. Thêm source

Ít nhất:

```text
main.c
startup
system source nếu library cần
```

### 5. Thêm include + define

Nếu compile báo thiếu header -> kiểm tra include path.

Nếu header có nhưng chọn sai device/library branch -> kiểm tra compile definitions.

### 6. Gắn CPU/ABI flags

Compile và link phải thống nhất Cortex-M3/Thumb/soft-float.

### 7. Gắn linker script

Lúc này mới có đủ điều kiện để tạo ELF đúng memory layout.

### 8. Link runtime/library

Chỉ link những gì project cần.

### 9. Build ELF trước

Đừng vội thêm flash. Trước tiên phải có ELF build/link đúng.

### 10. Sinh BIN/HEX/MAP/size

Đây là artifact và thông tin kiểm tra sau link.

### 11. Cuối cùng mới thêm flash target

Build pass không đồng nghĩa firmware chạy đúng trên board.

Flow đầy đủ:

```text
Toolchain
   |
   v
Project + target
   |
   v
Sources
   |
   v
Include + define
   |
   v
Compile flags
   |
   v
Linker script + libraries
   |
   v
ELF
   |
   +--> BIN / HEX
   +--> MAP / size
   |
   v
Flash / board test
```

---

## XIX. Debug CMake/Ninja

Khi build lỗi, nhìn command thật trước:

```bash
cmake --build build/debug --verbose
```

hoặc trong build directory:

```bash
ninja -v
```

Kiểm tra xem command có thật sự chứa:

```text
-mcpu=cortex-m3
-mthumb
-I...
-D...
-T...
```

### 1. Các lỗi thường gặp

| Lỗi | Kiểm tra |
|---|---|
| Host compiler được chọn | Toolchain file, cache build cũ |
| Startup không build | `LANGUAGES C ASM`, source startup |
| `undefined reference` | Thiếu source hoặc library implementation |
| ELF link nhưng địa chỉ sai | Linker script/vector base |
| Sửa `.ld` mà không relink | `LINK_DEPENDS` |
| Header không thấy | `target_include_directories()` |
| Macro device sai | `target_compile_definitions()` |
| BIN/HEX không sinh | Post-build command |
| Build pass nhưng board lỗi | Clock/reset/vector/interrupt/hardware |

Nếu đổi generator hoặc toolchain, nên tạo build directory mới thay vì cố tái sử
dụng cache cũ.

---

## XX. Mẫu `CMakeLists.txt` hoàn chỉnh

Đây là template đối chiếu. Không cần học thuộc; đọc theo từng block.

```cmake
cmake_minimum_required(VERSION 3.22)

project(STM32F103C8_Firmware LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Firmware target
# -----------------------------------------------------------------------------
add_executable(firmware)

target_sources(firmware PRIVATE
    Src/main.c
    startup/startup_stm32f103.s

    Libraries/CMSIS/CM3/DeviceSupport/system_stm32f10x.c
    Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_rcc.c
    Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_gpio.c
)

target_include_directories(firmware PRIVATE
    Inc
    Libraries/CMSIS/CM3/CoreSupport
    Libraries/CMSIS/CM3/DeviceSupport
    Libraries/STM32F10x_StdPeriph_Driver/inc
)

target_compile_definitions(firmware PRIVATE
    STM32F10X_MD
    USE_STDPERIPH_DRIVER
    HSE_VALUE=8000000UL
)

# -----------------------------------------------------------------------------
# MCU / ABI
# -----------------------------------------------------------------------------
set(MCU_FLAGS
    -mcpu=cortex-m3
    -mthumb
    -mfloat-abi=soft
)

target_compile_options(firmware PRIVATE
    ${MCU_FLAGS}
    -ffunction-sections
    -fdata-sections
    -fno-common
    -Wall
    -Wextra

    $<$<CONFIG:Debug>:-Os> # giữ firmware đúng partition; vẫn có -g3
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Release>:-Os>
    $<$<CONFIG:Release>:-g3>
)

# -----------------------------------------------------------------------------
# Linker
# -----------------------------------------------------------------------------
set(LINKER_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32f103c8.ld
)

target_link_options(firmware PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -nostartfiles
    -static
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/firmware.map
)

set_target_properties(firmware PROPERTIES
    SUFFIX ".elf"
    LINK_DEPENDS "${LINKER_SCRIPT}"
)

target_link_libraries(firmware PRIVATE
    "-Wl,--start-group"
    c
    m
    gcc
    "-Wl,--end-group"
)

# -----------------------------------------------------------------------------
# Artifacts
# -----------------------------------------------------------------------------
add_custom_command(TARGET firmware POST_BUILD
    COMMAND ${CMAKE_OBJCOPY}
        -O binary
        $<TARGET_FILE:firmware>
        ${CMAKE_CURRENT_BINARY_DIR}/firmware.bin

    COMMAND ${CMAKE_OBJCOPY}
        -O ihex
        $<TARGET_FILE:firmware>
        ${CMAKE_CURRENT_BINARY_DIR}/firmware.hex

    COMMAND ${CMAKE_SIZE}
        $<TARGET_FILE:firmware>

    BYPRODUCTS
        ${CMAKE_CURRENT_BINARY_DIR}/firmware.bin
        ${CMAKE_CURRENT_BINARY_DIR}/firmware.hex

    VERBATIM
)

# -----------------------------------------------------------------------------
# Flash helper
# -----------------------------------------------------------------------------
find_program(OPENOCD openocd)

if(OPENOCD)
    add_custom_target(flash
        COMMAND ${OPENOCD}
            -f interface/stlink.cfg
            -f target/stm32f1x.cfg
            -c "adapter speed 400"
            -c "program $<TARGET_FILE:firmware> verify reset exit"
        DEPENDS firmware
        USES_TERMINAL
        VERBATIM
    )
endif()
```

Đọc template theo bảy block:

```text
1. Project/language
2. Target + source
3. Include + define
4. Compile policy
5. Link policy
6. Artifact
7. Hardware helper
```

---

## XXI. Quy tắc nên nhớ

1. **Compiler được chọn trong toolchain trước `project()`.**
2. **Ưu tiên target-based CMake.**
3. **Source và include path là hai việc khác nhau.**
4. **CPU/ABI flags phải nhất quán giữa compile và link.**
5. **Linker script quyết định memory layout; CMake chỉ truyền nó cho linker.**
6. **Startup và linker script phải dùng cùng bộ symbol.**
7. **ELF là artifact chính; BIN/HEX là artifact phái sinh.**
8. **Build và flash là hai hành động khác nhau.**
9. **Khi lỗi, xem command thật bằng verbose build trước khi đoán.**
10. **Đừng abstraction quá sớm; file rõ ràng quan trọng hơn file “thông minh”.**

---

## XXII. Checklist cuối

- [ ] Toolchain được chọn trước `project()`.
- [ ] Project bật C và ASM.
- [ ] Source được liệt kê tường minh.
- [ ] Include path đúng.
- [ ] Device/library macro đúng.
- [ ] Cortex-M3 + Thumb + soft-float có ở compile và link.
- [ ] Startup và linker script khớp symbol.
- [ ] Linker script đúng vùng Flash/RAM của project.
- [ ] Sửa linker script làm firmware relink.
- [ ] Có ELF, BIN, HEX, MAP và size report.
- [ ] Flash là target riêng.
- [ ] Không sửa `build.ninja`.
- [ ] Build pass không được coi là board test pass.

---

## XXIII. Cách đọc một `CMakeLists.txt` lạ

Đi theo thứ tự:

```text
Target nào được tạo?
  -> source nào được compile?
  -> include và define nào được áp dụng?
  -> compile options nào được dùng?
  -> linker options và linker script nào?
  -> target link library nào?
  -> sau link sinh artifact gì?
```

Trả lời được chuỗi này là đã hiểu phần lớn file CMake đó.

---

## XXIV. Kết luận

CMake bare-metal không phải nơi “nhét flag cho build qua”. Nó là mô tả build graph:

```text
source + configuration
        |
        v
      target
        |
        v
       ELF
        |
        +--> BIN / HEX
        +--> MAP / size
        +--> flash/debug workflow
```

Nếu luôn nghĩ theo target và ba giai đoạn **compile -> link -> post-build**, việc
viết và debug CMake cho STM32 sẽ dễ theo dõi hơn rất nhiều.
