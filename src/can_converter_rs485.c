//
// Created by inż. Dawid Pisarczyk on 08.03.2026.
//

#include "can_converter_rs485.h"
#include <pb_decode.h>
#include "proto/master.pb.h"
#include "cobs.h"

LOG_MODULE_REGISTER(ccu_rs485, LOG_LEVEL_INF);

#define CCU_RS485_PARSER_THREAD_STACK_SIZE 1024
#define CCU_RS485_PARSER_THREAD_PRIORITY   5

K_THREAD_STACK_DEFINE(ccu_rs485_parser_thread_stack_area, CCU_RS485_PARSER_THREAD_STACK_SIZE);
struct k_thread ccu_rs485_parser_thread_data;

K_MSGQ_DEFINE(rs485_rx_msgq, sizeof(rs485_packet_t), 16, 4);

/* COBS frame accumulation state (used only in the parser thread) */
#define COBS_FRAME_BUF_SIZE 256
static uint8_t cobs_frame_buf[COBS_FRAME_BUF_SIZE];
static size_t  cobs_frame_len = 0;
static bool    cobs_in_frame  = false;

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
            rs485_packet_t packet = {0};
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
            if (!ret) {
                LOG_INF("RS485 RX reconfigured");
            }
            break;
        }
        case UART_TX_DONE: {
            LOG_INF("RS485 TX done");
            rs485_on_tx_done();
            break;
        }
        case UART_TX_ABORTED: {
            LOG_INF("RS485 TX aborted");
            rs485_on_tx_aborted(&rs485.dir_pin);
            break;
        }
        default:
            break;
    }
}

static void process_master_frame(const uint8_t *buf, size_t len) {
    MasterFrame frame = MasterFrame_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(buf, len);

    if (!pb_decode(&stream, MasterFrame_fields, &frame)) {
        LOG_WRN("NanoPB decode failed: %s", PB_GET_ERROR(&stream));
        return;
    }

    k_mutex_lock(&data_mutex, K_FOREVER);

    switch (frame.which_payload) {
        case MasterFrame_master_measurements_tag:
            data.master_measurements = frame.payload.master_measurements;
            data.master_measurements_valid = true;
            break;
        case MasterFrame_master_status_tag:
            data.master_status = frame.payload.master_status;
            data.master_status_valid = true;
            break;
        case MasterFrame_protium_values_tag:
            data.protium_values = frame.payload.protium_values;
            data.protium_values_valid = true;
            break;
        case MasterFrame_protium_operating_state_tag:
            data.protium_operating_state =
                frame.payload.protium_operating_state.current_state;
            data.protium_operating_state_valid = true;
            break;
        default:
            LOG_WRN("Unknown master frame payload tag: %d", (int)frame.which_payload);
            break;
    }

    k_mutex_unlock(&data_mutex);
}

static void process_byte(uint8_t byte) {
    if (byte == 0x00u) {
        if (cobs_in_frame && cobs_frame_len > 0) {
            uint8_t decoded[COBS_FRAME_BUF_SIZE];
            size_t decoded_len = cobs_decode(cobs_frame_buf, cobs_frame_len,
                                             decoded, sizeof(decoded));
            if (decoded_len > 0) {
                process_master_frame(decoded, decoded_len);
            } else {
                LOG_WRN("COBS decode failed (frame_len=%u)", cobs_frame_len);
            }
        }
        cobs_frame_len = 0;
        cobs_in_frame  = true;
        return;
    }

    if (!cobs_in_frame) {
        return;
    }

    if (cobs_frame_len >= COBS_FRAME_BUF_SIZE) {
        LOG_WRN("COBS frame overflow, resetting");
        cobs_frame_len = 0;
        cobs_in_frame  = false;
        return;
    }

    cobs_frame_buf[cobs_frame_len++] = byte;
}

static void ccu_rs485_parser_thread(void *p1, void *p2, void *p3) {
    rs485_packet_t packet;

    while (1) {
        k_msgq_get(&rs485_rx_msgq, &packet, K_FOREVER);

        for (size_t i = 0; i < packet.len; i++) {
            process_byte(packet.data[i]);
        }
    }
}

void ccu_rs485_init() {
    gpio_init(&rs485.rx_led, GPIO_OUTPUT_INACTIVE);
    gpio_init(&rs485.tx_led, GPIO_OUTPUT_INACTIVE);
    rs485_init(rs485.device, &rs485.dir_pin);
    uart_callback_set_(rs485.device, rs485_callback);
    uart_rx_init(rs485.device, rs485.rx_buf, sizeof(rs485.rx_buf), 100);

    k_tid_t tid = k_thread_create(&ccu_rs485_parser_thread_data,
                                  ccu_rs485_parser_thread_stack_area,
                                  K_THREAD_STACK_SIZEOF(ccu_rs485_parser_thread_stack_area),
                                  ccu_rs485_parser_thread,
                                  NULL, NULL, NULL,
                                  CCU_RS485_PARSER_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(tid, "rs485_parser");
}
