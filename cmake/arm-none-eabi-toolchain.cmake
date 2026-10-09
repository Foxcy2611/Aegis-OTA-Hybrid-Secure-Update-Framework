# =============================================================================
# MỤC ĐÍCH CỦA FILE NÀY:
# - Chọn ARM GNU Toolchain trước khi lệnh project() của CMake được chạy.
# - Khai báo target là hệ thống bare-metal ARM, không phải chương trình Win/Linux.
# - Cung cấp compiler và binutils cho C, Assembly, archive và firmware artifact.
#
# File này chỉ thiết lập môi trường cross-compile. Cờ riêng của STM32F103 được
# đặt trong stm32f103_options.cmake, không đặt tại đây.
# =============================================================================

# Target là hệ thống bare-metal, không run trên Win hay Linux
set(CMAKE_SYSTEM_NAME Generic) # Thông báo đây là bare-metal
set(CMAKE_SYSTEM_PROCESSOR arm)

# Tìm ARM GNU Toolchain trong PATH WSL
# ARM_GCC sẽ là link dẫn tới toolchain
find_program(ARM_GCC
    arm-none-eabi-gcc
    REQUIRED
)

find_program(ARM_AR
    arm-none-eabi-ar
    REQUIRED
)

find_program(ARM_RANLIB
    arm-none-eabi-ranlib
    REQUIRED
)

find_program(ARM_OBJCOPY
    arm-none-eabi-objcopy
    REQUIRED
)

find_program(ARM_OBJDUMP
    arm-none-eabi-objdump
    REQUIRED
)

find_program(ARM_SIZE
    arm-none-eabi-size
    REQUIRED
)

find_program(ARM_NM
    arm-none-eabi-nm
    REQUIRED
)

# Compiler cho source C và startup Assembly.
# Chọn ARM GCC cho C và Assembly
set(CMAKE_C_COMPILER
    "${ARM_GCC}"
    CACHE FILEPATH "ARM C compiler"
)

set(CMAKE_ASM_COMPILER
    "${ARM_GCC}"
    CACHE FILEPATH "ARM Assembly compiler"
)

# Các công cụ binutils dùng khi archive và xử lý firmware.
set(CMAKE_AR
    "${ARM_AR}"
    CACHE FILEPATH "ARM archiver"
)

set(CMAKE_RANLIB
    "${ARM_RANLIB}"
    CACHE FILEPATH "ARM archive indexer"
)

set(CMAKE_OBJCOPY
    "${ARM_OBJCOPY}"
    CACHE FILEPATH "ARM objcopy"
)

set(CMAKE_OBJDUMP
    "${ARM_OBJDUMP}"
    CACHE FILEPATH "ARM objdump"
)

set(CMAKE_SIZE
    "${ARM_SIZE}"
    CACHE FILEPATH "ARM size tool"
)

set(CMAKE_NM
    "${ARM_NM}"
    CACHE FILEPATH "ARM symbol tool"
)

# Ngăn CMake thử link 1 chương trình desktop trong bước kiểm compiler
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Không tìm header/library/program của máy host như dependency của firmware.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
