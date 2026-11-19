#include "can_converter_rs485.h"

#include <stddef.h>
#include <pb_decode.h>
#include <zephyr/logging/log.h>

#include "can_converter.h"
#include <hydrogreen/peripherals/cobs.h>
#include <hydrogreen/peripherals/gpio.h>
#include "proto/master.pb.h"
#include <hydrogreen/peripherals/rs485.h>
#include <hydrogreen/peripherals/uart.h>

LOG_MODULE_REGISTER(ccu_rs485, LOG_LEVEL_INF);

volatile uint32_t rs485_frames_received;

#define CCU_RS485_PARSER_STACK_SIZE  2048
#define CCU_RS485_PARSER_PRIORITY    5
#define CCU_RS485_RX_QUEUE_DEPTH     16
#define CCU_RS485_RX_IDLE_TIMEOUT_US 5000
#define CCU_RS485_LED_FLASH_MS       50

typedef struct {
    const struct device *uart;
    struct gpio_dt_spec dir;
    struct gpio_dt_spec tx_led;
    struct gpio_dt_spec rx_led;
} ccu_rs485_t;

K_THREAD_STACK_DEFINE(rs485_parser_stack, CCU_RS485_PARSER_STACK_SIZE);
static struct k_thread rs485_parser_thread;

K_MSGQ_DEFINE(rs485_rx_queue, sizeof(rs485_packet_t), CCU_RS485_RX_QUEUE_DEPTH, 4);

static ccu_rs485_t rs485 = {
    .uart   = DEVICE_DT_GET(DT_ALIAS(rs485)),
    .dir    = GPIO_DT_SPEC_GET(DT_ALIAS(rs485_dir), gpios),
    .tx_led = GPIO_DT_SPEC_GET(DT_ALIAS(rs485_tx_led), gpios),
    .rx_led = GPIO_DT_SPEC_GET(DT_ALIAS(rs485_rx_led), gpios),
};

static volatile bool test_active;
static struct k_work_delayable rx_led_off_work;
static uint8_t rs485_rx_buf_a[RS485_RX_BUF_SIZE];
static uint8_t rs485_rx_buf_b[RS485_RX_BUF_SIZE];
static uint8_t *rs485_next_rx_buf = rs485_rx_buf_b;

static void rx_led_off_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (!test_active) {
        gpio_reset(&rs485.rx_led);
    }
}

static void enqueue_rx_data(const uint8_t *data, size_t len) {
    while (len > 0) {
        rs485_packet_t packet = {0};
        packet.len = MIN(len, sizeof(packet.data));
        memcpy(packet.data, data, packet.len);

        if (k_msgq_put(&rs485_rx_queue, &packet, K_NO_WAIT) != 0) {
            LOG_WRN("RS485 RX queue full, dropping %u bytes", (unsigned int)packet.len);
        }

        data += packet.len;
        len -= packet.len;
    }
}

static void rs485_uart_callback(const struct device *dev, struct uart_event *evt, void *user_data) {
    ARG_UNUSED(user_data);

    switch (evt->type) {
        case UART_RX_RDY:
            enqueue_rx_data(evt->data.rx.buf + evt->data.rx.offset, evt->data.rx.len);
            gpio_set(&rs485.rx_led);
            k_work_reschedule(&rx_led_off_work, K_MSEC(CCU_RS485_LED_FLASH_MS));
            break;
        case UART_RX_BUF_REQUEST:
            (void)uart_rx_buf_rsp(dev, rs485_next_rx_buf, RS485_RX_BUF_SIZE);
            break;
        case UART_RX_BUF_RELEASED:
            rs485_next_rx_buf = evt->data.rx_buf.buf;
            break;
        case UART_RX_DISABLED:
            rs485_next_rx_buf = rs485_rx_buf_b;
            (void)uart_rx_init(dev, rs485_rx_buf_a, sizeof(rs485_rx_buf_a),
                               CCU_RS485_RX_IDLE_TIMEOUT_US);
            break;
        case UART_TX_DONE:
            rs485_set_rx(&rs485.dir);
            if (!test_active) {
                gpio_reset(&rs485.tx_led);
            }
            break;
        case UART_TX_ABORTED:
            rs485_set_rx(&rs485.dir);
            if (!test_active) {
                gpio_reset(&rs485.tx_led);
            }
            break;
        default:
            break;
    }
}

