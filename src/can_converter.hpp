//
// Created by inż.Dawid Pisarczyk on 28.12.2025.
//

#ifndef CAN_CONVERTER_H
#define CAN_CONVERTER_H

#include <stdint.h>
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include "hmi_parser_zephyr.hpp"

typedef struct {
    MasterMeasurements master_measurements;
    bool master_measurements_valid;

    MasterStatus master_status;
    bool master_status_valid;

    ProtiumValues protium_values;
    bool protium_values_valid;

    ProtiumOperatingState protium_operating_state;
    bool protium_operating_state_valid;
}master_data_t;

extern master_data_t data;
extern struct k_mutex data_mutex;




#endif // CAN_CONVERTER_H
