//
// Created by inż. Dawid Pisarczyk on 08.03.2026.
//

#include "can_converter_can.hpp"

#include "../can-definitions/can_ids.h"

LOG_MODULE_REGISTER(ccu_can, LOG_LEVEL_INF);

#define CCU_CAN_TX_THREAD_STACK_SIZE 2048
#define CCU_CAN_TX_THREAD_PRIORITY 5

K_THREAD_STACK_DEFINE(ccu_can_tx_thread_stack_area, CCU_CAN_TX_THREAD_STACK_SIZE);
struct k_thread ccu_can_tx_thread_data;

#define CCU_CAN_PERIODIC_THREAD_STACK_SIZE 2048
#define CCU_CAN_PERIODIC_THREAD_PRIORITY 5

K_THREAD_STACK_DEFINE(ccu_can_periodic_thread_stack_area, CCU_CAN_PERIODIC_THREAD_STACK_SIZE);
struct k_thread ccu_can_periodic_thread_data;

K_SEM_DEFINE(can_tx_done_sem, 0, 1);
K_MSGQ_DEFINE(can_tx_msgq, sizeof(struct can_frame), 32, 4);

static volatile int can_tx_result;

//zephyr typically provides only 28 standard filters
//hydrogreen ids are in range 0x100 → 0x125

struct can_filter ccu_can_filter = {
    .id = CAN_ID_TIME, // 0x100
    // mask 0x7C0 keeps upper bits fixed
    .mask = 0x7C0,   // range captured: 0x100–0x13F
    .flags = 0U,
};

ccu_can_t can = {
    .device = DEVICE_DT_GET(DT_ALIAS(can)),
    .tx_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_tx_led), gpios),
    .rx_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_rx_led), gpios),
};

static void enqueue_can_data(uint32_t id, const void *data, uint8_t len)
{
    struct can_frame frame = {};

    if (len > sizeof(frame.data)) {
        LOG_WRN("CAN payload too big: %u", len);
        return;
    }

    frame.id = id;
    frame.dlc = len;
    frame.flags = 0;
    memcpy(frame.data, data, len);

    if (k_msgq_put(&can_tx_msgq, &frame, K_NO_WAIT) != 0) {
        LOG_WRN("CAN queue full");
    }
}

static void enqueue_can_data_float(uint32_t id, float value)
{
    enqueue_can_data(id, &value, sizeof(float));
}

static void enqueue_can_data_u8(uint32_t id, uint8_t value)
{
    enqueue_can_data(id, &value, sizeof(uint8_t));
}

