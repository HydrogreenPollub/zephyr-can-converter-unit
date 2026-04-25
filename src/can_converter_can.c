#include "can_converter_can.h"
#include "can_converter_frames.h"
#include "can_converter_rs485.h"
#include "can_converter_dfu.h"
#include "status_led.h"
#include "candef.h"
#include "can_ids.h"

#include <zephyr/logging/log.h>

#include "can.h"
#include "gpio.h"

LOG_MODULE_REGISTER(ccu, LOG_LEVEL_INF);

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

static volatile int      can_tx_result;
static volatile bool     test_active;
static volatile uint32_t can_frames_tx_ok;
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

        can_frames_tx_ok++;
        gpio_set(&can.tx_led);
        k_work_reschedule(&tx_led_off_work, K_MSEC(50));
    }
}

static void ccu_can_periodic_thread(void *p1, void *p2, void *p3) {
    uint8_t cnt_100ms    = 0;
    uint8_t cnt_1000ms   = 0;
    uint8_t cnt_1s       = 0;
    uint8_t cnt_unassign = 0;  /* 0.1 Hz: send MCU_ANALOG_UNASSIGNED every 10 s */

    uint32_t           prev_rs485_rx  = 0;
    uint32_t           prev_can_tx_ok = 0;
    ccu_frame_counts_t prev_frames    = {0};

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

        /* 10 ms — 100 Hz: state machine + faults (high-priority, safety-relevant) */
        if (d.master_status_valid) {
            send_mcu_state(&d.master_status);
        }

        if (d.master_faults_valid) {
            send_mcu_faults(&d.master_faults);
        }

        cnt_100ms++;
        cnt_1000ms++;

        /* 100 ms — 10 Hz: speed + powertrain analog */
        if (cnt_100ms >= 10) {
            cnt_100ms = 0;

            if (d.master_measurements_valid) {
                send_mcu_analog_speed(&d.master_measurements);
                send_mcu_analog_powertrain(&d.master_measurements);
                send_mcu_analog_fuel_cell(&d.master_measurements);
                send_mcu_analog_accessory(&d.master_measurements);
            }
        }

        /* 1000 ms — 1 Hz: slow analog + digital inputs + Protium */
        if (cnt_1000ms >= 100) {
            cnt_1000ms = 0;

            if (d.master_measurements_valid) {
                send_mcu_analog_pedals(&d.master_measurements);
                send_mcu_inputs(&d.master_measurements);
            }

            if (d.protium_operating_state_valid) {
                send_protium_state(d.protium_operating_state);
            }

            if (d.protium_values_valid) {
                send_protium_power(&d.protium_values);
                send_protium_thermal(&d.protium_values);
                send_protium_hydrogen(&d.protium_values);
                send_protium_setpoints(&d.protium_values);
                send_protium_stasis(&d.protium_values);
                send_protium_misc(&d.protium_values);
            }
        }

        /* 1 s — diagnostic stats report */
        cnt_1s++;
        if (cnt_1s >= 100) {
            cnt_1s = 0;

            /* 0.1 Hz — unassigned analog inputs (every 10th 1 s tick) */
            cnt_unassign++;
            if (cnt_unassign >= 10) {
                cnt_unassign = 0;
                if (d.master_measurements_valid) {
                    send_mcu_analog_unassigned(&d.master_measurements);
                }
            }

            uint32_t cur_rs485  = rs485_frames_received;
            uint32_t cur_can_ok = can_frames_tx_ok;

            uint32_t d_rs485  = cur_rs485  - prev_rs485_rx;
            uint32_t d_can_ok = cur_can_ok - prev_can_tx_ok;

/* Frames enqueued in the last 1 s window == Hz directly. */
#define HZ(field) (frame_counts.field - prev_frames.field)

            LOG_INF("=== 1s stats: RS485 RX %u Hz | CAN TX ok %u ===",
                    d_rs485, d_can_ok);

            if (d.master_status_valid) {
                LOG_INF("  MCU state=%d move=%c mc=%c valve=%c",
                        (int)d.master_status.state,
                        d.master_status.vehicle_allowed_to_move  ? 'Y' : 'N',
                        d.master_status.motor_controller_enabled ? 'Y' : 'N',
                        d.master_status.main_valve_enabled       ? 'Y' : 'N');
            }

            if (d.master_faults_valid) {
                LOG_INF("  EMG: switch=%c dms=%c leak=%c"
                        " | ERR: fc_v=%c fc_c=%c sc_v=%c sc_c=%c"
                        " mc_v=%c mc_c=%c ab_v=%c ab_c=%c",
                        d.master_faults.emg_emergency_switch          ? 'Y' : 'N',
                        d.master_faults.emg_dead_mans_switch          ? 'Y' : 'N',
                        d.master_faults.emg_leakage_sensor_switch     ? 'Y' : 'N',
                        d.master_faults.err_fuel_cell_output_voltage  ? 'Y' : 'N',
                        d.master_faults.err_fuel_cell_output_current  ? 'Y' : 'N',
                        d.master_faults.err_supercapacitor_voltage    ? 'Y' : 'N',
                        d.master_faults.err_supercapacitor_current    ? 'Y' : 'N',
                        d.master_faults.err_motor_controller_supply_voltage  ? 'Y' : 'N',
                        d.master_faults.err_motor_controller_supply_current  ? 'Y' : 'N',
                        d.master_faults.err_accessory_battery_voltage ? 'Y' : 'N',
                        d.master_faults.err_accessory_battery_current ? 'Y' : 'N');
            }

            LOG_INF("  MCU Hz:"
                    " STATE=%u FAULTS=%u DRIVE=%u"
                    " PT=%u FC=%u ACC=%u PEDALS=%u INPUTS=%u",
                    HZ(mcu_state), HZ(mcu_faults), HZ(mcu_analog_speed),
                    HZ(mcu_analog_powertrain), HZ(mcu_analog_fuel_cell),
                    HZ(mcu_analog_accessory),
                    HZ(mcu_analog_pedals), HZ(mcu_inputs));
            LOG_INF("  PROT Hz:"
                    " STATE=%u POWER=%u THERMAL=%u"
                    " HYDROGEN=%u SETPOINTS=%u STASIS=%u MISC=%u",
                    HZ(protium_state), HZ(protium_power), HZ(protium_thermal),
                    HZ(protium_hydrogen), HZ(protium_setpoints),
                    HZ(protium_stasis), HZ(protium_misc));

#undef HZ

            prev_rs485_rx  = cur_rs485;
            prev_can_tx_ok = cur_can_ok;
            prev_frames    = frame_counts;
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

    send_mcu_analog_speed(&d.master_measurements);
    send_mcu_analog_pedals(&d.master_measurements);
    send_mcu_analog_powertrain(&d.master_measurements);
    send_mcu_analog_fuel_cell(&d.master_measurements);
    send_mcu_analog_accessory(&d.master_measurements);
    send_mcu_analog_unassigned(&d.master_measurements);
    send_mcu_inputs(&d.master_measurements);
    send_mcu_state(&d.master_status);
    send_mcu_faults(&d.master_faults);
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
