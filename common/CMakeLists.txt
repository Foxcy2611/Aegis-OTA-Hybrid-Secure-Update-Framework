# =============================================================================
# MỤC ĐÍCH CỦA FILE NÀY:
# - Đóng gói startup STM32F103 medium-density và CMSIS system runtime dùng chung.
# - Bảo đảm mỗi firmware nhận đúng vector table, Reset_Handler và SystemInit.
# - Tránh lặp lại danh sách source nền tảng trong Bootloader và Application.
# =============================================================================

add_library(aegis_stm32_runtime OBJECT
    "${CMAKE_CURRENT_SOURCE_DIR}/startup_stm32f103_md.s"
    "${PROJECT_SOURCE_DIR}/libraries/CMSIS/CM3/DeviceSupport/system_stm32f10x.c"
)

target_compile_options(aegis_stm32_runtime PRIVATE
    $<$<CONFIG:Debug>:-Os>
    $<$<CONFIG:Debug>:-flto>
)

add_library(Aegis::Stm32Runtime ALIAS aegis_stm32_runtime)

target_include_directories(aegis_stm32_runtime PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/inc"
)

target_link_libraries(aegis_stm32_runtime PUBLIC
    stm32f103_options
)

# Thuật toán/logic thuần dùng chung, không sở hữu peripheral hoặc địa chỉ Flash.
add_library(aegis_common STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/crc32.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/device_keys.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/delta_auth.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/firmware_auth.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/metadata_record.c"
)

target_compile_options(aegis_common PRIVATE
    $<$<CONFIG:Debug>:-Os>
    $<$<CONFIG:Debug>:-flto>
)

add_library(Aegis::Common ALIAS aegis_common)

target_include_directories(aegis_common PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/inc"
)

target_link_libraries(aegis_common PUBLIC
    Aegis::Shared
    stm32f103_options
    aegis_warnings
)

target_link_libraries(aegis_common PRIVATE
    Aegis::Monocypher
)
