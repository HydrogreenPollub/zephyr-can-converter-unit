//
// Created by inż.Dawid Pisarczyk on 28.12.2025.
//

#include "can_converter.h"

LOG_MODULE_REGISTER(can_converter_unit);

static const struct can_filter ccu_can_filters[] = {

    CAN_FILTER(CAN_ID_TIME),
    CAN_FILTER(CAN_ID_TIME_BEFORE_TRANSMIT),

    CAN_FILTER(CAN_ID_ACC_BATTERY_VOLTAGE),
    CAN_FILTER(CAN_ID_ACC_BATTERY_CURRENT),

    CAN_FILTER(CAN_ID_FC_OUTPUT_VOLTAGE),
    CAN_FILTER(CAN_ID_FC_OUTPUT_CURRENT),

    CAN_FILTER(CAN_ID_SC_VOLTAGE),
    CAN_FILTER(CAN_ID_SC_CURRENT),

    CAN_FILTER(CAN_ID_MC_SUPPLY_VOLTAGE),
    CAN_FILTER(CAN_ID_MC_SUPPLY_CURRENT),

    CAN_FILTER(CAN_ID_FC_ENERGY_ACCUMULATED),

    CAN_FILTER(CAN_ID_H2_PRESSURE_LOW),
    CAN_FILTER(CAN_ID_H2_PRESSURE_FUEL_CELL),
    CAN_FILTER(CAN_ID_H2_PRESSURE_HIGH),
    CAN_FILTER(CAN_ID_H2_LEAKAGE_SENSOR_VOLTAGE),

    CAN_FILTER(CAN_ID_FAN_DUTY_CYCLE),
    CAN_FILTER(CAN_ID_BLOWER_DUTY_CYCLE),

    CAN_FILTER(CAN_ID_TEMP_FC_LOC1),
    CAN_FILTER(CAN_ID_TEMP_FC_LOC2),

    CAN_FILTER(CAN_ID_ACCEL_PEDAL_VOLTAGE),
    CAN_FILTER(CAN_ID_BRAKE_PEDAL_VOLTAGE),
    CAN_FILTER(CAN_ID_ACCEL_OUTPUT_VOLTAGE),
    CAN_FILTER(CAN_ID_BRAKE_OUTPUT_VOLTAGE),

    CAN_FILTER(CAN_ID_BUTTONS_MASTER_MASK),
    CAN_FILTER(CAN_ID_BUTTONS_STEERING_MASK),

    CAN_FILTER(CAN_ID_SENSOR_RPM),
    CAN_FILTER(CAN_ID_SENSOR_SPEED),

    CAN_FILTER(CAN_ID_LAP_NUMBER),
    CAN_FILTER(CAN_ID_LAP_TIME),

    CAN_FILTER(CAN_ID_GPS_ALTITUDE),
    CAN_FILTER(CAN_ID_GPS_LATITUDE),
    CAN_FILTER(CAN_ID_GPS_LONGITUDE),
    CAN_FILTER(CAN_ID_GPS_SPEED),

    CAN_FILTER(CAN_ID_MASTER_STATE),
    CAN_FILTER(CAN_ID_PROTIUM_STATE),

    CAN_FILTER(CAN_ID_MAIN_VALVE_ENABLE_OUTPUT),
    CAN_FILTER(CAN_ID_MC_ENABLE_OUTPUT),

    CAN_FILTER(CAN_ID_BUTTONS_LIGHTS_MASK),
};

ccu_can_t can = {
    .can_device = DEVICE_DT_GET(DT_ALIAS(can)),
    .can_rx_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_rx_led), gpios),
    .can_tx_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_tx_led), gpios),
};

ccu_rs485_t rs485 = {
    .rs485_device = DEVICE_DT_GET(DT_ALIAS(rs485)),
    .rs485_rx_led = GPIO_DT_SPEC_GET(DT_ALIAS(rs485_rx_led), gpios),
    .rs485_tx_led = GPIO_DT_SPEC_GET(DT_ALIAS(rs485_tx_led), gpios),
};


