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
#include "rs485.h"

#define CAN_FILTER(_id)                                                        \
  { .id = (_id), .mask = CAN_STD_ID_MASK, .flags = 0U }

typedef struct {
  const struct device *device;
  struct gpio_dt_spec tx_led;
  struct gpio_dt_spec rx_led;
} ccu_can_t;

typedef struct {
  const struct device *device;
  struct gpio_dt_spec dir_pin;
  struct gpio_dt_spec tx_led;
  struct gpio_dt_spec rx_led;
  uint8_t rx_buf[RS485_RX_BUF_SIZE];
  uint8_t rx_buf_next[RS485_RX_BUF_SIZE];
}ccu_rs485_t;

void ccu_can_init();
void can_converter_init();
void can_converter_tick();

#endif // CAN_CONVERTER_H
