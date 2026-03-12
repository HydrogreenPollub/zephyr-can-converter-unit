#include "can_converter_can.hpp"
#include "candef.h"

LOG_MODULE_REGISTER(ccu_can, LOG_LEVEL_INF);

#define CCU_CAN_TX_THREAD_STACK_SIZE       2048
#define CCU_CAN_TX_THREAD_PRIORITY         5
#define CCU_CAN_PERIODIC_THREAD_STACK_SIZE 2048
#define CCU_CAN_PERIODIC_THREAD_PRIORITY   5

K_THREAD_STACK_DEFINE(ccu_can_tx_thread_stack_area, CCU_CAN_TX_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(ccu_can_periodic_thread_stack_area, CCU_CAN_PERIODIC_THREAD_STACK_SIZE);

struct k_thread ccu_can_tx_thread_data;
struct k_thread ccu_can_periodic_thread_data;

K_SEM_DEFINE(can_tx_done_sem, 0, 1);
K_MSGQ_DEFINE(can_tx_msgq, sizeof(struct can_frame), 32, 4);

static volatile int can_tx_result;
static struct k_work_delayable tx_led_off_work;

struct can_filter ccu_can_filter = {
    .id    = CANDEF_MCU_TIME_SYNC_FRAME_ID,
    .mask  = 0x7C0,
    .flags = CAN_FRAME_IDE,
};

ccu_can_t can = {
    .device = DEVICE_DT_GET(DT_ALIAS(can)),
    .tx_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_tx_led), gpios),
    .rx_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_rx_led), gpios),
};

static void ccu_can_tx_callback(const struct device *dev, int error, void *user_data);

static void tx_led_off_handler(struct k_work *work)
{
    gpio_reset(&can.tx_led);
}

