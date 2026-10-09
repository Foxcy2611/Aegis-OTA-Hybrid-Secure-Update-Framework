# Board target Phase 4; được root CMake add dưới dạng EXCLUDE_FROM_ALL.

add_executable(aegis_phase4_lora_board_test
    "${CMAKE_CURRENT_SOURCE_DIR}/board_main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/phase4_packet.c"
)

target_include_directories(aegis_phase4_lora_board_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(aegis_phase4_lora_board_test PRIVATE
    Aegis::Shared
    Aegis::Common
    Aegis::Driver
    Aegis::Stm32Runtime
    Aegis::SPL
    aegis_warnings
)

aegis_configure_firmware(
    aegis_phase4_lora_board_test
    "${PROJECT_SOURCE_DIR}/bootloader/linker_bootloader.ld"
)
