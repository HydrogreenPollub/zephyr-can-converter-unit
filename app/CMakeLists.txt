cmake_minimum_required(VERSION 3.13.1)

if(NOT DEFINED BOARD)
    set(BOARD steering_wheel CACHE STRING "Zephyr target board")
    message(STATUS "BOARD not defined externally, using default: ${BOARD}")
endif()

if(NOT DEFINED BOARD_ROOT)
    set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../ CACHE PATH "Root directory for custom boards")
    message(STATUS "BOARD_ROOT not defined externally, using default: ${BOARD_ROOT}")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app LANGUAGES C)

target_sources(app PRIVATE
        src/main.c
        src/can_converter.c
        peripherals/can.c
        peripherals/gpio.c
        peripherals/adc.c
        peripherals/pwm.c
        peripherals/counter.c
)

target_include_directories(app PRIVATE
        peripherals
        lib-can-definitions
        src
)