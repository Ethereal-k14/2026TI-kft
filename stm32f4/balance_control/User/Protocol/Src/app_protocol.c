/**
 * @file    app_protocol.c
 * @brief   通用二进制串口协议编解码实现
 *
 *  CRC16-CCITT-FALSE：poly=0x1021, init=0xFFFF, refin=false, refout=false
 *  解析器为字节流状态机，支持 SOF 重同步，不停止 DMA。
 */
#include "app_protocol.h"
#include "main.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * CRC16-CCITT-FALSE 查表（预计算，加速实时计算）
 * ---------------------------------------------------------------------- */
static const uint16_t k_crc16_table[256U] =
{
    0x0000U,0x1021U,0x2042U,0x3063U,0x4084U,0x50A5U,0x60C6U,0x70E7U,
    0x8108U,0x9129U,0xA14AU,0xB16BU,0xC18CU,0xD1ADU,0xE1CEU,0xF1EFU,
    0x1231U,0x0210U,0x3273U,0x2252U,0x52B5U,0x4294U,0x72F7U,0x62D6U,
    0x9339U,0x8318U,0xB37BU,0xA35AU,0xD3BDU,0xC39CU,0xF3FFU,0xE3DEU,
    0x2462U,0x3443U,0x0420U,0x1401U,0x64E6U,0x74C7U,0x44A4U,0x5485U,
    0xA56AU,0xB54BU,0x8528U,0x9509U,0xE5EEU,0xF5CFU,0xC5ACU,0xD58DU,
    0x3653U,0x2672U,0x1611U,0x0630U,0x76D7U,0x66F6U,0x5695U,0x46B4U,
    0xB75BU,0xA77AU,0x9719U,0x8738U,0xF7DFU,0xE7FEU,0xD79DU,0xC7BCU,
    0x4884U,0x58A5U,0x68C6U,0x78E7U,0x0840U,0x1861U,0x2802U,0x3823U,
    0xC9CCU,0xD9EDU,0xE98EU,0xF9AFU,0x8948U,0x9969U,0xA90AU,0xB92BU,
    0x5AF5U,0x4AD4U,0x7AB7U,0x6A96U,0x1A71U,0x0A50U,0x3A33U,0x2A12U,
    0xDBFDU,0xCBDCU,0xFBBFU,0xEB9EU,0x9B79U,0x8B58U,0xBB3BU,0xAB1AU,
    0x6CA6U,0x7C87U,0x4CE4U,0x5CC5U,0x2C22U,0x3C03U,0x0C60U,0x1C41U,
    0xEDAEU,0xFD8FU,0xCDECU,0xDDCDU,0xAD2AU,0xBD0BU,0x8D68U,0x9D49U,
    0x7E97U,0x6EB6U,0x5ED5U,0x4EF4U,0x3E13U,0x2E32U,0x1E51U,0x0E70U,
    0xFF9FU,0xEFBEU,0xDFDDU,0xCFFCU,0xBF1BU,0xAF3AU,0x9F59U,0x8F78U,
    0x9188U,0x81A9U,0xB1CAU,0xA1EBU,0xD10CU,0xC12DU,0xF14EU,0xE16FU,
    0x1080U,0x00A1U,0x30C2U,0x20E3U,0x5004U,0x4025U,0x7046U,0x6067U,
    0x83B9U,0x9398U,0xA3FBU,0xB3DAU,0xC33DU,0xD31CU,0xE37FU,0xF35EU,
    0x02B1U,0x1290U,0x22F3U,0x32D2U,0x4235U,0x5214U,0x6277U,0x7256U,
    0xB5EAU,0xA5CBU,0x95A8U,0x8589U,0xF56EU,0xE54FU,0xD52CU,0xC50DU,
    0x34E2U,0x24C3U,0x14A0U,0x0481U,0x7466U,0x6447U,0x5424U,0x4405U,
    0xA7DBU,0xB7FAU,0x8799U,0x97B8U,0xE75FU,0xF77EU,0xC71DU,0xD73CU,
    0x26D3U,0x36F2U,0x0691U,0x16B0U,0x6657U,0x7676U,0x4615U,0x5634U,
    0xD94CU,0xC96DU,0xF90EU,0xE92FU,0x99C8U,0x89E9U,0xB98AU,0xA9ABU,
    0x5844U,0x4865U,0x7806U,0x6827U,0x18C0U,0x08E1U,0x3882U,0x28A3U,
    0xCB7DU,0xDB5CU,0xEB3FU,0xFB1EU,0x8BF9U,0x9BD8U,0xABBBU,0xBB9AU,
    0x4A75U,0x5A54U,0x6A37U,0x7A16U,0x0AF1U,0x1AD0U,0x2AB3U,0x3A92U,
    0xFD2EU,0xED0FU,0xDD6CU,0xCD4DU,0xBDAAU,0xAD8BU,0x9DE8U,0x8DC9U,
    0x7C26U,0x6C07U,0x5C64U,0x4C45U,0x3CA2U,0x2C83U,0x1CE0U,0x0CC1U,
    0xEF1FU,0xFF3EU,0xCF5DU,0xDF7CU,0xAF9BU,0xBFBAU,0x8FD9U,0x9FF8U,
    0x6E17U,0x7E36U,0x4E55U,0x5E74U,0x2E93U,0x3EB2U,0x0ED1U,0x1EF0U,
};

