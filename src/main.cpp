#include <errno.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include "can_converter_can.hpp"
#include "can_converter_rs485.hpp"


LOG_MODULE_REGISTER(main);

int main(void) {
    ccu_can_init();
    ccu_rs485_init();

    // while (1) {
    // //     k_sleep(K_FOREVER);
    //     lcu_on_tick();
    // }
    return 0;
}
