#ifndef CAN_H
#define CAN_H

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "can_ids.h"
#include <stdio.h>

#define CCU_CAN_TX_THREAD_STACK_SIZE 512
#define CCU_CAN_TX_THREAD_PRIORITY 5

#define CAN_FILTER(_id)                                                        \
{ .id = (_id), .mask = CAN_STD_ID_MASK, .flags = 0U }

typedef struct {
    const struct device *can_device;
    struct gpio_dt_spec can_tx_led;
    struct gpio_dt_spec can_rx_led;
} ccu_can_t;

int can_send_(const struct device *can_dev, uint16_t id, uint8_t *data, uint8_t data_len);
int can_send_float(const struct device *can_dev, uint16_t id, float value);
int can_add_rx_filter_(const struct device *can_dev, can_rx_callback_t can_rx_callback,
                       const struct can_filter *filter);

void can_tx_callback(const struct device *dev, int error, void *user_data);
void ccu_can_rx_callback(const struct device *dev, struct can_frame *frame,
                         void *user_data);
void ccu_can_tx_thread(void *p1, void *p2, void *p3);

int can_init(const struct device *can_dev, uint32_t baudrate);
void ccu_can_init();

#endif //CAN_H