// void ccu_can_rx_callback(const struct device *dev, struct can_frame *frame,
//                          void *user_data) {
//
//   ARG_UNUSED(dev);
//   ARG_UNUSED(user_data);
//
//   LOG_INF("CAN ID: 0x%03X", frame->id);
//
//   switch((can_id_t)frame->id) {
//
//     case CAN_ID_TIME:
//           LOG_INF("TIME ID: 0x%03X", frame->id);
//         /* TODO: handle time */
//         break;
//
//     case CAN_ID_TIME_BEFORE_TRANSMIT:
//         /* TODO */
//         break;
//
//     case CAN_ID_ACC_BATTERY_VOLTAGE:
//         /* TODO */
//         break;
//
//     case CAN_ID_ACC_BATTERY_CURRENT:
//         /* TODO */
//         break;
//
//     case CAN_ID_FC_OUTPUT_VOLTAGE:
//         /* TODO */
//         break;
//
//     case CAN_ID_FC_OUTPUT_CURRENT:
//         /* TODO */
//         break;
//
//     case CAN_ID_SC_VOLTAGE:
//         /* TODO */
//         break;
//
//     case CAN_ID_SC_CURRENT:
//         /* TODO */
//         break;
//
//     case CAN_ID_MC_SUPPLY_VOLTAGE:
//         /* TODO */
//         break;
//
//     case CAN_ID_MC_SUPPLY_CURRENT:
//         /* TODO */
//         break;
//
//     case CAN_ID_FC_ENERGY_ACCUMULATED:
//         /* TODO */
//         break;
//
//     case CAN_ID_H2_PRESSURE_LOW:
//         /* TODO */
//         break;
//
//     case CAN_ID_H2_PRESSURE_FUEL_CELL:
//         /* TODO */
//         break;
//
//     case CAN_ID_H2_PRESSURE_HIGH:
//         /* TODO */
//         break;
//
//     case CAN_ID_H2_LEAKAGE_SENSOR_VOLTAGE:
//         /* TODO */
//         break;
//
//     case CAN_ID_FAN_DUTY_CYCLE:
//         /* TODO */
//         break;
//
//     case CAN_ID_BLOWER_DUTY_CYCLE:
//         /* TODO */
//         break;
//
//     case CAN_ID_TEMP_FC_LOC1:
//         /* TODO */
//         break;
//
//     case CAN_ID_TEMP_FC_LOC2:
//         /* TODO */
//         break;
//
//     case CAN_ID_ACCEL_PEDAL_VOLTAGE:
//         /* TODO */
//         break;
//
//     case CAN_ID_BRAKE_PEDAL_VOLTAGE:
//         /* TODO */
//         break;
//
//     case CAN_ID_ACCEL_OUTPUT_VOLTAGE:
//         /* TODO */
//         break;
//
//     case CAN_ID_BRAKE_OUTPUT_VOLTAGE:
//         /* TODO */
//         break;
//
//     case CAN_ID_BUTTONS_MASTER_MASK:
//         /* TODO */
//         break;
//
//     case CAN_ID_BUTTONS_STEERING_MASK:
//         /* TODO */
//         break;
//
//     case CAN_ID_SENSOR_RPM:
//         /* TODO */
//         break;
//
//     case CAN_ID_SENSOR_SPEED:
//         /* TODO */
//         break;
//
//     case CAN_ID_LAP_NUMBER:
//         /* TODO */
//         break;
//
//     case CAN_ID_LAP_TIME:
//         /* TODO */
//         break;
//
//     case CAN_ID_GPS_ALTITUDE:
//         /* TODO */
//         break;
//
//     case CAN_ID_GPS_LATITUDE:
//         /* TODO */
//         break;
//
//     case CAN_ID_GPS_LONGITUDE:
//         /* TODO */
//         break;
//
//     case CAN_ID_GPS_SPEED:
//         /* TODO */
//         break;
//
//     case CAN_ID_MASTER_STATE:
//         /* TODO */
//         break;
//
//     case CAN_ID_PROTIUM_STATE:
//         /* TODO */
//         break;
//
//     case CAN_ID_MAIN_VALVE_ENABLE_OUTPUT:
//         /* TODO */
//         break;
//
//     case CAN_ID_MC_ENABLE_OUTPUT:
//         /* TODO */
//         break;
//
//     case CAN_ID_BUTTONS_LIGHTS_MASK:
//         /* TODO */
//         break;
//
//     default:
//         break;
//   }
// }


static void ccu_can_tx_callback(const struct device *dev,
                            int error,
                            void *user_data)
{
    can_tx_result = error;
    k_sem_give(&can_tx_done_sem);
}

static void ccu_can_tx_thread(void *p1, void *p2, void *p3) {

    struct can_frame frame = {0};
    int ret;
    LOG_INF("CAN TX thread started");

    while (1) {

        k_msgq_get(&can_tx_msgq, &frame, K_FOREVER);

        ret = can_send(can.device, &frame, K_MSEC(100), ccu_can_tx_callback, NULL);
        if(!ret){
            if (k_sem_take(&can_tx_done_sem, K_MSEC(200)) == 0){
                if (can_tx_result == 0) {
                    LOG_INF("CAN sent success");
                    gpio_set(&can.tx_led);
                    k_sleep(K_MSEC(50));
                    gpio_reset(&can.tx_led);
                }
                else {
                    LOG_ERR("CAN TX error: %d", can_tx_result);
                }
            }
            else {
                LOG_ERR("CAN TX timeout");
            }
        }
        else {
            LOG_ERR("Can sent failed: %d", ret);
        }
        k_sleep(K_MSEC(100));
    }
}

