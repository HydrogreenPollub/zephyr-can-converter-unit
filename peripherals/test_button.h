#ifndef TEST_BUTTON_H
#define TEST_BUTTON_H

#include <zephyr/drivers/gpio.h>

/* Callback invoked from a thread context (not ISR) on every button press. */
typedef void (*test_button_cb_t)(void);

/* Initialise a test button and start its handler thread.
 * The callback may block (e.g. k_sleep) safely.
 * Currently supports one button per board; call once at startup. */
void test_button_init(const struct gpio_dt_spec *btn, test_button_cb_t cb);

#endif /* TEST_BUTTON_H */
