# =============================================================================
# MỤC ĐÍCH CỦA FILE NÀY:
# - Biên dịch các driver phần cứng dùng chung của STM32.
# - Cung cấp target Aegis::Driver cho Bootloader và Application.
# - Driver chỉ chứa primitive peripheral, không chứa OTA policy.
# =============================================================================

add_library(aegis_driver STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/boot_mailbox_io.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ext_flash.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lora_as32.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lora_as32_protocol.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/timebase.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/watchdog.c"
)

target_compile_options(aegis_driver PRIVATE
    $<$<CONFIG:Debug>:-Os>
    $<$<CONFIG:Debug>:-flto>
)

add_library(Aegis::Driver ALIAS aegis_driver)

target_include_directories(aegis_driver PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/inc"
)

target_link_libraries(aegis_driver PRIVATE
    Aegis::Shared
    Aegis::Common
    Aegis::SPL
    stm32f103_options
    aegis_warnings
)
