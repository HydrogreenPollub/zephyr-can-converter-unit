#ifndef CAN_CONVERTER_CAN_SEND_H
#define CAN_CONVERTER_CAN_SEND_H

#include "can_converter.hpp"

void send_mcu_state(const MasterStatus *s);

void send_mcu_inputs(const MasterMeasurements *m);

void send_mcu_analog_drive(const MasterMeasurements *m);

void send_mcu_analog_pedals(const MasterMeasurements *m);

void send_mcu_analog_powertrain(const MasterMeasurements *m);

void send_mcu_analog_fuel_cell(const MasterMeasurements *m);

void send_mcu_analog_accessory(const MasterMeasurements *m);

void send_protium_state(ProtiumOperatingState operating_state);

#endif // CAN_CONVERTER_CAN_SEND_H