/* -------------------------------------------------------------------------
 * 公开：CRC16-CCITT-FALSE
 * ---------------------------------------------------------------------- */
uint16_t App_Protocol_Crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    for (i = 0U; i < len; i++)
    {
        crc = (uint16_t)((crc << 8U) ^
              k_crc16_table[((crc >> 8U) ^ (uint16_t)data[i]) & 0xFFU]);
    }
    return crc;
}

/* -------------------------------------------------------------------------
 * 私有：重置解析状态机
 * ---------------------------------------------------------------------- */
static void parse_reset(proto_port_ctx_t *ctx)
{
    ctx->parse_idx    = 0U;
    ctx->expected_len = 0U;
}

/* -------------------------------------------------------------------------
 * 私有：处理一个完整帧（parse_buf 中已有完整内容）
 * ---------------------------------------------------------------------- */
static void parse_dispatch(proto_port_ctx_t *ctx)
{
    const uint8_t *buf = ctx->parse_buf;
    uint16_t frame_len = ctx->expected_len;

    /* --- 版本校验 --- */
    if (buf[2U] != PROTO_VERSION)
    {
        ctx->rx_ver_err++;
        return;
    }

    /* --- 载荷长度 --- */
    uint16_t pay_len = (uint16_t)(buf[4U] | ((uint16_t)buf[5U] << 8U));
    if (pay_len > PROTO_MAX_PAYLOAD)
    {
        ctx->rx_len_err++;
        return;
    }

    /* 校验 frame_len 是否一致 */
    if (frame_len != PROTO_OVERHEAD + pay_len)
    {
        ctx->rx_len_err++;
        return;
    }

    /* --- CRC 校验（覆盖 Ver 到 Payload，跳过 SOF） */
    uint16_t crc_calc = App_Protocol_Crc16(&buf[2U],
                                            (uint16_t)(PROTO_HDR_LEN - 2U + pay_len));
    uint16_t crc_rx   = (uint16_t)(buf[frame_len - 2U] |
                         ((uint16_t)buf[frame_len - 1U] << 8U));
    if (crc_calc != crc_rx)
    {
        ctx->rx_crc_err++;
        return;
    }

    /* --- 序号跳变检测 --- */
    uint16_t seq = (uint16_t)(buf[6U] | ((uint16_t)buf[7U] << 8U));
    if (ctx->seq_initialized)
    {
        uint16_t expected = (uint16_t)(ctx->last_seq + 1U);
        if (seq != expected)
        {
            ctx->rx_seq_jump++;
            /* 仅记录，不丢弃该帧 */
        }
    }
    ctx->last_seq        = seq;
    ctx->seq_initialized = true;

    /* --- 提取时间戳 --- */
    uint32_t ts_us =
        (uint32_t)buf[8U]              |
        ((uint32_t)buf[9U]  <<  8U)    |
        ((uint32_t)buf[10U] << 16U)    |
        ((uint32_t)buf[11U] << 24U);

    /* --- 分发回调 --- */
    proto_msg_id_t msg_id = (proto_msg_id_t)buf[3U];
    ctx->rx_frame_ok++;

    if (ctx->cb != NULL)
    {
        ctx->cb(msg_id, &buf[PROTO_HDR_LEN], pay_len, ts_us);
    }
}