static void enqueue_frame(uint32_t id, const uint8_t *data, uint8_t len)
{
    struct can_frame frame = {};

    if (len > sizeof(frame.data)) {
        LOG_WRN("CAN payload too big: %u", len);
        return;
    }

    frame.id    = id;
    frame.dlc   = len;
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

static void send_mcu_state(const MasterStatus *s)
{
    struct candef_mcu_state_t frame = {
        .status = candef_mcu_state_status_encode(s->state),
    };
    PACK_AND_ENQUEUE(MCU_STATE, mcu_state, &frame);
}

static void send_mcu_inputs(const MasterMeasurements *m)
{
    struct candef_mcu_inputs_t frame = {
        .gas_pedal = (uint8_t)((m->din >> 0) & 1u),
    };
    PACK_AND_ENQUEUE(MCU_INPUTS, mcu_inputs, &frame);
}

static void send_mcu_analog_drive(const MasterMeasurements *m)
{
    struct candef_mcu_analog_drive_t frame = {
        .rpm   = candef_mcu_analog_drive_rpm_encode(m->rpm),
        .speed = candef_mcu_analog_drive_speed_encode(m->speed),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_DRIVE, mcu_analog_drive, &frame);
}

static void send_mcu_analog_pedals(const MasterMeasurements *m)
{
    struct candef_mcu_analog_pedals_t frame = {
        .acceleration_pedal_voltage = candef_mcu_analog_pedals_acceleration_pedal_voltage_encode(m->accelPedalVoltage),
        .brake_pedal_voltage        = candef_mcu_analog_pedals_brake_pedal_voltage_encode(m->brakePedalVoltage),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_PEDALS, mcu_analog_pedals, &frame);
}

static void send_mcu_analog_powertrain(const MasterMeasurements *m)
{
    struct candef_mcu_analog_powertrain_t frame = {
        .supercapacitor_voltage          = candef_mcu_analog_powertrain_supercapacitor_voltage_encode(m->supercapacitorVoltage),
        .supercapacitor_current          = candef_mcu_analog_powertrain_supercapacitor_current_encode(m->supercapacitorCurrent),
        .motor_controller_supply_current = candef_mcu_analog_powertrain_motor_controller_supply_current_encode(m->motorControllerSupplyCurrent),
        .motor_controller_supply_voltage = candef_mcu_analog_powertrain_motor_controller_supply_voltage_encode(m->motorControllerSupplyVoltage),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_POWERTRAIN, mcu_analog_powertrain, &frame);
}

static void send_mcu_analog_fuel_cell(const MasterMeasurements *m)
{
    struct candef_mcu_analog_fuel_cell_t frame = {
        .fuel_cell_output_current        = candef_mcu_analog_fuel_cell_fuel_cell_output_current_encode(m->fuelCellOutputCurrent),
        .fuel_cell_output_voltage        = candef_mcu_analog_fuel_cell_fuel_cell_output_voltage_encode(m->fuelCellOutputVoltage),
        .hydrogen_high_pressure          = candef_mcu_analog_fuel_cell_hydrogen_high_pressure_encode(m->hydrogenHighPressure),
        .hydrogen_leakage_sensor_voltage = candef_mcu_analog_fuel_cell_hydrogen_leakage_sensor_voltage_encode(m->hydrogenLeakageSensorVoltage),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_FUEL_CELL, mcu_analog_fuel_cell, &frame);
}

static void send_mcu_analog_accessory(const MasterMeasurements *m)
{
    struct candef_mcu_analog_accessory_t frame = {
        .accessory_battery_current = candef_mcu_analog_accessory_accessory_battery_current_encode(m->accessoryBatteryCurrent),
        .accessory_battery_voltage = candef_mcu_analog_accessory_accessory_battery_voltage_encode(m->accessoryBatteryVoltage),
    };
    PACK_AND_ENQUEUE(MCU_ANALOG_ACCESSORY, mcu_analog_accessory, &frame);
}

static void send_protium_state(ProtiumOperatingState operating_state)
{
    struct candef_protium_state_t frame = {
        .operating_state = candef_protium_state_operating_state_encode(operating_state),
    };
    PACK_AND_ENQUEUE(PROTIUM_STATE, protium_state, &frame);
}

static void ccu_can_tx_thread(void *p1, void *p2, void *p3)
{
    struct can_frame frame = {0};
    int ret;
    LOG_INF("CAN TX thread started");

    while (1) {
        k_msgq_get(&can_tx_msgq, &frame, K_FOREVER);

        ret = can_send(can.device, &frame, K_MSEC(100), ccu_can_tx_callback, NULL);
        if (!ret) {
            if (k_sem_take(&can_tx_done_sem, K_MSEC(200)) == 0) {
                if (can_tx_result == 0) {
                    gpio_set(&can.tx_led);
                    k_work_reschedule(&tx_led_off_work, K_MSEC(50));
                } else {
                    LOG_ERR("CAN TX error: %d", can_tx_result);
                }
            } else {
                LOG_ERR("CAN TX timeout");
            }
        } else {
            LOG_ERR("CAN send failed: %d", ret);
        }
    }
}

static void ccu_can_periodic_thread(void *p1, void *p2, void *p3)
{
    uint8_t  cnt_100ms  = 0;
    uint8_t  cnt_1000ms = 0;
    uint16_t fake_tick  = 0;
    float    fake_accel = 0.0f;  // persists across iterations

    while (1) {
        k_sleep(K_MSEC(10));

        master_data_t d = {};
        k_mutex_lock(&data_mutex, K_FOREVER);
        d = data;
        k_mutex_unlock(&data_mutex);

        if (d.master_measurements_valid) {
            send_mcu_analog_drive(&d.master_measurements);
        }

        cnt_100ms++;
        cnt_1000ms++;

        if (cnt_1000ms >= 100) {
            cnt_1000ms = 0;

            fake_accel = (float)(fake_tick % 100);
            fake_tick++;

            if (d.protium_operating_state_valid) {
                send_protium_state(d.protium_operating_state);
            }
        }

        if (cnt_100ms >= 10) {
            cnt_100ms = 0;

            d.master_measurements.fuelCellOutputVoltage = 21.0f;
            d.master_measurements.supercapacitorVoltage = 37.0f;
            d.master_measurements.accelPedalVoltage     = fake_accel;

            send_mcu_analog_pedals(&d.master_measurements);
            send_mcu_analog_powertrain(&d.master_measurements);
            send_mcu_analog_fuel_cell(&d.master_measurements);
            send_mcu_analog_accessory(&d.master_measurements);
            send_mcu_inputs(&d.master_measurements);
            send_mcu_state(&d.master_status);
        }
    }
}

static void ccu_can_tx_callback(const struct device *dev, int error, void *user_data)
{
    can_tx_result = error;
    k_sem_give(&can_tx_done_sem);
}

void ccu_can_init(void)
{
    can_init(can.device, HYDROGREEN_CAN_BAUD_RATE);
    gpio_init(&can.rx_led, GPIO_OUTPUT_INACTIVE);
    gpio_init(&can.tx_led, GPIO_OUTPUT_INACTIVE);
    k_work_init_delayable(&tx_led_off_work, tx_led_off_handler);

    k_tid_t tx_tid = k_thread_create(
        &ccu_can_tx_thread_data, ccu_can_tx_thread_stack_area,
        K_THREAD_STACK_SIZEOF(ccu_can_tx_thread_stack_area),
        ccu_can_tx_thread, NULL, NULL, NULL,
        CCU_CAN_TX_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(tx_tid, "can_tx");

    k_tid_t periodic_tid = k_thread_create(
        &ccu_can_periodic_thread_data, ccu_can_periodic_thread_stack_area,
        K_THREAD_STACK_SIZEOF(ccu_can_periodic_thread_stack_area),
        ccu_can_periodic_thread, NULL, NULL, NULL,
        CCU_CAN_PERIODIC_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(periodic_tid, "can_periodic");
}