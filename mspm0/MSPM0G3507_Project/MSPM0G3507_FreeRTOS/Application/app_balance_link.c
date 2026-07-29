/**
 * @file app_balance_link.c
 * @brief A5/5A framed link shared with stm32f4/balance_control.
 */
#include "app_balance_link.h"
#include "app_main.h"
#include "app_protocol_user.h"
#include "app_line_track.h"
#include "proto_uart1_a.h"
#include "osal_api.h"
#include "project_config.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define LINK_SOF0 (0xA5U)
#define LINK_SOF1 (0x5AU)
#define LINK_VERSION (0x01U)
#define LINK_HEADER_LEN (12U)
#define LINK_OVERHEAD (14U)
#define LINK_MAX_PAYLOAD (32U)
#define LINK_MAX_FRAME (LINK_OVERHEAD + LINK_MAX_PAYLOAD)
#define LINK_MSG_CMD (0x10U)
#define LINK_MSG_STATUS (0x11U)
#define LINK_MSG_IMU (0x12U)
#define LINK_MSG_HEARTBEAT (0x13U)

typedef struct {
    uint8_t rx[LINK_MAX_FRAME];
    uint16_t rx_len;
    uint16_t expected_len;
    uint16_t tx_seq;
    uint16_t last_cmd_seq;
    uint32_t last_rx_ms;
    bool remote_session;
    bool watchdog_timeout;
} balance_link_ctx_t;

static balance_link_ctx_t s_link;

static uint16_t crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    for (uint16_t i = 0U; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8U;
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            crc = (crc & 0x8000U) != 0U ?
                (uint16_t)((crc << 1U) ^ 0x1021U) : (uint16_t)(crc << 1U);
        }
    }
    return crc;
}

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8U);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8U);
    p[2] = (uint8_t)(v >> 16U);
    p[3] = (uint8_t)(v >> 24U);
}

static bool send_frame(uint8_t id, const uint8_t *payload, uint16_t len)
{
    uint8_t frame[LINK_MAX_FRAME];
    uint16_t crc;
    if ((len > LINK_MAX_PAYLOAD) || ((payload == NULL) && (len != 0U))) {
        return false;
    }
    frame[0] = LINK_SOF0;
    frame[1] = LINK_SOF1;
    frame[2] = LINK_VERSION;
    frame[3] = id;
    put_u16(&frame[4], len);
    put_u16(&frame[6], s_link.tx_seq++);
    put_u32(&frame[8], osal_ticks_to_ms(osal_get_tick_count()) * 1000U);
    if (len != 0U) { (void)memcpy(&frame[12], payload, len); }
    crc = crc16(&frame[2], (uint16_t)(10U + len));
    put_u16(&frame[12U + len], crc);
    return proto_uart1_a_write(frame, (uint16_t)(LINK_OVERHEAD + len)) == BSP_OK;
}

static void stop_chassis(bool emergency)
{
    app_protocol_user_line_track_force_stop();
    if (emergency) {
        /* ESTOP may preempt the control task; normal STOP is consumed there. */
        app_motor_stop_all(app_protocol_get_context());
    }
}

static void handle_frame(const uint8_t *frame, uint16_t len)
{
    const uint16_t payload_len = (uint16_t)frame[4] |
                                 ((uint16_t)frame[5] << 8U);
    const uint16_t received_crc = (uint16_t)frame[len - 2U] |
                                  ((uint16_t)frame[len - 1U] << 8U);
    if ((frame[2] != LINK_VERSION) || (payload_len > LINK_MAX_PAYLOAD) ||
        (len != (uint16_t)(LINK_OVERHEAD + payload_len)) ||
        (received_crc != crc16(&frame[2], (uint16_t)(10U + payload_len)))) {
        return;
    }
    s_link.last_rx_ms = osal_ticks_to_ms(osal_get_tick_count());
    if ((frame[3] == LINK_MSG_CMD) && (payload_len >= 2U)) {
        s_link.last_cmd_seq = (uint16_t)frame[6] | ((uint16_t)frame[7] << 8U);
        switch (frame[12]) {
        case 0U:
            s_link.remote_session = false;
            s_link.watchdog_timeout = false;
            stop_chassis(false);
            break;
        case 1U:
            s_link.watchdog_timeout = false;
            s_link.remote_session = true;
            app_protocol_user_line_track_request_start();
            break;
        case 2U:
            s_link.remote_session = false;
            s_link.watchdog_timeout = false;
            stop_chassis(true);
            break;
        default: break;
        }
    }
}

