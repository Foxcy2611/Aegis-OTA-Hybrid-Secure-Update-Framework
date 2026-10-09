cmake_minimum_required(VERSION 3.22)
project(AegisPhase5To8Host LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

get_filename_component(AEGIS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(MONOCYPHER "${AEGIS_ROOT}/libraries/Third_Party/Monocypher")

add_library(phase5_8_production STATIC
    "${AEGIS_ROOT}/application/src/delta_apply.c"
    "${AEGIS_ROOT}/application/src/ota_transport.c"
    "${AEGIS_ROOT}/application/src/secure_frame.c"
    "${AEGIS_ROOT}/application/src/secure_transport.c"
    "${AEGIS_ROOT}/bootloader/src/boot_state_machine.c"
    "${AEGIS_ROOT}/common/src/crc32.c"
    "${AEGIS_ROOT}/common/src/metadata_record.c"
    "${AEGIS_ROOT}/esp32_gateway/components/ota_sender/ota_sender.c"
    "${MONOCYPHER}/monocypher.c"
)
target_include_directories(phase5_8_production PUBLIC
    "${AEGIS_ROOT}/application/inc"
    "${AEGIS_ROOT}/bootloader/inc"
    "${AEGIS_ROOT}/common/inc"
    "${AEGIS_ROOT}/esp32_gateway/components/ota_sender/include"
    "${AEGIS_ROOT}/shared"
    "${MONOCYPHER}"
)
target_compile_options(phase5_8_production PRIVATE -Wall -Wextra -Werror)

add_executable(phase5_8_tests phase5_8_tests.c)
target_compile_options(phase5_8_tests PRIVATE -Wall -Wextra -Werror)
target_link_libraries(phase5_8_tests PRIVATE phase5_8_production)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
enable_testing()
add_test(NAME phase5_8_contracts COMMAND phase5_8_tests)
add_test(
    NAME phase7_tools
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test_tools.py"
            "${AEGIS_ROOT}"
)
set_tests_properties(phase7_tools PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
