# =============================================================================
# MỤC ĐÍCH CỦA FILE NÀY:
# - Tạo firmware target aegis_bootloader đặt tại đầu Internal Flash.
# - Chỉ khai báo source production trong bootloader/src và bootloader/inc.
# - Không đưa source đối chiếu trong bootloader/Backup vào bản build chính.
# =============================================================================

set(AEGIS_BOOTLOADER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/flash_program.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/boot_metadata.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/boot_state_machine.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/boot_update.c"
)

add_executable(aegis_bootloader
    ${AEGIS_BOOTLOADER_SOURCES}
)

# Ed25519ph + recovery pipeline phải luôn fit 16 KB, kể cả preset Debug.
# Vẫn giữ -g3 từ profile chung; chỉ tối ưu size và bật LTO cho image này.
target_compile_options(aegis_bootloader PRIVATE
    $<$<CONFIG:Debug>:-Os>
    $<$<CONFIG:Debug>:-flto>
)
target_link_options(aegis_bootloader PRIVATE
    $<$<CONFIG:Debug>:-flto>
)

target_include_directories(aegis_bootloader PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/inc"
)

target_link_libraries(aegis_bootloader PRIVATE
    Aegis::Shared
    Aegis::Common
    Aegis::Driver
    Aegis::Stm32Runtime
    Aegis::SPL
    aegis_warnings
)

aegis_configure_firmware(
    aegis_bootloader
    "${CMAKE_CURRENT_SOURCE_DIR}/linker_bootloader.ld"
)
