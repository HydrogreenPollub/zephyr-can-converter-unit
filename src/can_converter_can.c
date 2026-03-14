#include "can_converter_can.h"
#include "can_converter_can_send.h"
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
static struct k_work_delayable tx_led_off_work;
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

static void tx_led_off_handler(struct k_work *work) {
    gpio_reset(&can.tx_led);
}

static void bus_off_recovery_handler(struct k_work *work) {
    int ret = can_recover(can.device, K_MSEC(100));
    if (ret != 0 && ret != -ENOTSUP) {
        LOG_WRN("CAN recovery failed (%d), retrying in 1s", ret);
        k_work_reschedule(&bus_off_recovery_work, K_SECONDS(1));
    } else {
        LOG_INF("CAN bus recovered");
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

    if (state == CAN_STATE_BUS_OFF) {
        k_msgq_purge(&can_tx_msgq);
        k_work_reschedule(&bus_off_recovery_work, K_MSEC(100));
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
    uint16_t fake_tick = 0;
    float fake_accel = 0.0f;

    while (1) {
        k_sleep(K_MSEC(10));

        master_data_t d = {0};
        k_mutex_lock(&data_mutex, K_FOREVER);
        d = data;
        k_mutex_unlock(&data_mutex);

        if (d.master_measurements_valid) {
            send_mcu_analog_drive(&d.master_measurements);
        }

        cnt_100ms++;
        cnt_1000ms++;

        if (cnt_1000ms >= 100) {
            cnt_1000ms = 0;

            fake_accel = (float) (fake_tick % 100);
            fake_tick++;

            if (d.protium_operating_state_valid) {
                send_protium_state(d.protium_operating_state);
            }
        }

        if (cnt_100ms >= 10) {
            cnt_100ms = 0;

            d.master_measurements.fuel_cell_output_voltage = 21.0f;
            d.master_measurements.supercapacitor_voltage = 37.0f;
            d.master_measurements.accel_pedal_voltage = fake_accel;

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

void ccu_can_init(void) {
    can_init(can.device, HYDROGREEN_CAN_BAUD_RATE);
    gpio_init(&can.rx_led, GPIO_OUTPUT_INACTIVE);
    gpio_init(&can.tx_led, GPIO_OUTPUT_INACTIVE);
    k_work_init_delayable(&tx_led_off_work, tx_led_off_handler);
    k_work_init_delayable(&bus_off_recovery_work, bus_off_recovery_handler);
    can_set_state_change_callback(can.device, can_state_change_cb, NULL);

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
