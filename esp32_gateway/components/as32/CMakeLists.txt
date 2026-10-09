# Component adapter mức thấp UART/MD0/MD1/AUX cho AS32 phía ESP32.

idf_component_register(
    SRCS
        "as32_adapter.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_driver_gpio
        esp_driver_uart
        esp_timer
)
