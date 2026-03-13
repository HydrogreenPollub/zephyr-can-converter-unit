#include <errno.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include "can_converter_can.h"
#include "can_converter_rs485.h"


LOG_MODULE_REGISTER(main);

int main(void) {
    ccu_can_init();
    ccu_rs485_init();

    return 0;
}
