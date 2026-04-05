#include "can_converter_frames.h"

#include "can_converter_can.h"
#include "can.h"
#include "candef.h"

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ccu_can, LOG_LEVEL_INF);

static void enqueue_frame(uint32_t id, const uint8_t *data, uint8_t len) {
    struct can_frame frame = {};
    if (len > sizeof(frame.data)) { return; }
    frame.id    = id;
    frame.dlc   = len;
    frame.flags = CAN_FRAME_IDE;
    memcpy(frame.data, data, len);
    if (k_msgq_put(&can_tx_msgq, &frame, K_NO_WAIT) != 0) {
        LOG_WRN("CAN TX queue full");
    }
}

#define PACK_AND_ENQUEUE(UPPER_NAME, lower_name, struct_ptr)                        \
    do {                                                                             \
        uint8_t _buf[CANDEF_##UPPER_NAME##_LENGTH];                                 \
        candef_##lower_name##_pack(_buf, (struct_ptr), sizeof(_buf));               \
        enqueue_frame(CANDEF_##UPPER_NAME##_FRAME_ID, _buf, sizeof(_buf));          \
    } while (0)

/* ── MCU frame encoders ────────────────────────────────────────────────── */

void send_mcu_time_sync(uint32_t ms_tick, uint32_t cycle_tick) {
    struct candef_mcu_time_sync_t frame = {
        .time_ms      = candef_mcu_time_sync_time_ms_encode((double)ms_tick),
        .clock_cycles = candef_mcu_time_sync_clock_cycles_encode((double)cycle_tick),
    };
    PACK_AND_ENQUEUE(MCU_TIME_SYNC, mcu_time_sync, &frame);
}

void send_mcu_state(const MasterStatus *s) {
    struct candef_mcu_state_t frame = {
        .status                   = candef_mcu_state_status_encode(s->state),
        .vehicle_type             = candef_mcu_state_vehicle_type_encode(s->vehicle_type),
        .vehicle_allowed_to_move  = candef_mcu_state_vehicle_allowed_to_move_encode(s->vehicle_allowed_to_move),
        .motor_controller_enabled = candef_mcu_state_motor_controller_enabled_encode(s->motor_controller_enabled),
        .main_valve_enabled       = candef_mcu_state_main_valve_enabled_encode(s->main_valve_enabled),
    };
    PACK_AND_ENQUEUE(MCU_STATE, mcu_state, &frame);
}

void send_mcu_inputs(const MasterMeasurements *m) {
    struct candef_mcu_inputs_t frame = {
        .emergency_switch    = m->din.emergency_switch,
        .dead_mans_switch    = m->din.dead_mans_switch,
        .leakage_detected    = m->din.leakage_detected,
        .shell_relay         = m->din.shell_relay,
        .start_button        = m->din.start_button,
        .reset_button        = m->din.reset_button,
        .calibration_button  = m->din.calibration_button,
        .gas_pedal           = m->din.gas_pedal,
    };
    PACK_AND_ENQUEUE(MCU_INPUTS, mcu_inputs, &frame);
}

void send_mcu_faults(const MasterFaults *f) {
    struct candef_mcu_faults_t frame = {
        .emergency_switch            = f->emg_emergency_switch,
        .emergency_dead_mans_switch  = f->emg_dead_mans_switch,
        .emergency_leakage_detected  = f->emg_leakage_sensor_switch,
        /* Map single SCADE error flags to both low+high DBC fields */
        .error_fc_v_low  = f->err_fuel_cell_output_voltage,
        .error_fc_v_high = f->err_fuel_cell_output_voltage,
        .error_fc_c_low  = f->err_fuel_cell_output_current,
        .error_fc_c_high = f->err_fuel_cell_output_current,
        .error_sc_v_low  = f->err_supercapacitor_voltage,
        .error_sc_v_high = f->err_supercapacitor_voltage,
        .error_sc_c_low  = f->err_supercapacitor_current,
        .error_sc_c_high = f->err_supercapacitor_current,
        .error_mc_v_low  = f->err_motor_controller_supply_voltage,
        .error_mc_v_high = f->err_motor_controller_supply_voltage,
        .error_mc_c_low  = f->err_motor_controller_supply_current,
        .error_mc_c_high = f->err_motor_controller_supply_current,
        .error_ab_v_low  = f->err_accessory_battery_voltage,
        .error_ab_v_high = f->err_accessory_battery_voltage,
        .error_ab_c_low  = f->err_accessory_battery_current,
        .error_ab_c_high = f->err_accessory_battery_current,
    };
    PACK_AND_ENQUEUE(MCU_FAULTS, mcu_faults, &frame);
}

void send_mcu_analog_speed(const MasterMeasurements *m) {
    struct candef_mcu_analog_drive_t frame = {
        .rpm   = candef_mcu_analog_drive_rpm_encode((double)m->rpm),
        .speed = candef_mcu_analog_drive_speed_encode((double)m->speed),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_DRIVE, mcu_analog_drive, &frame);
}

void send_mcu_analog_pedals(const MasterMeasurements *m) {
    struct candef_mcu_analog_pedals_t frame = {
        .acceleration_pedal_voltage =
            candef_mcu_analog_pedals_acceleration_pedal_voltage_encode(
                (double)m->accel_pedal_voltage),
        .brake_pedal_voltage =
            candef_mcu_analog_pedals_brake_pedal_voltage_encode(
                (double)m->brake_pedal_voltage),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_PEDALS, mcu_analog_pedals, &frame);
}

void send_mcu_analog_powertrain(const MasterMeasurements *m) {
    struct candef_mcu_analog_powertrain_t frame = {
        .supercapacitor_voltage =
            candef_mcu_analog_powertrain_supercapacitor_voltage_encode(
                (double)m->supercapacitor_voltage),
        .supercapacitor_current =
            candef_mcu_analog_powertrain_supercapacitor_current_encode(
                (double)m->supercapacitor_current),
        .motor_controller_supply_current =
            candef_mcu_analog_powertrain_motor_controller_supply_current_encode(
                (double)m->motor_controller_supply_current),
        .motor_controller_supply_voltage =
            candef_mcu_analog_powertrain_motor_controller_supply_voltage_encode(
                (double)m->motor_controller_supply_voltage),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_POWERTRAIN, mcu_analog_powertrain, &frame);
}

void send_mcu_analog_fuel_cell(const MasterMeasurements *m) {
    struct candef_mcu_analog_fuel_cell_t frame = {
        .fuel_cell_output_current =
            candef_mcu_analog_fuel_cell_fuel_cell_output_current_encode(
                (double)m->fuel_cell_output_current),
        .fuel_cell_output_voltage =
            candef_mcu_analog_fuel_cell_fuel_cell_output_voltage_encode(
                (double)m->fuel_cell_output_voltage),
        .hydrogen_high_pressure =
            candef_mcu_analog_fuel_cell_hydrogen_high_pressure_encode(
                (double)m->hydrogen_high_pressure),
        .hydrogen_leakage_sensor_voltage =
            candef_mcu_analog_fuel_cell_hydrogen_leakage_sensor_voltage_encode(
                (double)m->hydrogen_leakage_sensor_voltage),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_FUEL_CELL, mcu_analog_fuel_cell, &frame);
}

void send_mcu_analog_accessory(const MasterMeasurements *m) {
    struct candef_mcu_analog_accessory_t frame = {
        .accessory_battery_current =
            candef_mcu_analog_accessory_accessory_battery_current_encode(
                (double)m->accessory_battery_current),
        .accessory_battery_voltage =
            candef_mcu_analog_accessory_accessory_battery_voltage_encode(
                (double)m->accessory_battery_voltage),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_ACCESSORY, mcu_analog_accessory, &frame);
}

void send_mcu_analog_unassigned(const MasterMeasurements *m) {
    struct candef_mcu_analog_unassigned_t frame = {
        .ai1 = candef_mcu_analog_unassigned_ai1_encode((double)m->ai1),
        .ai3 = candef_mcu_analog_unassigned_ai3_encode((double)m->ai3),
        .ai4 = candef_mcu_analog_unassigned_ai4_encode((double)m->ai4),
        .ai5 = candef_mcu_analog_unassigned_ai5_encode((double)m->ai5),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_UNASSIGNED, mcu_analog_unassigned, &frame);
}

/* ── Protium frame encoders ────────────────────────────────────────────── */

void send_protium_state(ProtiumOperatingState operating_state) {
    struct candef_protium_state_t frame = {
        .operating_state =
            candef_protium_state_operating_state_encode(operating_state),
    };
    PACK_AND_ENQUEUE(PROTIUM_STATE, protium_state, &frame);
}

void send_protium_power(const ProtiumValues *v) {
    struct candef_protium_power_t frame = {
        .fc_v   = candef_protium_power_fc_v_encode((double)v->fc_v),
        .fc_a   = candef_protium_power_fc_a_encode((double)v->fc_a),
        .fc_w   = candef_protium_power_fc_w_encode((double)v->fc_w),
        .energy = candef_protium_power_energy_encode((double)v->energy),
    };
    PACK_AND_ENQUEUE(PROTIUM_POWER, protium_power, &frame);
}

void send_protium_thermal(const ProtiumValues *v) {
    struct candef_protium_thermal_t frame = {
        .fct1 = candef_protium_thermal_fct1_encode((double)v->fct1),
        .fct2 = candef_protium_thermal_fct2_encode((double)v->fct2),
        .fan  = candef_protium_thermal_fan_encode((double)v->fan),
        .blw  = candef_protium_thermal_blw_encode((double)v->blw),
    };
    PACK_AND_ENQUEUE(PROTIUM_THERMAL, protium_thermal, &frame);
}

void send_protium_hydrogen(const ProtiumValues *v) {
    struct candef_protium_hydrogen_t frame = {
        .h2_p1 = candef_protium_hydrogen_h2_p1_encode((double)v->h2p1),
        .h2_p2 = candef_protium_hydrogen_h2_p2_encode((double)v->h2p2),
        .tank_p = candef_protium_hydrogen_tank_p_encode((double)v->tank_p),
        .tank_t = candef_protium_hydrogen_tank_t_encode((double)v->tank_t),
    };
    PACK_AND_ENQUEUE(PROTIUM_HYDROGEN, protium_hydrogen, &frame);
}

void send_protium_setpoints(const ProtiumValues *v) {
    struct candef_protium_setpoints_t frame = {
        .v_set           = candef_protium_setpoints_v_set_encode((double)v->v_set),
        .i_set           = candef_protium_setpoints_i_set_encode((double)v->i_set),
        .ucb_v           = candef_protium_setpoints_ucb_v_encode((double)v->ucb_v),
        .number_of_cells = candef_protium_setpoints_number_of_cells_encode((double)v->number_of_cell),
    };
    PACK_AND_ENQUEUE(PROTIUM_SETPOINTS, protium_setpoints, &frame);
}

void send_protium_stasis(const ProtiumValues *v) {
    struct candef_protium_stasis_t frame = {
        .stasis_selector = candef_protium_stasis_stasis_selector_encode((double)v->stasis_selector),
        .stasis_v1       = candef_protium_stasis_stasis_v1_encode((double)v->stasis_v1),
        .stasis_v2       = candef_protium_stasis_stasis_v2_encode((double)v->stasis_v2),
        .batt_v          = candef_protium_stasis_batt_v_encode((double)v->batt_v),
    };
    PACK_AND_ENQUEUE(PROTIUM_STASIS, protium_stasis, &frame);
}

void send_protium_misc(const ProtiumValues *v) {
    struct candef_protium_misc_t frame = {
        .ip = candef_protium_misc_ip_encode((double)v->ip),
        .tp = candef_protium_misc_tp_encode((double)v->tp),
    };
    PACK_AND_ENQUEUE(PROTIUM_MISC, protium_misc, &frame);
}