static void on_frame(const uint8_t *decoded, size_t len, void *user_data) {
    ARG_UNUSED(user_data);

    MasterFrame frame = MasterFrame_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(decoded, len);

    if (!pb_decode(&stream, MasterFrame_fields, &frame)) {
        LOG_WRN("NanoPB decode failed: %s", PB_GET_ERROR(&stream));
        return;
    }

    k_mutex_lock(&data_mutex, K_FOREVER);

    switch (frame.which_payload) {
        case MasterFrame_master_measurements_tag:
            data.master_measurements       = frame.payload.master_measurements;
            data.master_measurements_valid = true;
            LOG_DBG("Received \"measurements\" frame at ms=%u", frame.ms_clock_tick_count);
            break;
        case MasterFrame_master_status_tag:
            data.master_status       = frame.payload.master_status;
            data.master_status_valid = true;
            LOG_DBG("Received \"status\" frame at ms=%u state=%d", frame.ms_clock_tick_count,
                    (int)frame.payload.master_status.state);
            break;
        case MasterFrame_protium_values_tag:
            data.protium_values       = frame.payload.protium_values;
            data.protium_values_valid = true;
            LOG_DBG("Received \"Protium values\" frame at ms=%u", frame.ms_clock_tick_count);
            break;
        case MasterFrame_protium_operating_state_tag:
            data.protium_operating_state =
                frame.payload.protium_operating_state.current_state;
            data.protium_operating_state_valid = true;
            LOG_DBG("Received \"Protium state\" frame at ms=%u state=%d",
                    frame.ms_clock_tick_count,
                    (int)frame.payload.protium_operating_state.current_state);
            break;
        case MasterFrame_master_faults_tag:
            data.master_faults       = frame.payload.master_faults;
            data.master_faults_valid = true;
            LOG_DBG("Received \"faults\" frame at ms=%u", frame.ms_clock_tick_count);
            break;
        default:
            LOG_WRN("Unknown master frame payload tag: %d",
                    (int)frame.which_payload);
            break;
    }

    k_mutex_unlock(&data_mutex);

    rs485_frames_received++;
}

static void parser_thread_fn(void *p1, void *p2, void *p3) {
    rs485_packet_t packet;
    cobs_parser_t  parser;

    cobs_parser_init(&parser);

    while (1) {
        if (k_msgq_get(&rs485_rx_queue, &packet, K_MSEC(500)) != 0) {
            LOG_WRN("No RS485 packet for 500 ms — RX may have stalled");
            continue;
        }

        for (size_t i = 0; i < packet.len; i++) {
            cobs_parser_feed(&parser, packet.data[i], on_frame, NULL);
        }
    }
}

void ccu_rs485_test_set(bool active) {
    test_active = active;
    if (active) {
        gpio_set(&rs485.rx_led);
        gpio_set(&rs485.tx_led);
    } else {
        gpio_reset(&rs485.rx_led);
        gpio_reset(&rs485.tx_led);
    }
}

void ccu_rs485_init(void) {
    /* CCU owns the RX callback to keep its queue and LED timing. */
    if (uart_device_init(rs485.uart) != 0) {
        return;
    }
    if (gpio_init(&rs485.dir, GPIO_OUTPUT_INACTIVE) != 0) {
        return;
    }
    rs485_set_rx(&rs485.dir);
    gpio_init(&rs485.rx_led, GPIO_OUTPUT_INACTIVE);
    gpio_init(&rs485.tx_led, GPIO_OUTPUT_INACTIVE);
    k_work_init_delayable(&rx_led_off_work, rx_led_off_handler);
    uart_callback_set_(rs485.uart, rs485_uart_callback);
    uart_rx_init(rs485.uart, rs485_rx_buf_a, sizeof(rs485_rx_buf_a),
                 CCU_RS485_RX_IDLE_TIMEOUT_US);

    k_tid_t tid = k_thread_create(&rs485_parser_thread,
                                  rs485_parser_stack,
                                  K_THREAD_STACK_SIZEOF(rs485_parser_stack),
                                  parser_thread_fn,
                                  NULL, NULL, NULL,
                                  CCU_RS485_PARSER_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(tid, "rs485_parser");
}
