#include "can.h"

#include <string.h>

LOG_MODULE_REGISTER(can);

K_THREAD_STACK_DEFINE(ccu_can_tx_thread_stack_area, CCU_CAN_TX_THREAD_STACK_SIZE);
struct k_thread ccu_can_tx_thread_data;

ccu_can_t can = {
    .can_device = DEVICE_DT_GET(DT_ALIAS(can)),
    .can_rx_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_rx_led), gpios),
    .can_tx_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_tx_led), gpios),
};

int can_add_rx_filter_(const struct device *can_dev,
                       can_rx_callback_t can_rx_callback,
                       const struct can_filter *filter) {
    int filter_id = can_add_rx_filter(can_dev, can_rx_callback, NULL, filter);
    if (filter_id < 0) {
        LOG_ERR("Unable to add rx filter [%d]", filter_id);
    } else {
        LOG_INF("CAN rx filter [%d] has been added succesfully", filter_id);
    }
    return filter_id;
}

int can_send_(const struct device *can_dev, uint16_t id, uint8_t *data,
              uint8_t data_len) {
    struct can_frame frame = {0};
    frame.id = id;
    frame.dlc = data_len;

    strncpy(frame.data, (char *) data, data_len);

    return can_send(can_dev, &frame, K_FOREVER, NULL, NULL);
}

int can_send_float(const struct device *can_dev, uint16_t id, float value) {
    return can_send_(can_dev, id, (uint8_t *) &value, sizeof(float));
}

void can_tx_callback(const struct device *dev, int error, void *user_data) {
    char *sender = (char *) user_data;

    if (error == -ENETUNREACH) {
        LOG_ERR("CAN network unreachable, error: %d, sender: %s", error, sender);
        return;
    }

    if (error != 0) {
        LOG_ERR("Sending failed [%d] Sender: %s", error, sender);
    }

    LOG_INF("Send succes");
}

void ccu_can_rx_callback(const struct device *dev, struct can_frame *frame,
                         void *user_data) {
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    LOG_DBG("CAN ID: 0x%03X", frame->id);

    switch ((can_id_t) frame->id) {
        // TODO
        default:
            break;
    }
}

void ccu_can_tx_thread(void *p1, void *p2, void *p3) {
    while (1) {
        // can_send_(can.can_device, CAN_ID_BUTTONS_LIGHTS_MASK, &lights_buttons_mask,
        //           sizeof(lights_buttons_mask));

        k_sleep(K_MSEC(10));
    }
}

int can_init(const struct device *can_dev, uint32_t baudrate) {
    struct can_timing timing;
    int ret;
    ret = can_calc_timing(can_dev, &timing, baudrate, 0);
    if (ret > 0) {
        LOG_INF("Sample-Point error: %d", ret);
    }
    if (ret < 0) {
        LOG_ERR("Failed to calc a valid timing");
        return -1;
    }

    ret = can_set_timing(can_dev, &timing);
    if (ret != 0) {
        LOG_ERR("Failed to set timing");
    }

    ret = can_start(can_dev);
    if (ret != 0) {
        LOG_ERR("Failed to start CAN controller");
    }

    if (ret == 0) {
        LOG_INF("CAN initialized correctly");
    }

    // for (int i = 0; i < ARRAY_SIZE(ccu_can_filters); i++) {
    //     can_add_rx_filter_(can.can_device, ccu_can_rx_callback,
    //                        &ccu_can_filters[i]);
    // }

    k_tid_t ccu_can_tx_thread_data_tid = k_thread_create(&ccu_can_tx_thread_data, ccu_can_tx_thread_stack_area,
                                                         K_THREAD_STACK_SIZEOF(ccu_can_tx_thread_stack_area),
                                                         ccu_can_tx_thread,
                                                         NULL, NULL, NULL,
                                                         CCU_CAN_TX_THREAD_PRIORITY, 0, K_NO_WAIT);

    return ret;
}

void ccu_can_init() {
    can_init(can.can_device, HYDROGREEN_CAN_BAUD_RATE);
}
