# Project ESP-IDF độc lập cho ESP32 Gateway Controller.
# Không include project này vào build tree ARM của STM32.

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(aegis_esp32_gateway)
