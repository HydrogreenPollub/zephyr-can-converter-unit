//
// Created by User on 08.03.2026.
//

#ifndef CAN_CONVERTER_RS485_H
#define CAN_CONVERTER_RS485_H

#include <stdint.h>
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "gpio.h"
#include "rs485.h"
#include "can_converter.h"

typedef struct {
    const struct device *device;
    struct gpio_dt_spec dir_pin;
    struct gpio_dt_spec tx_led;
    struct gpio_dt_spec rx_led;
    uint8_t rx_buf[RS485_RX_BUF_SIZE];
    uint8_t rx_buf_next[RS485_RX_BUF_SIZE];
} ccu_rs485_t;

typedef struct {
    uint8_t data[64];
    uint8_t len;
} rs485_packet_t;


void ccu_rs485_init();

#endif //CAN_CONVERTER_RS485_H
