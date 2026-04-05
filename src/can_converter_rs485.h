/**
 * @file can_converter_rs485.h
 * @brief RS485 receive path for the CAN converter.
 *
 * Owns the RS485 context, a COBS stream parser, and the receive thread that
 * decodes incoming NanoPB frames and writes them into the shared data store.
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the RS485 subsystem and start the receive thread.
 *
 * Calls rs485_init() on the internal context (configures hardware, registers
 * the UART callback, starts DMA reception), then spawns the parser thread.
 */
void ccu_rs485_init(void);
void ccu_rs485_test_set(bool active);

#ifdef __cplusplus
}
#endif
