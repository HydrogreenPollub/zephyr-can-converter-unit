//
// Created by User on 28.12.2025.
//

#ifndef CAN_CONVERTER_H
#define CAN_CONVERTER_H

#include "can.h"
#include "can_ids.h"
#include "gpio.h"
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define DEBOUNCE_MS 20

typedef struct {
  const struct device *can_device;
  struct gpio_dt_spec can_tx_led;
  struct gpio_dt_spec can_rx_led;
} ccu_can_t;

void can_converter_init();

#endif // CAN_CONVERTER_H
