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


void can_converter_init();

#endif // CAN_CONVERTER_H
