/**
 * @file master_helpers.h
 * @brief Hand-written helpers that complement the NanoPB-generated master.pb.h.
 *
 * Include this alongside master.pb.h wherever you need the convenience
 * utilities below.
 */

#pragma once

#include <stdint.h>

#include "proto/master.pb.h"

/**
 * @brief Construct a DigitalInputs struct from a raw bitmask.
 *
 * Useful on the sender side when reading a hardware GPIO register and
 * assigning the whole value at once:
 * @code
 *   measurements.din = digital_inputs_from_raw(gpio_register);
 * @endcode
 *
 * Bit positions match the DIN pin order on the TM4C master board:
 *   bit 0 — gas_pedal         (DIN1)
 *   bit 1 — start_button      (DIN2)
 *   bit 2 — dead_mans_switch  (DIN3)
 *   bit 3 — emergency_switch  (DIN4)
 *   bit 4 — reset_button      (DIN5)
 *   bit 5 — calibration_button(DIN6)
 *   bit 6 — leakage_detected  (DIN7)
 *   bit 7 — shell_relay       (DIN8)
 */
static inline DigitalInputs digital_inputs_from_raw(uint32_t raw) {
    DigitalInputs din = DigitalInputs_init_zero;
    din.gas_pedal          = (raw & (1u << 0)) != 0u;
    din.start_button       = (raw & (1u << 1)) != 0u;
    din.dead_mans_switch   = (raw & (1u << 2)) != 0u;
    din.emergency_switch   = (raw & (1u << 3)) != 0u;
    din.reset_button       = (raw & (1u << 4)) != 0u;
    din.calibration_button = (raw & (1u << 5)) != 0u;
    din.leakage_detected   = (raw & (1u << 6)) != 0u;
    din.shell_relay        = (raw & (1u << 7)) != 0u;
    return din;
}
