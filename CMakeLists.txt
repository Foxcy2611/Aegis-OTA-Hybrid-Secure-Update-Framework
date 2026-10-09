# Tác dụng:
# - Là entry point của build system STM32.
# - Khai báo project, ngôn ngữ và chuẩn C dùng chung.
# - Nạp cấu hình STM32F103 và các firmware helper.
# - Đi vào Shared, Bootloader và Application để tạo target.
# - Tạo các target tổng dùng để build cả hai firmware.
#
# File này không chọn compiler.
# ARM compiler phải được chọn trước project() bằng:
# cmake/arm-none-eabi-toolchain.cmake hoặc CMakePresets.json.

cmake_minimum_required(VERSION 3.22)

project(
    AegisOTA
    VERSION 1.0.0
    DESCRIPTION "Hybrid Secure OTA Framework for STM32F103C8T6"
    LANGUAGES C ASM
)

# Sử dụng chuẩn C11 thuần
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Sinh compile_commmands.json cho clangd, IDE và static analysis
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build STM32 của project sử dụng Ninja làm generator canonical
if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
    message(FATAL_ERROR
        "STM32 build phải sử dụng Ninja. "
        "Hãy configure với '-G Ninja' hoặc dùng CMakePresets.json."
    )
endif()

# Nếu người dùng không chọn Debug hoặc Release thì mặc định dùng Debug
if(NOT CMAKE_BUILD_TYPE)
    set(
        CMAKE_BUILD_TYPE
        Debug
        CACHE STRING 
        "Build type: Debug hoặc Release"
        FORCE
    )
endif()

# Project chỉ duy trì hai profile firmware chính thức.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND
   NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR
        "CMAKE_BUILD_TYPE phải là Debug hoặc Release, "
        "hiện tại là '${CMAKE_BUILD_TYPE}'."
    )
endif()

# Giới hạn các build type hợp lệ hiển thị trong CMAKE GUI/IDE
set_property(
    CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS 
        Debug
        Release
)

# Cho phép include file trong cmake/ bằng tên module
list(APPEND CMAKE_MODULE_PATH
    "${PROJECT_SOURCE_DIR}/cmake"
)

# Tạo các target cấu hình chung cho STM32F103C8T6
include(stm32f103_options)

# Cung cấp function gắn linker script và sinh artifact firmware
include(firmware_helpers)

# Thư viện hãng và runtime MCU được tạo trước các firmware.
add_subdirectory(libraries)

# Shared được tạo trước vì cả Boot và App đều dùng nó.
add_subdirectory(shared)

# Common sở hữu startup và CMSIS runtime dùng chung.
add_subdirectory(common)

# Primitive driver dùng chung cho Bootloader và Application.
add_subdirectory(driver)

# Add thêm các CMake con trong Boot và App
add_subdirectory(bootloader)
add_subdirectory(application)

# Board regression không thuộc target firmware mặc định.
add_subdirectory(
    test/Test_to_Phase/Phase4_LoRa
    EXCLUDE_FROM_ALL
)

# Target tổng để build cả Boot và App
add_custom_target(firmware
    DEPENDS
        aegis_bootloader
        aegis_application
)

# Target tổng để in kích thước 2 firm
add_custom_target(size
    DEPENDS
        aegis_bootloader_size
        aegis_application_size
)

# Target tổng để tạo disassembly của hai firmware.
add_custom_target(disasm
    DEPENDS
        aegis_bootloader_disasm
        aegis_application_disasm
)

# Target tổng để flash riêng từng image khi OpenOCD có sẵn.
if(TARGET aegis_bootloader_flash)
    add_custom_target(flash_bootloader
        DEPENDS aegis_bootloader_flash
    )
endif()

if(TARGET aegis_application_flash)
    add_custom_target(flash_application
        DEPENDS aegis_application_flash
    )
endif()

# Hiển thị cấu hình đang được sử dụng.
message(STATUS "Project           : ${PROJECT_NAME}")
message(STATUS "Version           : ${PROJECT_VERSION}")
message(STATUS "Generator         : ${CMAKE_GENERATOR}")
message(STATUS "Build type        : ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler        : ${CMAKE_C_COMPILER}")
message(STATUS "ASM compiler      : ${CMAKE_ASM_COMPILER}")
message(STATUS "Source directory  : ${PROJECT_SOURCE_DIR}")
message(STATUS "Build directory   : ${PROJECT_BINARY_DIR}")