void ccu_can_rx_callback(const struct device *dev, struct can_frame *frame,
                         void *user_data) {

  ARG_UNUSED(dev);
  ARG_UNUSED(user_data);

  LOG_DBG("CAN ID: 0x%03X", frame->id);

  switch((can_id_t)frame->id) {

    case CAN_ID_TIME:
        /* TODO: handle time */
        break;

    case CAN_ID_TIME_BEFORE_TRANSMIT:
        /* TODO */
        break;

    case CAN_ID_ACC_BATTERY_VOLTAGE:
        /* TODO */
        break;

    case CAN_ID_ACC_BATTERY_CURRENT:
        /* TODO */
        break;

    case CAN_ID_FC_OUTPUT_VOLTAGE:
        /* TODO */
        break;

    case CAN_ID_FC_OUTPUT_CURRENT:
        /* TODO */
        break;

    case CAN_ID_SC_VOLTAGE:
        /* TODO */
        break;

    case CAN_ID_SC_CURRENT:
        /* TODO */
        break;

    case CAN_ID_MC_SUPPLY_VOLTAGE:
        /* TODO */
        break;

    case CAN_ID_MC_SUPPLY_CURRENT:
        /* TODO */
        break;

    case CAN_ID_FC_ENERGY_ACCUMULATED:
        /* TODO */
        break;

    case CAN_ID_H2_PRESSURE_LOW:
        /* TODO */
        break;

    case CAN_ID_H2_PRESSURE_FUEL_CELL:
        /* TODO */
        break;

    case CAN_ID_H2_PRESSURE_HIGH:
        /* TODO */
        break;

    case CAN_ID_H2_LEAKAGE_SENSOR_VOLTAGE:
        /* TODO */
        break;

    case CAN_ID_FAN_DUTY_CYCLE:
        /* TODO */
        break;

    case CAN_ID_BLOWER_DUTY_CYCLE:
        /* TODO */
        break;

    case CAN_ID_TEMP_FC_LOC1:
        /* TODO */
        break;

    case CAN_ID_TEMP_FC_LOC2:
        /* TODO */
        break;

    case CAN_ID_ACCEL_PEDAL_VOLTAGE:
        /* TODO */
        break;

    case CAN_ID_BRAKE_PEDAL_VOLTAGE:
        /* TODO */
        break;

    case CAN_ID_ACCEL_OUTPUT_VOLTAGE:
        /* TODO */
        break;

    case CAN_ID_BRAKE_OUTPUT_VOLTAGE:
        /* TODO */
        break;

    case CAN_ID_BUTTONS_MASTER_MASK:
        /* TODO */
        break;

    case CAN_ID_BUTTONS_STEERING_MASK:
        /* TODO */
        break;

    case CAN_ID_SENSOR_RPM:
        /* TODO */
        break;

    case CAN_ID_SENSOR_SPEED:
        /* TODO */
        break;

    case CAN_ID_LAP_NUMBER:
        /* TODO */
        break;

    case CAN_ID_LAP_TIME:
        /* TODO */
        break;

    case CAN_ID_GPS_ALTITUDE:
        /* TODO */
        break;

    case CAN_ID_GPS_LATITUDE:
        /* TODO */
        break;

    case CAN_ID_GPS_LONGITUDE:
        /* TODO */
        break;

    case CAN_ID_GPS_SPEED:
        /* TODO */
        break;

    case CAN_ID_MASTER_STATE:
        /* TODO */
        break;

    case CAN_ID_PROTIUM_STATE:
        /* TODO */
        break;

    case CAN_ID_MAIN_VALVE_ENABLE_OUTPUT:
        /* TODO */
        break;

    case CAN_ID_MC_ENABLE_OUTPUT:
        /* TODO */
        break;

    case CAN_ID_BUTTONS_LIGHTS_MASK:
        /* TODO */
        break;

    default:
        break;
  }
}

static void ccu_can_init() {
  can_init(can.can_device, HYDROGREEN_CAN_BAUD_RATE);

  for (int i = 0; i < ARRAY_SIZE(ccu_can_filters); i++) {
    can_add_rx_filter_(can.can_device, ccu_can_rx_callback,
                       &ccu_can_filters[i]);
  }
}

#define CCU_RS485_TX_THREAD_STACK_SIZE 512
#define CCU_RS485_TX_THREAD_PRIORITY 5

K_THREAD_STACK_DEFINE(ccu_rs485_tx_thread_stack_area, CCU_RS485_TX_THREAD_STACK_SIZE);
struct k_thread ccu_rs485_tx_thread_data;

static void ccu_rs485_tx_thread(void *p1, void *p2, void *p3) {

}

#define CCU_CAN_TX_THREAD_STACK_SIZE 512
#define CCU_CAN_TX_THREAD_PRIORITY 5

K_THREAD_STACK_DEFINE(ccu_can_tx_thread_stack_area, CCU_CAN_TX_THREAD_STACK_SIZE);
struct k_thread ccu_can_tx_thread_data;

static void ccu_can_tx_thread(void *p1, void *p2, void *p3) {
    while (1) {

        // can_send_(can.can_device, CAN_ID_BUTTONS_LIGHTS_MASK, &lights_buttons_mask,
        //           sizeof(lights_buttons_mask));

        k_sleep(K_MSEC(10));
    }
}

void can_converter_init() {
    ccu_can_init();

    k_tid_t ccu_rs485_tx_thread_tid = k_thread_create(&ccu_rs485_tx_thread_data, ccu_rs485_tx_thread_stack_area,
                                     K_THREAD_STACK_SIZEOF(ccu_rs485_tx_thread_stack_area),
                                     ccu_rs485_tx_thread,
                                     NULL, NULL, NULL,
                                     CCU_RS485_TX_THREAD_PRIORITY, 0, K_NO_WAIT);

    k_tid_t ccu_can_tx_thread_data_tid = k_thread_create(&ccu_can_tx_thread_data, ccu_can_tx_thread_stack_area,
                                     K_THREAD_STACK_SIZEOF(ccu_can_tx_thread_stack_area),
                                     ccu_can_tx_thread,
                                     NULL, NULL, NULL,
                                     CCU_CAN_TX_THREAD_PRIORITY, 0, K_NO_WAIT);
}


