cmake_minimum_required(VERSION 3.13.1)
if(NOT DEFINED BOARD)
    set(BOARD can_converter CACHE STRING "Zephyr target board")
    message(STATUS "BOARD not defined externally, using default: ${BOARD}")
endif()

if(NOT DEFINED BOARD_ROOT)
    set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/ CACHE PATH "Root directory for custom boards")
    message(STATUS "BOARD_ROOT not defined externally, using default: ${BOARD_ROOT}")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)
zephyr_nanopb_sources(app proto/master.proto)

target_include_directories(app PRIVATE
        peripherals
        can-definitions
        src
)

target_sources(app PRIVATE
        src/main.c
        src/can_converter.c
        src/can_converter_can.c
        src/can_converter_can_send.c
        src/can_converter_rs485.c
        src/cobs.c
        peripherals/can.c
        peripherals/gpio.c
        peripherals/adc.c
        peripherals/pwm.c
        peripherals/counter.c
        peripherals/uart.c
        peripherals/rs485.c
        can-definitions/candef.c
)
