cmake_minimum_required(VERSION 3.22)

project(AegisPhase3HostTests LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

get_filename_component(
    AEGIS_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../.."
    ABSOLUTE
)

set(
    AEGIS_MONOCYPHER_DIR
    "${AEGIS_ROOT}/libraries/Third_Party/Monocypher"
)

add_library(phase3_monocypher STATIC
    "${AEGIS_MONOCYPHER_DIR}/monocypher.c"
    "${AEGIS_MONOCYPHER_DIR}/monocypher-ed25519.c"
)

target_include_directories(phase3_monocypher SYSTEM PUBLIC
    "${AEGIS_MONOCYPHER_DIR}"
)

add_library(phase3_production STATIC
    "${AEGIS_ROOT}/application/src/secure_transport.c"
    "${AEGIS_ROOT}/common/src/crc32.c"
    "${AEGIS_ROOT}/common/src/firmware_auth.c"
)

target_include_directories(phase3_production PUBLIC
    "${AEGIS_ROOT}/application/inc"
    "${AEGIS_ROOT}/common/inc"
    "${AEGIS_ROOT}/shared"
)

target_compile_options(phase3_production PRIVATE
    -Wall
    -Wextra
    -Werror
)

target_link_libraries(phase3_production PUBLIC
    phase3_monocypher
)

add_executable(phase3_crypto_tests
    phase3_crypto_tests.c
)

target_compile_options(phase3_crypto_tests PRIVATE
    -Wall
    -Wextra
    -Werror
)

target_link_libraries(phase3_crypto_tests PRIVATE
    phase3_production
)

add_executable(phase3_artifact_verify
    phase3_artifact_verify.c
)

target_compile_options(phase3_artifact_verify PRIVATE
    -Wall
    -Wextra
    -Werror
)

target_link_libraries(phase3_artifact_verify PRIVATE
    phase3_production
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

enable_testing()

add_test(
    NAME phase3_crypto_contract
    COMMAND phase3_crypto_tests
)

add_test(
    NAME phase3_signer_cross_language
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_phase3_signer.py"
        --signer
        "${AEGIS_ROOT}/tools/sign_firmware.py"
        --verifier
        "$<TARGET_FILE:phase3_artifact_verify>"
)

set_tests_properties(
    phase3_signer_cross_language
    PROPERTIES
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
)
