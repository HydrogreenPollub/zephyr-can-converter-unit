#ifndef CAN_CONVERTER_DFU_H
#define CAN_CONVERTER_DFU_H

#include "can_dfu.h" /* generic peripheral — provides can_dfu_on_frame / can_dfu_is_active */

/* CAN IDs used by the CCU DFU protocol (extended frames) */
#define CCU_DFU_CMD_ID  0x7E0U  /* updater -> device: REQUEST or COMMIT */
#define CCU_DFU_DATA_ID 0x7E1U  /* updater -> device: firmware chunks   */
#define CCU_DFU_RSP_ID  0x7E2U  /* device  -> updater: status responses */

void ccu_dfu_init(void);

#endif /* CAN_CONVERTER_DFU_H */
