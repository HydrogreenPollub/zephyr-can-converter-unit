//
// Created by User on 08.03.2026.
//

#ifndef CAN_CONVERTER_CAN_H
#define CAN_CONVERTER_CAN_H

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
#include "can_converter.hpp"

#define CAN_FILTER(_id)                                                        \
{ .id = (_id), .mask = CAN_STD_ID_MASK, .flags = 0U }

typedef struct {
    const struct device *device;
    struct gpio_dt_spec tx_led;
    struct gpio_dt_spec rx_led;
} ccu_can_t;

void ccu_can_init();

#endif //CAN_CONVERTER_CAN_H
