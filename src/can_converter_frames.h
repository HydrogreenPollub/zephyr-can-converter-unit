/**
 * @file can_converter_frames.h
 * @brief CAN frame encoding for the CAN converter.
 *
 * Each send_*() function packs the relevant signals from the shared data
 * store into a candef frame and enqueues it onto the CAN TX queue.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "can_converter.h"

/* ── MCU frames ────────────────────────────────────────────────────────── */

/** @brief Enqueue MCU_TIME_SYNC frame (clock ticks). */
void send_mcu_time_sync(uint32_t ms_tick, uint32_t cycle_tick);

/** @brief Enqueue MCU_STATE frame (state machine + actuator status). */
void send_mcu_state(const MasterStatus *s);

/** @brief Enqueue MCU_INPUTS frame (all digital inputs). */
void send_mcu_inputs(const MasterMeasurements *m);

/** @brief Enqueue MCU_FAULTS frame (emergency + sensor error flags). */
void send_mcu_faults(const MasterFaults *f);

/** @brief Enqueue MCU_ANALOG_SPEED frame (RPM, speed). */
void send_mcu_analog_speed(const MasterMeasurements *m);

/** @brief Enqueue MCU_ANALOG_PEDALS frame (accel/brake voltages). */
void send_mcu_analog_pedals(const MasterMeasurements *m);

/** @brief Enqueue MCU_ANALOG_POWERTRAIN frame (supercap, MC voltage/current). */
void send_mcu_analog_powertrain(const MasterMeasurements *m);

/** @brief Enqueue MCU_ANALOG_FUEL_CELL frame (FC voltage/current, H2). */
void send_mcu_analog_fuel_cell(const MasterMeasurements *m);

/** @brief Enqueue MCU_ANALOG_ACCESSORY frame (accessory battery). */
void send_mcu_analog_accessory(const MasterMeasurements *m);

/** @brief Enqueue MCU_ANALOG_UNASSIGNED frame (AI1, AI3, AI4, AI5). */
void send_mcu_analog_unassigned(const MasterMeasurements *m);

/* ── Protium frames ────────────────────────────────────────────────────── */

/** @brief Enqueue PROTIUM_STATE frame (operating state enum). */
void send_protium_state(ProtiumOperatingState operating_state);

/** @brief Enqueue PROTIUM_POWER frame (fc_v, fc_a, fc_w, energy). */
void send_protium_power(const ProtiumValues *v);

/** @brief Enqueue PROTIUM_THERMAL frame (fct1, fct2, fan, blw). */
void send_protium_thermal(const ProtiumValues *v);

/** @brief Enqueue PROTIUM_HYDROGEN frame (h2p1, h2p2, tank_p, tank_t). */
void send_protium_hydrogen(const ProtiumValues *v);

/** @brief Enqueue PROTIUM_SETPOINTS frame (v_set, i_set, ucb_v, cells). */
void send_protium_setpoints(const ProtiumValues *v);

/** @brief Enqueue PROTIUM_STASIS frame (selector, v1, v2, batt_v). */
void send_protium_stasis(const ProtiumValues *v);

/** @brief Enqueue PROTIUM_MISC frame (ip, tp). */
void send_protium_misc(const ProtiumValues *v);

#ifdef __cplusplus
}
#endif