/* -------------------------------------------------------------------------
 * 公开接口实现
 * ---------------------------------------------------------------------- */

void App_Protocol_Init(proto_port_ctx_t   *ctx,
                        uart_port_t         port,
                        proto_rx_callback_t cb)
{
    if (ctx == NULL)
    {
        return;
    }
    (void)memset(ctx, 0, sizeof(*ctx));
    ctx->port = port;
    ctx->cb   = cb;
}

void App_Protocol_Process(proto_port_ctx_t *ctx)
{
    if (ctx == NULL)
    {
        return;
    }
    uint8_t  byte_buf[64U];
    uint16_t n = BSP_UartDma_Read(ctx->port, byte_buf, (uint16_t)sizeof(byte_buf));
    uint16_t i;

    for (i = 0U; i < n; i++)
    {
        uint8_t b = byte_buf[i];

        if (ctx->expected_len == 0U)
        {
            /* 等待 SOF */
            if (ctx->parse_idx == 0U)
            {
                if (b == PROTO_SOF0)
                {
                    ctx->parse_buf[ctx->parse_idx++] = b;
                }
            }
            else if (ctx->parse_idx == 1U)
            {
                if (b == PROTO_SOF1)
                {
                    ctx->parse_buf[ctx->parse_idx++] = b;
                }
                else
                {
                    /* SOF 不匹配，重新搜索 */
                    parse_reset(ctx);
                    if (b == PROTO_SOF0)
                    {
                        ctx->parse_buf[ctx->parse_idx++] = b;
                    }
                }
            }
            else
            {
                /* 正在接收固定头部 */
                ctx->parse_buf[ctx->parse_idx++] = b;

                /* 收齐固定头部后（含时间戳）确定完整帧长。 */
                if (ctx->parse_idx == PROTO_HDR_LEN)
                {
                    /* parse_buf[4..5] = PayLen（小端） */
                    uint16_t pay = (uint16_t)(ctx->parse_buf[4U] |
                                   ((uint16_t)ctx->parse_buf[5U] << 8U));
                    if (pay > PROTO_MAX_PAYLOAD)
                    {
                        ctx->rx_len_err++;
                        parse_reset(ctx);
                    }
                    else
                    {
                        ctx->expected_len = PROTO_OVERHEAD + pay;
                    }
                }
            }
        }
        else
        {
            /* 继续接收到完整帧 */
            if (ctx->parse_idx < PROTO_MAX_FRAME)
            {
                ctx->parse_buf[ctx->parse_idx++] = b;
            }

            if (ctx->parse_idx >= ctx->expected_len)
            {
                /* 帧接收完整，处理 */
                parse_dispatch(ctx);
                parse_reset(ctx);
            }
        }
    }
}

