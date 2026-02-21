//
// Created by inż.Dawid Pisarczyk on 14.02.2026.
//

#ifndef RS485_H
#define RS485_H

#include <stdio.h>
#include <zephyr/logging/log.h>
#include "uart.h"
#include "gpio.h"

#define RS485_RX_BUF_SIZE 256

extern struct k_mutex tx_mutex;

int rs485_init(const struct device* dev, struct gpio_dt_spec *dir);
int rs485_send(const struct device* dev, struct gpio_dt_spec *dir, const uint8_t *data, size_t len);
int rs485_set_tx(struct gpio_dt_spec *gpio);
int rs485_set_rx(struct gpio_dt_spec *gpio);

#endif //RS485_H
