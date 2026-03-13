#include "can_converter_can_send.h"
#include "can_converter_can.h"
#include "candef.h"

LOG_MODULE_DECLARE(ccu_can, LOG_LEVEL_INF);

static void enqueue_frame(uint32_t id, const uint8_t *data, uint8_t len) {
    struct can_frame frame = {};

    if (len > sizeof(frame.data)) {
        LOG_WRN("CAN payload too big: %u", len);
        return;
    }

    frame.id = id;
    frame.dlc = len;
    frame.flags = CAN_FRAME_IDE;
    memcpy(frame.data, data, len);

    if (k_msgq_put(&can_tx_msgq, &frame, K_NO_WAIT) != 0) {
        LOG_WRN("CAN TX queue full");
    }
}

#define PACK_AND_ENQUEUE(UPPER_NAME, lower_name, struct_ptr)               \
    do {                                                                   \
        uint8_t _buf[CANDEF_##UPPER_NAME##_LENGTH];                        \
        candef_##lower_name##_pack(_buf, (struct_ptr), sizeof(_buf));      \
        enqueue_frame(CANDEF_##UPPER_NAME##_FRAME_ID, _buf, sizeof(_buf)); \
    } while (0)

void send_mcu_state(const MasterStatus *s) {
    struct candef_mcu_state_t frame = {
        .status = candef_mcu_state_status_encode(s->state),
    };
    PACK_AND_ENQUEUE(MCU_STATE, mcu_state, &frame);
}

void send_mcu_inputs(const MasterMeasurements *m) {
    struct candef_mcu_inputs_t frame = {
        .gas_pedal = (uint8_t) ((m->din >> 0) & 1u),
    };
    PACK_AND_ENQUEUE(MCU_INPUTS, mcu_inputs, &frame);
}

void send_mcu_analog_drive(const MasterMeasurements *m) {
    struct candef_mcu_analog_drive_t frame = {
        .rpm = candef_mcu_analog_drive_rpm_encode(m->rpm),
        .speed = candef_mcu_analog_drive_speed_encode(m->speed),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_DRIVE, mcu_analog_drive, &frame);
}

void send_mcu_analog_pedals(const MasterMeasurements *m) {
    struct candef_mcu_analog_pedals_t frame = {
        .acceleration_pedal_voltage = candef_mcu_analog_pedals_acceleration_pedal_voltage_encode(m->accel_pedal_voltage),
        .brake_pedal_voltage = candef_mcu_analog_pedals_brake_pedal_voltage_encode(m->brake_pedal_voltage),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_PEDALS, mcu_analog_pedals, &frame);
}

void send_mcu_analog_powertrain(const MasterMeasurements *m) {
    struct candef_mcu_analog_powertrain_t frame = {
        .supercapacitor_voltage = candef_mcu_analog_powertrain_supercapacitor_voltage_encode(m->supercapacitor_voltage),
        .supercapacitor_current = candef_mcu_analog_powertrain_supercapacitor_current_encode(m->supercapacitor_current),
        .motor_controller_supply_current = candef_mcu_analog_powertrain_motor_controller_supply_current_encode(
            m->motor_controller_supply_current),
        .motor_controller_supply_voltage = candef_mcu_analog_powertrain_motor_controller_supply_voltage_encode(
            m->motor_controller_supply_voltage),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_POWERTRAIN, mcu_analog_powertrain, &frame);
}

void send_mcu_analog_fuel_cell(const MasterMeasurements *m) {
    struct candef_mcu_analog_fuel_cell_t frame = {
        .fuel_cell_output_current = candef_mcu_analog_fuel_cell_fuel_cell_output_current_encode(
            m->fuel_cell_output_current),
        .fuel_cell_output_voltage = candef_mcu_analog_fuel_cell_fuel_cell_output_voltage_encode(
            m->fuel_cell_output_voltage),
        .hydrogen_high_pressure = candef_mcu_analog_fuel_cell_hydrogen_high_pressure_encode(m->hydrogen_high_pressure),
        .hydrogen_leakage_sensor_voltage = candef_mcu_analog_fuel_cell_hydrogen_leakage_sensor_voltage_encode(
            m->hydrogen_leakage_sensor_voltage),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_FUEL_CELL, mcu_analog_fuel_cell, &frame);
}

void send_mcu_analog_accessory(const MasterMeasurements *m) {
    struct candef_mcu_analog_accessory_t frame = {
        .accessory_battery_current = candef_mcu_analog_accessory_accessory_battery_current_encode(
            m->accessory_battery_current),
        .accessory_battery_voltage = candef_mcu_analog_accessory_accessory_battery_voltage_encode(
            m->accessory_battery_voltage),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_ACCESSORY, mcu_analog_accessory, &frame);
}

void send_protium_state(ProtiumOperatingState operating_state) {
    struct candef_protium_state_t frame = {
        .operating_state = candef_protium_state_operating_state_encode(operating_state),
    };
    PACK_AND_ENQUEUE(PROTIUM_STATE, protium_state, &frame);
}