static void ccu_can_periodic_thread(void *p1, void *p2, void *p3)
{
    uint8_t cnt_100ms = 0;
    uint8_t cnt_1000ms = 0;

    while (1) {
        master_data_t data_to_send = {};

        k_sleep(K_MSEC(10));

        k_mutex_lock(&data_mutex, K_FOREVER);
        data_to_send = data;
        k_mutex_unlock(&data_mutex);

        // every 10 ms
        if (data_to_send.master_measurements_valid) {
            enqueue_can_data_float(CAN_ID_SENSOR_SPEED, data_to_send.master_measurements.speed);
            enqueue_can_data_float(CAN_ID_SENSOR_RPM, data_to_send.master_measurements.rpm);
        }

        cnt_100ms++;
        cnt_1000ms++;

        // every 100 ms
        if (cnt_100ms >= 10) {
            cnt_100ms = 0;

            if (data_to_send.master_measurements_valid) {
                enqueue_can_data_float(CAN_ID_SC_VOLTAGE, data_to_send.master_measurements.supercapacitorVoltage);
                enqueue_can_data_float(CAN_ID_SC_CURRENT, data_to_send.master_measurements.supercapacitorCurrent);
                enqueue_can_data_float(CAN_ID_ACCEL_PEDAL_VOLTAGE, data_to_send.master_measurements.accelPedalVoltage);
                enqueue_can_data_float(CAN_ID_BRAKE_PEDAL_VOLTAGE, data_to_send.master_measurements.brakePedalVoltage);
                enqueue_can_data_float(CAN_ID_ACC_BATTERY_VOLTAGE, data_to_send.master_measurements.accessoryBatteryVoltage);
                enqueue_can_data_float(CAN_ID_ACC_BATTERY_CURRENT, data_to_send.master_measurements.accessoryBatteryCurrent);
                enqueue_can_data_float(CAN_ID_H2_PRESSURE_HIGH, data_to_send.master_measurements.hydrogenHighPressure);
                enqueue_can_data_float(CAN_ID_H2_LEAKAGE_SENSOR_VOLTAGE, data_to_send.master_measurements.hydrogenLeakageSensorVoltage);
                enqueue_can_data_float(CAN_ID_FC_OUTPUT_CURRENT, data_to_send.master_measurements.fuelCellOutputCurrent);
                enqueue_can_data_float(CAN_ID_FC_OUTPUT_VOLTAGE, data_to_send.master_measurements.fuelCellOutputVoltage);
                enqueue_can_data_float(CAN_ID_MC_SUPPLY_VOLTAGE, data_to_send.master_measurements.motorControllerSupplyVoltage);
                enqueue_can_data_float(CAN_ID_MC_SUPPLY_CURRENT, data_to_send.master_measurements.motorControllerSupplyCurrent);
                enqueue_can_data_u8(CAN_ID_BUTTONS_MASTER_MASK, data_to_send.master_measurements.din);
            }

            if (data_to_send.master_status_valid) {
                enqueue_can_data_u8(CAN_ID_MASTER_STATE, data_to_send.master_status.state);
            }
        }

        // every 1000 ms
        if (cnt_1000ms >= 100) {
            cnt_1000ms = 0;

            if (data_to_send.protium_operating_state_valid) {
                enqueue_can_data_u8(CAN_ID_PROTIUM_STATE, data_to_send.protium_operating_state);
            }
        }
    }
}

void ccu_can_init() {
    can_init(can.device, HYDROGREEN_CAN_BAUD_RATE);
    gpio_init(&can.rx_led, GPIO_OUTPUT_INACTIVE);
    gpio_init(&can.tx_led, GPIO_OUTPUT_INACTIVE);
    // can_add_rx_filter_(can.device, ccu_can_rx_callback, &ccu_can_filter);

    k_tid_t ccu_can_tx_thread_data_tid = k_thread_create(&ccu_can_tx_thread_data, ccu_can_tx_thread_stack_area,
                                  K_THREAD_STACK_SIZEOF(ccu_can_tx_thread_stack_area),
                                  ccu_can_tx_thread,
                                  NULL, NULL, NULL,
                                  CCU_CAN_TX_THREAD_PRIORITY, 0, K_NO_WAIT);

    k_thread_name_set(ccu_can_tx_thread_data_tid, "can_tx");

    k_tid_t ccu_can_periodic_thread_data_tid = k_thread_create(&ccu_can_periodic_thread_data, ccu_can_periodic_thread_stack_area,
                              K_THREAD_STACK_SIZEOF(ccu_can_periodic_thread_stack_area),
                              ccu_can_periodic_thread,
                              NULL, NULL, NULL,
                              CCU_CAN_PERIODIC_THREAD_PRIORITY, 0, K_NO_WAIT);

    k_thread_name_set(ccu_can_periodic_thread_data_tid, "can_periodic");
}




