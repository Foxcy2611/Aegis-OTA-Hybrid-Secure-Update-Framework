cmake_minimum_required(VERSION 3.22)

project(AegisPhase4HostTests LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

get_filename_component(
    AEGIS_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../.."
    ABSOLUTE
)

add_library(phase4_production_logic STATIC
    "${AEGIS_ROOT}/driver/src/lora_as32_protocol.c"
    "${AEGIS_ROOT}/common/src/crc32.c"
    "${AEGIS_ROOT}/esp32_gateway/main/phase4_link_packet.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../phase4_packet.c"
)

target_include_directories(phase4_production_logic PUBLIC
    "${AEGIS_ROOT}/driver/inc"
    "${AEGIS_ROOT}/common/inc"
    "${AEGIS_ROOT}/esp32_gateway/main"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_compile_options(phase4_production_logic PRIVATE
    -Wall
    -Wextra
    -Werror
)

add_executable(phase4_lora_host_tests
    phase4_lora_host_tests.c
)

target_compile_options(phase4_lora_host_tests PRIVATE
    -Wall
    -Wextra
    -Werror
)

target_link_libraries(phase4_lora_host_tests PRIVATE
    phase4_production_logic
)

enable_testing()

add_test(
    NAME phase4_lora_contract
    COMMAND phase4_lora_host_tests
)
