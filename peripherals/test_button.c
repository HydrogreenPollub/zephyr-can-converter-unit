#include "test_button.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(test_button, LOG_LEVEL_INF);

#define THREAD_STACK_SIZE 2048
#define THREAD_PRIORITY   6

K_THREAD_STACK_DEFINE(test_btn_stack, THREAD_STACK_SIZE);
static struct k_thread    test_btn_thread_data;
K_SEM_DEFINE(test_btn_sem, 0, 1);

static test_button_cb_t   registered_cb;
static struct gpio_callback btn_gpio_cb;

static void btn_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    k_sem_give(&test_btn_sem);
}

static void btn_thread(void *p1, void *p2, void *p3) {
    while (1) {
        k_sem_take(&test_btn_sem, K_FOREVER);
        if (registered_cb) {
            registered_cb();
        }
    }
}

void test_button_init(const struct gpio_dt_spec *btn, test_button_cb_t cb) {
    registered_cb = cb;

    gpio_pin_configure_dt(btn, GPIO_INPUT);
    gpio_init_callback(&btn_gpio_cb, btn_isr, BIT(btn->pin));
    gpio_add_callback(btn->port, &btn_gpio_cb);
    gpio_pin_interrupt_configure_dt(btn, GPIO_INT_EDGE_TO_ACTIVE);

    k_tid_t tid = k_thread_create(
        &test_btn_thread_data, test_btn_stack,
        K_THREAD_STACK_SIZEOF(test_btn_stack),
        btn_thread, NULL, NULL, NULL,
        THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(tid, "test_btn");

    LOG_INF("Test button initialized");
}
