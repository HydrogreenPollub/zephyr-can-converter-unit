
#include <zephyr/logging/log.h>
#include <zephyr/dfu/mcuboot.h>
#include "can_converter.h"
#include "can_converter_can.h"
#include "can_converter_rs485.h"
#include "status_led.h"

LOG_MODULE_REGISTER(main);

int main(void) {
    LOG_INF("CAN Converter Unit has started");

    ccu_can_init();
    ccu_rs485_init();
    ccu_board_init();

    /* Confirm the running image is healthy so MCUboot doesn't roll back
     * on the next reboot. No-op if not in swap-test mode. */
    boot_write_img_confirmed();

    status_led_set(STATUS_LED_OPERATIONAL);

    return 0;
}
