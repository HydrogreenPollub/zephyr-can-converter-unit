/**
 * @file can_converter_can.h
 * @brief CAN transmit path for the CAN converter.
 *
 * Owns the CAN context, the TX queue, the TX thread, the periodic data
 * broadcast thread, and bus-off recovery logic.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "can_converter.h"

typedef struct {
    const struct device *device;
    struct gpio_dt_spec  tx_led;
    struct gpio_dt_spec  rx_led;
} ccu_can_t;

extern ccu_can_t        can;
extern struct can_filter ccu_can_filter;
extern struct k_msgq     can_tx_msgq;

/**
 * @brief Initialise the CAN subsystem and start TX and periodic threads.
 */
void ccu_can_init(void);
void ccu_can_test_set(bool active);
void ccu_can_test_send_all(void);

#ifdef __cplusplus
}
#endif
