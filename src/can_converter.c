//
// Created by inż.Dawid Pisarczyk on 28.12.2025.
//

#include "can_converter.h"

#include "rs485.h"

LOG_MODULE_REGISTER(can_converter_unit);

static struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_ALIAS(status_led), gpios);

void can_converter_init() {
    //ccu_rs485_init();
    //ccu_can_init();

    gpio_init(&status_led, GPIO_OUTPUT_INACTIVE);

    while (true) {
        gpio_pin_toggle_dt(&status_led);
        k_msleep(500);
    }
}
