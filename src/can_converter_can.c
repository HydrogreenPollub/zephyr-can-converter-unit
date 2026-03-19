#include "can_converter_can.h"
#include "can_converter_can_send.h"
#include "can_converter_dfu.h"
#include "status_led.h"
#include "candef.h"

LOG_MODULE_REGISTER(ccu_can, LOG_LEVEL_INF);

#define CCU_CAN_TX_THREAD_STACK_SIZE       2048
#define CCU_CAN_TX_THREAD_PRIORITY         5
#define CCU_CAN_PERIODIC_THREAD_STACK_SIZE 2048
#define CCU_CAN_PERIODIC_THREAD_PRIORITY   5

K_THREAD_STACK_DEFINE(ccu_can_tx_thread_stack_area, CCU_CAN_TX_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(ccu_can_periodic_thread_stack_area, CCU_CAN_PERIODIC_THREAD_STACK_SIZE);

struct k_thread ccu_can_tx_thread_data;
struct k_thread ccu_can_periodic_thread_data;

K_SEM_DEFINE(can_tx_done_sem, 0, 1);
K_MSGQ_DEFINE(can_tx_msgq, sizeof(struct can_frame), 32, 4);

static volatile int can_tx_result;
static volatile bool test_active;
static struct k_work_delayable tx_led_off_work;
static struct k_work_delayable rx_led_off_work;
static struct k_work_delayable bus_off_recovery_work;

struct can_filter ccu_can_filter = {
    .id = CANDEF_MCU_TIME_SYNC_FRAME_ID,
    .mask = 0x7C0,
    .flags = CAN_FRAME_IDE,
};

ccu_can_t can = {
    .device = DEVICE_DT_GET(DT_ALIAS(can)),
    .tx_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_tx_led), gpios),
    .rx_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_rx_led), gpios),
};

static void ccu_can_tx_callback(const struct device *dev, int error, void *user_data);

static void tx_led_off_handler(struct k_work *work) { if (!test_active) gpio_reset(&can.tx_led); }
static void rx_led_off_handler(struct k_work *work) { if (!test_active) gpio_reset(&can.rx_led); }

static void bus_off_recovery_handler(struct k_work *work) {
    int ret = can_recover(can.device, K_MSEC(100));
    if (ret != 0 && ret != -ENOTSUP) {
        LOG_WRN("CAN recovery failed (%d), retrying in 1s", ret);
        k_work_reschedule(&bus_off_recovery_work, K_SECONDS(1));
    } else {
        LOG_INF("CAN bus recovered");
        status_led_set(STATUS_LED_OPERATIONAL);
    }
}

static void can_state_change_cb(const struct device *dev,
                                enum can_state state,
                                struct can_bus_err_cnt err_cnt,
                                void *user_data) {
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    LOG_WRN("CAN state: %d (tx_err=%d rx_err=%d)",
            state, err_cnt.tx_err_cnt, err_cnt.rx_err_cnt);

    switch (state) {
    case CAN_STATE_ERROR_ACTIVE:
        status_led_set(STATUS_LED_OPERATIONAL);
        break;
    case CAN_STATE_ERROR_WARNING:
    case CAN_STATE_ERROR_PASSIVE:
        status_led_set(STATUS_LED_WARNING);
        break;
    case CAN_STATE_BUS_OFF:
        status_led_set(STATUS_LED_BUS_OFF);
        k_msgq_purge(&can_tx_msgq);
        k_work_reschedule(&bus_off_recovery_work, K_MSEC(100));
        break;
    default:
        break;
    }
}

static void ccu_can_tx_thread(void *p1, void *p2, void *p3) {
    struct can_frame frame = {0};
    LOG_INF("CAN TX thread started");

    while (1) {
        k_msgq_get(&can_tx_msgq, &frame, K_FOREVER);

        int ret = can_send(can.device, &frame, K_MSEC(100), ccu_can_tx_callback, NULL);
        if (ret) {
            LOG_ERR("CAN send failed: %d", ret);
            continue;
        }

        if (k_sem_take(&can_tx_done_sem, K_MSEC(200)) != 0) {
            LOG_ERR("CAN TX timeout");
            continue;
        }

        if (can_tx_result != 0) {
            LOG_ERR("CAN TX error: %d", can_tx_result);
            continue;
        }

        gpio_set(&can.tx_led);
        k_work_reschedule(&tx_led_off_work, K_MSEC(50));
    }
}

static void ccu_can_periodic_thread(void *p1, void *p2, void *p3) {
    uint8_t cnt_100ms = 0;
    uint8_t cnt_1000ms = 0;
    while (1) {
        k_sleep(K_MSEC(10));

        /* Suspend all periodic TX during DFU — reduces bus contention so the
         * device only participates as a receiver + ACK node. */
        if (can_dfu_is_active()) {
            cnt_100ms  = 0;
            cnt_1000ms = 0;
            continue;
        }

        master_data_t d = {0};
        k_mutex_lock(&data_mutex, K_FOREVER);
        d = data;
        k_mutex_unlock(&data_mutex);

        cnt_100ms++;
        cnt_1000ms++;

        if (cnt_1000ms >= 100) {
            cnt_1000ms = 0;

            send_mcu_faults();

            if (d.protium_operating_state_valid) {
                send_protium_state(d.protium_operating_state);
            }
        }

        if (cnt_100ms >= 10) {
            cnt_100ms = 0;

            send_mcu_analog_drive(&d.master_measurements);
            send_mcu_analog_pedals(&d.master_measurements);
            send_mcu_analog_powertrain(&d.master_measurements);
            send_mcu_analog_fuel_cell(&d.master_measurements);
            send_mcu_analog_accessory(&d.master_measurements);
            send_mcu_inputs(&d.master_measurements);
            send_mcu_state(&d.master_status);
        }
    }
}

