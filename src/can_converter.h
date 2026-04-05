/**
 * @file can_converter.h
 * @brief Shared data store for the CAN↔RS485 converter.
 *
 * Holds the latest values decoded from the RS485 master stream.  All fields
 * are protected by @ref data_mutex; always lock before reading or writing.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include <zephyr/kernel.h>

#include "proto/master.pb.h"

/**
 * @brief Snapshot of the most recently received master data.
 *
 * Each payload type has a companion @c _valid flag that starts @c false and
 * is set to @c true the first time that payload is successfully decoded.
 */
typedef struct {
    MasterMeasurements master_measurements;
    bool               master_measurements_valid;

    MasterStatus master_status;
    bool         master_status_valid;

    ProtiumValues protium_values;
    bool          protium_values_valid;

    ProtiumOperatingState protium_operating_state;
    bool                  protium_operating_state_valid;

    MasterFaults master_faults;
    bool         master_faults_valid;
} master_data_t;

/** Global data store — lock @ref data_mutex before accessing. */
extern master_data_t data;

/** Mutex protecting @ref data. */
extern struct k_mutex data_mutex;

/* Board-specific initialisation: status LED, test button, CAN status broadcast.
 * Called from main() after all subsystems are up. */
void ccu_board_init(void);

#ifdef __cplusplus
}
#endif
