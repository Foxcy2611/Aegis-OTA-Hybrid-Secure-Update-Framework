# =============================================================================
# MỤC ĐÍCH CỦA FILE NÀY:
# - Tạo firmware target aegis_application đặt sau vùng Bootloader.
# - Khai báo source/include thuộc Application và dependency dùng chung cần thiết.
# - Gắn linker script Application rồi sinh ELF/BIN/HEX/MAP/LST qua helper chung.
# =============================================================================

set(AEGIS_APPLICATION_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/active_firmware.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/application_health.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/artifact_storage.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/delta_storage.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/delta_apply.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/secure_transport.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/secure_frame.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/transport_session_store.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ota_transport.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ota_update_agent.c"
)

add_executable(aegis_application
    ${AEGIS_APPLICATION_SOURCES}
)

target_link_options(aegis_application PRIVATE
    $<$<CONFIG:Debug>:-flto>
)

target_compile_options(aegis_application PRIVATE
    $<$<CONFIG:Debug>:-flto>
)

target_include_directories(aegis_application PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/inc"
)

target_link_libraries(aegis_application PRIVATE
    Aegis::Shared
    Aegis::Common
    Aegis::Driver
    Aegis::Monocypher
    Aegis::Stm32Runtime
    Aegis::SPL
    aegis_warnings
)

aegis_configure_firmware(
    aegis_application
    "${CMAKE_CURRENT_SOURCE_DIR}/linker_application.ld"
)
