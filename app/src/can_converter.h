//
// Created by inż.Dawid Pisarczyk on 28.12.2025.
//

#ifndef CAN_CONVERTER_H
#define CAN_CONVERTER_H

#include <stdint.h>
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/can.h>

#include "can.h"
#include "can_ids.h"
#include "gpio.h"

#define CAN_FILTER(_id)                                                        \
  { .id = (_id), .mask = CAN_STD_ID_MASK, .flags = 0U }

typedef struct {
  const struct device *can_device;
  struct gpio_dt_spec can_tx_led;
  struct gpio_dt_spec can_rx_led;
} ccu_can_t;

typedef struct {
  const struct device *rs485_device;
  struct gpio_dt_spec rs485_tx_led;
  struct gpio_dt_spec rs485_rx_led;
}ccu_rs485_t;

void can_converter_init();

#endif // CAN_CONVERTER_H
