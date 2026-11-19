#include "can_converter.h"
#include "can_converter_can.h"
#include "can_converter_frames.h"
#include "can_converter_rs485.h"
#include <hydrogreen/peripherals/status_led.h>
#include <hydrogreen/peripherals/test_button.h>

#include <stdint.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ccu_board, LOG_LEVEL_INF);

K_MUTEX_DEFINE(data_mutex);
master_data_t data = {0};

static void broadcast_status(status_led_state_t state) {
#if CCU_SEND_STATUS_FRAMES
    send_ccu_status((uint8_t)state);
#else
    ARG_UNUSED(state);
#endif
}

/* ── Test button callback ─────────────────────────────────────────────────── */
static void on_test_button(void) {
    LOG_INF("Test button pressed — lighting all LEDs, sending all frames");
    status_led_set_override(true);
    ccu_can_test_set(true);
    ccu_rs485_test_set(true);
    ccu_can_test_send_all();
    k_sleep(K_SECONDS(2));
    ccu_can_test_set(false);
    ccu_rs485_test_set(false);
    status_led_set_override(false);
    LOG_INF("Test button: done");
}

/* ── Board init ───────────────────────────────────────────────────────────── */
void ccu_board_init(void) {
    static const struct gpio_dt_spec status_led_gpio =
        GPIO_DT_SPEC_GET(DT_ALIAS(status_led), gpios);
    static const struct gpio_dt_spec test_btn_gpio =
        GPIO_DT_SPEC_GET(DT_ALIAS(button_test), gpios);

    status_led_init(&status_led_gpio, broadcast_status);
    test_button_init(&test_btn_gpio, on_test_button);
}
