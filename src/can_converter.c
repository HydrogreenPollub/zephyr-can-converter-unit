#include "can_converter.h"
#include "can_converter_can.h"
#include "can_converter_rs485.h"
#include "status_led.h"
#include "test_button.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ccu_board, LOG_LEVEL_INF);

K_MUTEX_DEFINE(data_mutex);
master_data_t data = {0};

/* ── CCU_STATUS CAN frame (CiA 303-3 inspired, ID 0x10F) ──────────────────── */
#define CCU_STATUS_FRAME_ID 0x10FU

static void broadcast_status(status_led_state_t state) {
    struct can_frame frame = {
        .id    = CCU_STATUS_FRAME_ID,
        .dlc   = 1,
        .flags = CAN_FRAME_IDE,
    };
    frame.data[0] = (uint8_t)state;
    k_msgq_put(&can_tx_msgq, &frame, K_NO_WAIT);
}

/* ── Test button callback ─────────────────────────────────────────────────── */
static void on_test_button(void) {
    LOG_INF("Test button pressed — lighting all LEDs, sending all frames");
    status_led_set_override(true);
    ccu_can_test();
    ccu_rs485_test();
    k_sleep(K_SECONDS(2));
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