static void ccu_can_tx_callback(const struct device *dev, int error, void *user_data) {
    can_tx_result = error;
    k_sem_give(&can_tx_done_sem);
}

static void ccu_dfu_rx_cb(const struct device *dev, struct can_frame *frame, void *user_data) {
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    /* Log CMD frames (REQUEST/COMMIT) at INF — always visible.
     * Log DATA frames at DBG — enable ccu_can DBG level to see them.
     * Logging every DATA frame at INF would generate ~500 log/s during DFU
     * and overwhelm the RTT buffer even in DROP mode. */
    if (frame->id == CCU_DFU_CMD_ID) {
        LOG_INF("CAN RX CMD id=0x%03X dlc=%u data=%02X %02X %02X %02X %02X",
                frame->id, frame->dlc,
                frame->dlc > 0 ? frame->data[0] : 0,
                frame->dlc > 1 ? frame->data[1] : 0,
                frame->dlc > 2 ? frame->data[2] : 0,
                frame->dlc > 3 ? frame->data[3] : 0,
                frame->dlc > 4 ? frame->data[4] : 0);
    } else {
        LOG_DBG("CAN RX DATA id=0x%03X dlc=%u seq=%u",
                frame->id, frame->dlc,
                frame->dlc >= 2 ? (uint16_t)(frame->data[0] | (frame->data[1] << 8)) : 0);
    }

    /* Flash CAN RX LED */
    gpio_set(&can.rx_led);
    k_work_reschedule(&rx_led_off_work, K_MSEC(50));

    can_dfu_on_frame(frame);
}

/* Single filter matches both 0x7E0 (CMD) and 0x7E1 (DATA) — both land on FIFO0 */
static const struct can_filter dfu_filter = {
    .id    = CCU_DFU_CMD_ID,
    .mask  = 0x1FFFFFFEU,
    .flags = CAN_FRAME_IDE,
};

void ccu_can_test_set(bool active) {
    test_active = active;
    if (active) {
        gpio_set(&can.rx_led);
        gpio_set(&can.tx_led);
    } else {
        gpio_reset(&can.rx_led);
        gpio_reset(&can.tx_led);
    }
}

void ccu_can_test_send_all(void) {
    master_data_t d = {0};
    k_mutex_lock(&data_mutex, K_FOREVER);
    d = data;
    k_mutex_unlock(&data_mutex);

    send_mcu_analog_drive(&d.master_measurements);
    send_mcu_analog_pedals(&d.master_measurements);
    send_mcu_analog_powertrain(&d.master_measurements);
    send_mcu_analog_fuel_cell(&d.master_measurements);
    send_mcu_analog_accessory(&d.master_measurements);
    send_mcu_inputs(&d.master_measurements);
    send_mcu_state(&d.master_status);
    send_mcu_faults();
    if (d.protium_operating_state_valid) {
        send_protium_state(d.protium_operating_state);
    }

    LOG_INF("CAN test: all frames enqueued");
}

void ccu_can_init(void) {
    can_init(can.device, HYDROGREEN_CAN_BAUD_RATE);
    gpio_init(&can.rx_led, GPIO_OUTPUT_INACTIVE);
    gpio_init(&can.tx_led, GPIO_OUTPUT_INACTIVE);
    k_work_init_delayable(&tx_led_off_work, tx_led_off_handler);
    k_work_init_delayable(&rx_led_off_work, rx_led_off_handler);
    k_work_init_delayable(&bus_off_recovery_work, bus_off_recovery_handler);
    can_set_state_change_callback(can.device, can_state_change_cb, NULL);

    can_add_rx_filter_(can.device, ccu_dfu_rx_cb, &dfu_filter);
    ccu_dfu_init();

    k_tid_t tx_tid = k_thread_create(
        &ccu_can_tx_thread_data, ccu_can_tx_thread_stack_area,
        K_THREAD_STACK_SIZEOF(ccu_can_tx_thread_stack_area),
        ccu_can_tx_thread, NULL, NULL, NULL,
        CCU_CAN_TX_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(tx_tid, "can_tx");

    k_tid_t periodic_tid = k_thread_create(
        &ccu_can_periodic_thread_data, ccu_can_periodic_thread_stack_area,
        K_THREAD_STACK_SIZEOF(ccu_can_periodic_thread_stack_area),
        ccu_can_periodic_thread, NULL, NULL, NULL,
        CCU_CAN_PERIODIC_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(periodic_tid, "can_periodic");
}
