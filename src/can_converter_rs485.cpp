//
// Created by inż. Dawid Pisarczyk on 08.03.2026.
//

#include "can_converter_rs485.hpp"

LOG_MODULE_REGISTER(ccu_rs485, LOG_LEVEL_INF);

#define CCU_RS485_TX_THREAD_STACK_SIZE 1024
#define CCU_RS485_TX_THREAD_PRIORITY 5
#define CCU_RS485_PARSER_THREAD_STACK_SIZE 1024
#define CCU_RS485_PARSER_THREAD_PRIORITY 5

K_THREAD_STACK_DEFINE(ccu_rs485_tx_thread_stack_area, CCU_RS485_TX_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(ccu_rs485_parser_thread_stack_area, CCU_RS485_PARSER_THREAD_STACK_SIZE);

struct k_thread ccu_rs485_tx_thread_data;
struct k_thread ccu_rs485_parser_thread_data;

K_MSGQ_DEFINE(rs485_rx_msgq, sizeof(rs485_packet_t), 16, 4);

static HmiParser parser;


ccu_rs485_t rs485 = {
    .device = DEVICE_DT_GET(DT_ALIAS(rs485)),
    .dir_pin = GPIO_DT_SPEC_GET(DT_ALIAS(rs485_dir), gpios),
    .tx_led = GPIO_DT_SPEC_GET(DT_ALIAS(rs485_tx_led), gpios),
    .rx_led = GPIO_DT_SPEC_GET(DT_ALIAS(rs485_rx_led), gpios),
    .rx_buf = {0},
    .rx_buf_next = {0},
};

void rs485_callback(const struct device *dev, struct uart_event *evt, void *user_data) {
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    switch (evt->type) {
        case UART_RX_RDY: {

            rs485_packet_t packet = {};
            LOG_HEXDUMP_INF(&evt->data.rx.buf[evt->data.rx.offset], evt->data.rx.len, "RS485 Rx data");

            size_t len = MIN(evt->data.rx.len, sizeof(packet.data));
            memcpy(packet.data, evt->data.rx.buf + evt->data.rx.offset, len);
            packet.len = len;
            if (k_msgq_put(&rs485_rx_msgq, &packet, K_NO_WAIT) != 0) {
                LOG_WRN("RS485 RX queue full");
            }
            break;
        }
        case UART_RX_BUF_REQUEST: {
            uart_rx_buf_rsp(rs485.device, rs485.rx_buf_next, sizeof(rs485.rx_buf_next));
            break;
        }
        case UART_RX_DISABLED: {
            LOG_INF("RS485 RX disabled");
            int ret = uart_rx_enable(rs485.device, rs485.rx_buf, sizeof(rs485.rx_buf), 100);
            if (!ret)
                LOG_INF("RS485 RX reconfigured");
            break;
        }
        case UART_TX_DONE: {
            LOG_INF("RS485 TX done");
            k_sem_give(&tx_done_sem);
            break;
        }
        case UART_TX_ABORTED: {
            LOG_INF("RS485 TX aborted");
            rs485_set_rx(&rs485.dir_pin);
            k_mutex_unlock(&tx_mutex);
            break;
        }
        default: {
            break;
        }
    }
}

static void rs485_parser_callbacks_init()
{
    parser.onMasterMeasurements =
    [](uint32_t msClockTickCount, uint32_t cycleClockTickCount, const MasterMeasurements& measurements)
    {
        ARG_UNUSED(msClockTickCount);
        ARG_UNUSED(cycleClockTickCount);

        k_mutex_lock(&data_mutex, K_FOREVER);
        data.master_measurements = measurements;
        data.master_measurements_valid = true;
        k_mutex_unlock(&data_mutex);
    };

    parser.onMasterStatus =
    [](uint32_t msClockTickCount, uint32_t cycleClockTickCount, const MasterStatus& status)
    {
        ARG_UNUSED(msClockTickCount);
        ARG_UNUSED(cycleClockTickCount);

        k_mutex_lock(&data_mutex, K_FOREVER);
        data.master_status = status;
        data.master_status_valid = true;
        k_mutex_unlock(&data_mutex);
    };

    parser.onProtiumValues =
    [](uint32_t msClockTickCount, uint32_t cycleClockTickCount, const ProtiumValues& values)
    {
        ARG_UNUSED(msClockTickCount);
        ARG_UNUSED(cycleClockTickCount);

        k_mutex_lock(&data_mutex, K_FOREVER);
        data.protium_values = values;
        data.protium_values_valid = true;
        k_mutex_unlock(&data_mutex);
    };

    parser.onProtiumOperatingState =
    [](uint32_t msClockTickCount, uint32_t cycleClockTickCount, ProtiumOperatingState currentOperatingState, const ProtiumOperatingStateLogEntry(&operatingStateLogEntries)[8]) {

        ARG_UNUSED(msClockTickCount);
        ARG_UNUSED(cycleClockTickCount);

        k_mutex_lock(&data_mutex, K_FOREVER);
        data.protium_operating_state = currentOperatingState;
        data.protium_operating_state_valid = true;
        k_mutex_unlock(&data_mutex);
    };
}


// uint8_t data1[2] = {0, 0x1};
// static void ccu_rs485_tx_thread(void *p1, void *p2, void *p3) {
//     while (1) {
//
//         if (rs485_send(rs485.device, &rs485.dir_pin, data1, sizeof(data1))!=0) {
//             LOG_INF("RS485 fail sent");
//         }
//         else {
//             LOG_INF("RS485 send success");
//         }
//
//         k_sleep(K_MSEC(100));
//     }
// }

static void ccu_rs485_parser_thread(void *p1, void *p2, void *p3)
{
    rs485_packet_t packet;

    while (1) {
        k_msgq_get(&rs485_rx_msgq, &packet, K_FOREVER);

        for (size_t i = 0; i < packet.len; i++) {
            parser.processOctet(packet.data[i]);
        }
    }
}

void ccu_rs485_init() {
    gpio_init(&rs485.rx_led, GPIO_OUTPUT_INACTIVE);
    gpio_init(&rs485.tx_led, GPIO_OUTPUT_INACTIVE);
    rs485_init(rs485.device, &rs485.dir_pin);
    uart_callback_set_(rs485.device, rs485_callback);
    uart_rx_init(rs485.device, rs485.rx_buf, sizeof(rs485.rx_buf), 100);
    rs485_parser_callbacks_init();

    // k_tid_t ccu_rs485_tx_thread_tid = k_thread_create(&ccu_rs485_tx_thread_data, ccu_rs485_tx_thread_stack_area,
    //                                  K_THREAD_STACK_SIZEOF(ccu_rs485_tx_thread_stack_area),
    //                                  ccu_rs485_tx_thread,
    //                                  NULL, NULL, NULL,
    //                                  CCU_RS485_TX_THREAD_PRIORITY, 0, K_NO_WAIT);
    // k_thread_name_set(ccu_rs485_tx_thread_tid, "rs485_tx");

    k_tid_t ccu_rs485_parser_thread_tid = k_thread_create(&ccu_rs485_parser_thread_data, ccu_rs485_parser_thread_stack_area,
                                     K_THREAD_STACK_SIZEOF(ccu_rs485_parser_thread_stack_area),
                                     ccu_rs485_parser_thread,
                                     NULL, NULL, NULL,
                                     CCU_RS485_PARSER_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(ccu_rs485_parser_thread_tid, "rs485_parser");


}