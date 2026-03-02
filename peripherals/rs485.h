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

#define CCU_RS485_TX_THREAD_STACK_SIZE 512
#define CCU_RS485_TX_THREAD_PRIORITY 5

typedef struct {
    const struct device *rs485_device;
    struct gpio_dt_spec rs485_tx_led;
    struct gpio_dt_spec rs485_rx_led;
    struct gpio_dt_spec rs485_dir;
} ccu_rs485_t;

extern struct k_mutex tx_mutex;

int rs485_set_tx(struct gpio_dt_spec *gpio);
int rs485_set_rx(struct gpio_dt_spec *gpio);

int rs485_send(const struct device *dev, struct gpio_dt_spec *dir, const uint8_t *data, size_t len);
void ccu_rs485_tx_thread(void *p1, void *p2, void *p3);

int rs485_init(const struct device *dev, struct gpio_dt_spec *dir);
void ccu_rs485_init();

#endif //RS485_H