static void feed_byte(uint8_t byte)
{
    if (s_link.rx_len == 0U) {
        if (byte == LINK_SOF0) { s_link.rx[s_link.rx_len++] = byte; }
        return;
    }
    if ((s_link.rx_len == 1U) && (byte != LINK_SOF1)) {
        s_link.rx_len = (byte == LINK_SOF0) ? 1U : 0U;
        return;
    }
    s_link.rx[s_link.rx_len++] = byte;
    if (s_link.rx_len == LINK_HEADER_LEN) {
        const uint16_t payload_len = (uint16_t)s_link.rx[4] |
                                     ((uint16_t)s_link.rx[5] << 8U);
        if (payload_len > LINK_MAX_PAYLOAD) { s_link.rx_len = 0U; return; }
        s_link.expected_len = (uint16_t)(LINK_OVERHEAD + payload_len);
    }
    if ((s_link.expected_len != 0U) && (s_link.rx_len == s_link.expected_len)) {
        handle_frame(s_link.rx, s_link.rx_len);
        s_link.rx_len = 0U;
        s_link.expected_len = 0U;
    } else if (s_link.rx_len >= LINK_MAX_FRAME) {
        s_link.rx_len = 0U;
        s_link.expected_len = 0U;
    }
}

static void send_status(void)
{
    uint8_t p[5];
    app_runtime_diag_t diag;
    const bool running = app_line_track_is_running();
    (void)app_runtime_diag_read(&diag);
    p[0] = (diag.fault_code != APP_RUNTIME_FAULT_NONE ||
            s_link.watchdog_timeout) ? 2U : (running ? 1U : 0U);
    put_u16(&p[1], (uint16_t)diag.fault_code |
                        (s_link.watchdog_timeout ? 0x8000U : 0U));
    put_u16(&p[3], s_link.last_cmd_seq);
    (void)send_frame(LINK_MSG_STATUS, p, sizeof(p));
}

static void send_imu(void)
{
    uint8_t p[13];
    app_imu_data_t imu;
    const uint32_t now = osal_ticks_to_ms(osal_get_tick_count());
    app_shared_ctx_t *ctx = app_protocol_get_context();
    OSAL_CRITICAL_SECTION { imu = ctx->imu; }
    put_u32(&p[0], (uint32_t)(int32_t)(imu.accel_x_g * 9806.65f));
    put_u32(&p[4], (uint32_t)(int32_t)(imu.accel_y_g * 9806.65f));
    put_u32(&p[8], (uint32_t)(int32_t)(imu.accel_z_g * 9806.65f));
    p[12] = ((now - imu.timestamp_ms) <= 50U) ? 255U : 0U;
    (void)send_frame(LINK_MSG_IMU, p, sizeof(p));
}

static void balance_link_task(void *arg)
{
    uint32_t last_imu = 0U;
    uint32_t last_status = 0U;
    uint8_t byte;
    (void)arg;
    for (;;) {
        const uint32_t now = osal_ticks_to_ms(osal_get_tick_count());
        while (proto_uart1_a_getc(&byte) == BSP_OK) { feed_byte(byte); }
        if (s_link.remote_session && (s_link.last_rx_ms != 0U) &&
            ((now - s_link.last_rx_ms) > PRJ_BALANCE_LINK_WATCHDOG_MS)) {
            s_link.remote_session = false;
            s_link.watchdog_timeout = true;
            stop_chassis(false);
        }
        if ((now - last_imu) >= 20U) { last_imu = now; send_imu(); }
        if ((now - last_status) >= 100U) {
            last_status = now;
            send_status();
            (void)send_frame(LINK_MSG_HEARTBEAT, NULL, 0U);
        }
        osal_task_delay_ms(5U);
    }
}

int32_t app_balance_link_init(void)
{
    (void)memset(&s_link, 0, sizeof(s_link));
    if (proto_uart1_a_init() != BSP_OK) { return -1; }
    return osal_task_create(balance_link_task, "balance_link", 384U, NULL, 3U)
        == NULL ? -2 : 0;
}
