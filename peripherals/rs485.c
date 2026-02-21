//
// Created by inż. Dawid Pisarczyk on 14.02.2026.
//

#include "rs485.h"

LOG_MODULE_REGISTER(rs485);

struct k_mutex tx_mutex;

int rs485_set_rx(struct gpio_dt_spec *gpio)
{
    return gpio_reset(gpio);  // 0 - RX mode
}

int rs485_set_tx(struct gpio_dt_spec *gpio)
{
    return gpio_set(gpio);  // 1 - TX mode
}

int rs485_init(const struct device* dev, struct gpio_dt_spec *dir)
{
    int ret;

    ret = uart_device_init(dev);
    if (!ret) {
        LOG_ERR("RS485 device not ready");
        return -ENODEV;
    }

    ret = gpio_init(dir, GPIO_OUTPUT_INACTIVE);
    if (!ret) {
        LOG_ERR("RS485 direction pin not ready");
        return -1;
    }
    ret = rs485_set_rx(dir);
    k_mutex_init(&tx_mutex);
    LOG_INF("RS485 hardware initialized succesfully");

    return ret;
}

int rs485_send(const struct device* dev, struct gpio_dt_spec *dir, const uint8_t *data, size_t len)
{
    k_mutex_lock(&tx_mutex, K_FOREVER);

    rs485_set_tx(dir);

    int ret = uart_tx(dev, data, len, SYS_FOREVER_MS);
    if (ret != 0) {
        rs485_set_rx(dir);
        k_mutex_unlock(&tx_mutex);
        LOG_ERR("RS485 TX failed");
        return ret;
    }

    return 0;
}