bsp_err_t App_Protocol_Send(proto_port_ctx_t *ctx,
                              proto_msg_id_t   msg_id,
                              const uint8_t   *payload,
                              uint16_t         payload_len)
{
    if ((ctx == NULL) || ((payload == NULL) && (payload_len != 0U)))
    {
        return BSP_ERR_INVALID;
    }
    if (payload_len > PROTO_MAX_PAYLOAD)
    {
        return BSP_ERR_RANGE;
    }

    uint8_t  frame[PROTO_MAX_FRAME];
    uint16_t idx = 0U;

    /* SOF */
    frame[idx++] = PROTO_SOF0;
    frame[idx++] = PROTO_SOF1;
    /* Version */
    frame[idx++] = PROTO_VERSION;
    /* Message ID */
    frame[idx++] = (uint8_t)msg_id;
    /* Payload Length（小端） */
    frame[idx++] = (uint8_t)(payload_len & 0xFFU);
    frame[idx++] = (uint8_t)((payload_len >> 8U) & 0xFFU);
    /* Sequence（小端） */
    ctx->tx_seq++;
    frame[idx++] = (uint8_t)(ctx->tx_seq & 0xFFU);
    frame[idx++] = (uint8_t)((ctx->tx_seq >> 8U) & 0xFFU);
    /* Timestamp（小端） */
    uint32_t ts = BSP_GetTimestampUs();
    frame[idx++] = (uint8_t)(ts & 0xFFU);
    frame[idx++] = (uint8_t)((ts >> 8U)  & 0xFFU);
    frame[idx++] = (uint8_t)((ts >> 16U) & 0xFFU);
    frame[idx++] = (uint8_t)((ts >> 24U) & 0xFFU);
    /* Payload */
    if (payload != NULL && payload_len > 0U)
    {
        (void)memcpy(&frame[idx], payload, payload_len);
        idx = (uint16_t)(idx + payload_len);
    }
    /* CRC（覆盖 Ver 至 Payload） */
    uint16_t crc = App_Protocol_Crc16(&frame[2U], (uint16_t)(idx - 2U));
    frame[idx++] = (uint8_t)(crc & 0xFFU);
    frame[idx++] = (uint8_t)((crc >> 8U) & 0xFFU);

    return BSP_UartDma_Transmit(ctx->port, frame, idx);
}

bsp_err_t App_Protocol_SendPriority(proto_port_ctx_t *ctx,
                                     proto_msg_id_t   msg_id,
                                     const uint8_t   *payload,
                                     uint16_t         payload_len)
{
    if ((ctx == NULL) || ((payload == NULL) && (payload_len != 0U)))
    {
        return BSP_ERR_INVALID;
    }
    if (payload_len > PROTO_MAX_PAYLOAD)
    {
        return BSP_ERR_RANGE;
    }

    uint8_t  frame[PROTO_MAX_FRAME];
    uint16_t idx = 0U;

    frame[idx++] = PROTO_SOF0;
    frame[idx++] = PROTO_SOF1;
    frame[idx++] = PROTO_VERSION;
    frame[idx++] = (uint8_t)msg_id;
    frame[idx++] = (uint8_t)(payload_len & 0xFFU);
    frame[idx++] = (uint8_t)((payload_len >> 8U) & 0xFFU);
    ctx->tx_seq++;
    frame[idx++] = (uint8_t)(ctx->tx_seq & 0xFFU);
    frame[idx++] = (uint8_t)((ctx->tx_seq >> 8U) & 0xFFU);
    uint32_t ts  = BSP_GetTimestampUs();
    frame[idx++] = (uint8_t)(ts & 0xFFU);
    frame[idx++] = (uint8_t)((ts >> 8U)  & 0xFFU);
    frame[idx++] = (uint8_t)((ts >> 16U) & 0xFFU);
    frame[idx++] = (uint8_t)((ts >> 24U) & 0xFFU);
    if (payload != NULL && payload_len > 0U)
    {
        (void)memcpy(&frame[idx], payload, payload_len);
        idx = (uint16_t)(idx + payload_len);
    }
    uint16_t crc = App_Protocol_Crc16(&frame[2U], (uint16_t)(idx - 2U));
    frame[idx++] = (uint8_t)(crc & 0xFFU);
    frame[idx++] = (uint8_t)((crc >> 8U) & 0xFFU);

    return BSP_UartDma_TransmitPriority(ctx->port, frame, idx);
}
