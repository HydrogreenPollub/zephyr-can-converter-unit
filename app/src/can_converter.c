//
// Created by inż.Dawid Pisarczyk on 28.12.2025.
//

#include "can_converter.h"

LOG_MODULE_REGISTER(can_converter_unit);

struct can_filter swu_filter[] = {
    {.flags = 0U, .id = CAN_ID_SENSOR_SPEED, .mask = CAN_STD_ID_MASK},
    {.flags = 0U, .id = CAN_ID_SC_VOLTAGE, .mask = CAN_STD_ID_MASK},
};

swu_can_t can = {
    .can_device = DEVICE_DT_GET(DT_ALIAS(can)),
    .can_rx_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_rx_led), gpios),
};

void ccu_can_rx_callback(const struct device *dev, struct can_frame *frame,
                         void *user_data) {
  if ((frame->id) == 0x11) {
    printf("Odebrano: %u\n", frame->data[0]);
  }
}

static void cwu_can_init() {
  int ret;
  ret = can_init(can.can_device, HYDROGREEN_CAN_BAUD_RATE);
  if (ret == 0) {
    LOG_INF("CAN initialized correctly");
  }
  for (int i = 0; i < ARRAY_SIZE(swu_filter); i++) {
    struct can_filter *filter = &ccu_filter[i];
    ret = can_add_rx_filter_(can.can_device, lcu_can_rx_callback, filter);
    if (ret >= 0) {
      LOG_INF("CAN rx filter [%d] has been added succesfully", ret);
    }
  }
}

static void can_timer_handler(struct k_timer *timer) {
  ARG_UNUSED(timer);

  for (int i = 0; i < ARRAY_SIZE(lights_buttons); i++) {
    k_work_submit(&lights_buttons[i].work.work);
  }

  send_pending = true;
}

K_TIMER_DEFINE(can_timer, can_timer_handler, NULL);

void steering_wheel_init() {
  lights_buttons_init();
  swu_can_init();

  k_timer_start(&can_timer, K_MSEC(CHECK_LIGHTS_BUTTONS_MS),
                K_MSEC(CHECK_LIGHTS_BUTTONS_MS));
}

void can_tx_thread(void) {
  while (1) {
    if (send_pending) {
      send_pending = false;
      can_send_(can.can_device, CAN_ID_BUTTONS_LIGHTS_MASK,
                &lights_buttons_mask, sizeof(lights_buttons_mask));
    }
    k_sleep(K_MSEC(10));
  }
}
