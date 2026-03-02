#include <errno.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/util.h>

#include "can_converter.h"

LOG_MODULE_REGISTER(main);

int main() {
    can_converter_init();
    return 0;
}
