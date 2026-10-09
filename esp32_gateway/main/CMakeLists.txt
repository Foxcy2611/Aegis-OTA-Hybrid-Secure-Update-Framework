# Entry point bring-up Phase 4: packet thô, radio task/queue và phép đo P2P.

idf_component_register(
    SRCS
        "app_main.c"
        "ota_gateway.c"
        "phase4_link_packet.c"
        "radio_task.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        as32
        ota_sender
        esp_timer
)
