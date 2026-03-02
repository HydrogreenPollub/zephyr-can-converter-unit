//
// Created by inż. Dawid Pisarczyk on 14.02.2026.
//

#include "rs485.h"
#include "can_converter.h"

LOG_MODULE_REGISTER(rs485);

K_THREAD_STACK_DEFINE(ccu_rs485_tx_thread_stack_area, CCU_RS485_TX_THREAD_STACK_SIZE);
struct k_thread ccu_rs485_tx_thread_data;

ccu_rs485_t rs485 = {
    .rs485_device = DEVICE_DT_GET(DT_ALIAS(rs485)),
    .rs485_rx_led = GPIO_DT_SPEC_GET(DT_ALIAS(rs485_rx_led), gpios),
    .rs485_tx_led = GPIO_DT_SPEC_GET(DT_ALIAS(rs485_tx_led), gpios),
    .rs485_dir = GPIO_DT_SPEC_GET(DT_ALIAS(rs485_dir), gpios),
};

struct k_mutex tx_mutex;

int rs485_set_rx(struct gpio_dt_spec *gpio) {
    return gpio_reset(gpio); // 0 - RX mode
}

int rs485_set_tx(struct gpio_dt_spec *gpio) {
    return gpio_set(gpio); // 1 - TX mode
}

int rs485_send(const struct device *dev, struct gpio_dt_spec *dir, const uint8_t *data, size_t len) {
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

void ccu_rs485_tx_thread(void *p1, void *p2, void *p3) {
}

int rs485_init(const struct device *dev, struct gpio_dt_spec *dir) {
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

    k_tid_t ccu_rs485_tx_thread_tid = k_thread_create(&ccu_rs485_tx_thread_data, ccu_rs485_tx_thread_stack_area,
                                                      K_THREAD_STACK_SIZEOF(ccu_rs485_tx_thread_stack_area),
                                                      ccu_rs485_tx_thread,
                                                      NULL, NULL, NULL,
                                                      CCU_RS485_TX_THREAD_PRIORITY, 0, K_NO_WAIT);

    return ret;
}

void ccu_rs485_init() {
    rs485_init(rs485.rs485_device, &rs485.rs485_dir);
}
