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
project(app LANGUAGES C CXX)

target_include_directories(app PRIVATE
        peripherals
        can-definitions
        src
        hmi_parser
)

target_sources(app PRIVATE
        src/main.cpp
        src/can_converter.cpp
        src/can_converter_can.cpp
        src/can_converter_rs485.cpp
        peripherals/can.c
        peripherals/gpio.c
        peripherals/adc.c
        peripherals/pwm.c
        peripherals/counter.c
        peripherals/uart.c
        peripherals/rs485.c
)

target_compile_features(app PRIVATE cxx_std_17)