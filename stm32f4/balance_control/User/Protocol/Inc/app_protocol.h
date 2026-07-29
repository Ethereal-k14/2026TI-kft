/**
 * @file    app_protocol.h
 * @brief   通用二进制串口协议编解码（规范 §4）
 *
 *  帧结构（小端序）：
 *  ┌────────┬─────────┬────────┬──────────────┬─────────┬───────────┬─────────────┬──────┐
 *  │ SOF[2] │ Ver[1]  │ ID[1]  │ PayLen[2]    │ Seq[2]  │ Ts[4]     │ Payload[N]  │CRC[2]│
 *  │0xA55A  │  0x01   │ 消息ID │ 0–256字节    │独立递增  │µs，允许回绕│ 定宽整数     │CCITT │
 *  └────────┴─────────┴────────┴──────────────┴─────────┴───────────┴─────────────┴──────┘
 *
 *  CRC16-CCITT-FALSE 覆盖范围：Version 至 Payload（含）
 *
 *  解析器执行：SOF 重同步、长度上限（256）、版本校验、CRC、序号跳变、超时检查
 *  错误帧只增加诊断计数，不修改控制状态
 */
#ifndef APP_PROTOCOL_H
#define APP_PROTOCOL_H

#include "bsp_common.h"
#include "bsp_uart_dma.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * 帧常量
 * ---------------------------------------------------------------------- */
#define PROTO_SOF0          0xA5U
#define PROTO_SOF1          0x5AU
#define PROTO_VERSION       0x01U
#define PROTO_HDR_LEN       12U   /* SOF(2)+Ver(1)+ID(1)+PayLen(2)+Seq(2)+Ts(4) */
#define PROTO_CRC_LEN       2U
#define PROTO_OVERHEAD      (PROTO_HDR_LEN + PROTO_CRC_LEN)
#define PROTO_MAX_PAYLOAD   256U
#define PROTO_MAX_FRAME     (PROTO_OVERHEAD + PROTO_MAX_PAYLOAD)

/* 消息超时（ms）：超过此时间未收到同 ID 帧视为超时 */
#define PROTO_MSG_TIMEOUT_MS  500U

/* -------------------------------------------------------------------------
 * 消息 ID（规范 §4.1 / §4.2）
 * ---------------------------------------------------------------------- */
typedef enum
{
    /* 下位机（USART2） */
    MSG_CHASSIS_CMD       = 0x10U, /* 主控→下位机：启停命令 */
    MSG_CHASSIS_STATUS    = 0x11U, /* 下位机→主控：状态+故障 */
    MSG_CHASSIS_IMU       = 0x12U, /* 下位机→主控：加速度 */
    MSG_CHASSIS_HEARTBEAT = 0x13U, /* 双向心跳 */

    /* 视觉（USART3） */
    MSG_VISION_POSE       = 0x20U, /* 视觉→主控：位置/速度/置信度 */
    MSG_VISION_STATUS     = 0x21U, /* 视觉→主控：帧状态 */
    MSG_VISION_TARGET     = 0x22U, /* 主控→视觉：目标/时间同步 */
} proto_msg_id_t;

/* -------------------------------------------------------------------------
 * 解码回调类型（由上层注册，协议层发现完整帧后调用）
 * ---------------------------------------------------------------------- */
typedef void (*proto_rx_callback_t)(proto_msg_id_t id,
                                     const uint8_t *payload,
                                     uint16_t       payload_len,
                                     uint32_t       timestamp_us);

/* -------------------------------------------------------------------------
 * 端口解析上下文
 * ---------------------------------------------------------------------- */
typedef struct
{
    uart_port_t      port;
    proto_rx_callback_t cb;

    /* 解析状态机 */
    uint8_t  parse_buf[PROTO_MAX_FRAME];
    uint16_t parse_idx;
    uint16_t expected_len;     /* 0=等待帧头 */

    /* 序号跟踪 */
    uint16_t last_seq;
    bool     seq_initialized;

    /* 诊断计数 */
    uint32_t rx_frame_ok;
    uint32_t rx_crc_err;
    uint32_t rx_len_err;
    uint32_t rx_ver_err;
    uint32_t rx_seq_jump;
    uint32_t rx_timeout;

    /* TX 序号（每端口独立） */
    uint16_t tx_seq;
} proto_port_ctx_t;

/* -------------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

/**
 * @brief 初始化协议端口
 * @param ctx   端口上下文（调用者提供静态存储）
 * @param port  UART 端口
 * @param cb    接收回调（NULL = 不回调）
 */
void App_Protocol_Init(proto_port_ctx_t *ctx,
                        uart_port_t       port,
                        proto_rx_callback_t cb);

/**
 * @brief 周期性轮询：从 ring buffer 读取字节，驱动解析状态机
 *        在调度器对应任务频率中调用（100 Hz 或更高）
 */
void App_Protocol_Process(proto_port_ctx_t *ctx);

/**
 * @brief 编码并发送一帧
 * @param ctx        端口上下文
 * @param msg_id     消息 ID
 * @param payload    载荷指针（可为 NULL 当 len=0）
 * @param payload_len 载荷字节数（0–256）
 * @return BSP_OK 或错误码
 */
bsp_err_t App_Protocol_Send(proto_port_ctx_t *ctx,
                              proto_msg_id_t   msg_id,
                              const uint8_t   *payload,
                              uint16_t         payload_len);

/**
 * @brief 高优先级发送（急停/故障帧，调用 BSP_UartDma_TransmitPriority）
 */
bsp_err_t App_Protocol_SendPriority(proto_port_ctx_t *ctx,
                                     proto_msg_id_t   msg_id,
                                     const uint8_t   *payload,
                                     uint16_t         payload_len);

/**
 * @brief 计算 CRC16-CCITT-FALSE
 * @param data 数据指针
 * @param len  数据长度
 * @return     CRC16 值
 */
uint16_t App_Protocol_Crc16(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif
#endif /* APP_PROTOCOL_H */
