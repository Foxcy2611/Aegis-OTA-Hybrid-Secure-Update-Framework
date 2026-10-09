# =============================================================================
# MỤC ĐÍCH CỦA FILE NÀY:
# - Đóng gói STM32F10x Standard Peripheral Library thành một static library.
# - Đóng gói Monocypher 4.0.3 nguyên trạng thành crypto static library.
# - Giữ danh sách source thư viện hãng tại một nơi, không lặp trong từng firmware.
# - Linker chỉ lấy những module SPL/crypto thực sự được firmware gọi.
# =============================================================================

set(STM32F10X_SPL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/misc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/stm32f10x_adc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/stm32f10x_bkp.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/stm32f10x_dma.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/stm32f10x_exti.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/stm32f10x_flash.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/stm32f10x_gpio.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/stm32f10x_i2c.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/stm32f10x_iwdg.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/stm32f10x_pwr.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/stm32f10x_rcc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/stm32f10x_rtc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/stm32f10x_sdio.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/stm32f10x_spi.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/stm32f10x_tim.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F10x_StdPeriph_Driver/src/stm32f10x_usart.c"
)

add_library(aegis_stm32f10x_spl STATIC
    ${STM32F10X_SPL_SOURCES}
)

target_compile_options(aegis_stm32f10x_spl PRIVATE
    $<$<CONFIG:Debug>:-Os>
    $<$<CONFIG:Debug>:-flto>
)

add_library(Aegis::SPL ALIAS aegis_stm32f10x_spl)

target_link_libraries(aegis_stm32f10x_spl PUBLIC
    stm32f103_options
)

# =============================================================================
# MONOCYPHER 4.0.3
# =============================================================================
# Source trong Third_Party là vendor source bất biến:
# - Không format hoặc sửa để theo style của project.
# - Không áp aegis_warnings/-Werror dành cho code tự viết.
# - Header được đánh dấu SYSTEM để warning bên trong vendor header không truyền
#   sang Application/Bootloader.
#
# monocypher.c cung cấp ChaCha20-Poly1305 và crypto_wipe.
# monocypher-ed25519.c cung cấp SHA-512 và Ed25519/Ed25519ph compatibility API.

set(AEGIS_MONOCYPHER_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/Third_Party/Monocypher"
)

set(AEGIS_MONOCYPHER_SOURCES
    "${AEGIS_MONOCYPHER_DIR}/monocypher.c"
    "${AEGIS_MONOCYPHER_DIR}/monocypher-ed25519.c"
)

set(AEGIS_MONOCYPHER_HEADERS
    "${AEGIS_MONOCYPHER_DIR}/monocypher.h"
    "${AEGIS_MONOCYPHER_DIR}/monocypher-ed25519.h"
)

foreach(required_file
    ${AEGIS_MONOCYPHER_SOURCES}
    ${AEGIS_MONOCYPHER_HEADERS}
    "${AEGIS_MONOCYPHER_DIR}/LICENCE.md"
    "${AEGIS_MONOCYPHER_DIR}/VERSION"
    "${AEGIS_MONOCYPHER_DIR}/UPSTREAM.md"
)
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Thiếu file Monocypher đã pin: ${required_file}"
        )
    endif()
endforeach()

# Khóa byte-for-byte bốn source đã đối chiếu với archive chính thức 4.0.3.
# CMake configure phải fail nếu vendor source bị sửa, format hoặc thay version.
function(aegis_verify_monocypher_sha512 file_name expected_sha512)
    set(file_path "${AEGIS_MONOCYPHER_DIR}/${file_name}")
    file(SHA512 "${file_path}" actual_sha512)

    if(NOT actual_sha512 STREQUAL expected_sha512)
        message(FATAL_ERROR
            "Monocypher vendor source không còn nguyên trạng: ${file_name}\n"
            "Expected SHA-512: ${expected_sha512}\n"
            "Actual SHA-512  : ${actual_sha512}"
        )
    endif()
endfunction()

aegis_verify_monocypher_sha512(
    "monocypher.c"
    "9618a9261b9a2295a3729efbc3f1c1146abbe3f1c627766262c9099879495652c74582a3ffe355f274707ea574131852d5b69f1a14a6a2be77b165251fcc87a5"
)

aegis_verify_monocypher_sha512(
    "monocypher.h"
    "09a2d4015d4b8ef319a11beeed5470b49455eeec5056b96720da9ba3d6108dae1566358b56f87001de2f493f248ae1014fe402000c3e4eea7a80a639f2c8f453"
)

aegis_verify_monocypher_sha512(
    "monocypher-ed25519.c"
    "6f37e57f02d4aa8eb475f6fdb1d127d066a8dd86b0868b31c58e5682ca685c6d1e2e3c9e4dc58cad655518d3ce97c298321b9f49f7608d0ee7ef2b2fbc065fab"
)

aegis_verify_monocypher_sha512(
    "monocypher-ed25519.h"
    "18daec3d437ae86bff6726aed1b3170a89342f27b863ca38f44cce23d39ad64945900149f5c73f481f7b7e0c5320955e4724b1b680b091b94bdd392f8c66d8f4"
)

file(READ "${AEGIS_MONOCYPHER_DIR}/VERSION" AEGIS_MONOCYPHER_VERSION)
string(STRIP "${AEGIS_MONOCYPHER_VERSION}" AEGIS_MONOCYPHER_VERSION)

if(NOT AEGIS_MONOCYPHER_VERSION STREQUAL "4.0.3")
    message(FATAL_ERROR
        "Monocypher phải giữ đúng version 4.0.3; "
        "đang thấy '${AEGIS_MONOCYPHER_VERSION}'."
    )
endif()

add_library(aegis_monocypher STATIC
    ${AEGIS_MONOCYPHER_SOURCES}
    ${AEGIS_MONOCYPHER_HEADERS}
)

target_compile_options(aegis_monocypher PRIVATE
    $<$<CONFIG:Debug>:-Os>
    $<$<CONFIG:Debug>:-flto>
)

add_library(Aegis::Monocypher ALIAS aegis_monocypher)

target_include_directories(aegis_monocypher SYSTEM PUBLIC
    "${AEGIS_MONOCYPHER_DIR}"
)

target_link_libraries(aegis_monocypher PRIVATE
    stm32f103_options
)

message(STATUS
    "Monocypher        : ${AEGIS_MONOCYPHER_VERSION} (vendored, unmodified)"
)
