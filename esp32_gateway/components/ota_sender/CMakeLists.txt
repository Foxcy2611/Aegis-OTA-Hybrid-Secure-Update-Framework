get_filename_component(AEGIS_ROOT "${COMPONENT_DIR}/../../.." ABSOLUTE)

idf_component_register(
    SRCS
        "ota_sender.c"
        "${AEGIS_ROOT}/application/src/ota_transport.c"
        "${AEGIS_ROOT}/libraries/Third_Party/Monocypher/monocypher.c"
    INCLUDE_DIRS
        "include"
        "${AEGIS_ROOT}/application/inc"
        "${AEGIS_ROOT}/shared"
        "${AEGIS_ROOT}/libraries/Third_Party/Monocypher"
)
