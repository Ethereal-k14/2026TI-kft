/**
 * @file    bsp_key.c
 * @brief   按键与限位开关 BSP 实现
 *
 *  在 BSP_Key_Init() 中重新配置 EXTI 边沿：
 *  - PE0（START_KEY）：下降沿（低有效）
 *  - PE5（LIMIT_MIN）：双边沿（检测触发与恢复）
 *  - PE6（LIMIT_MAX）：双边沿
 *
 *  去抖：START_KEY 中断只记录时间戳，BSP_Key_Process() 每 1 ms 检查
 *  连续低电平 ≥ 20 ms 后产生一次事件（不把按键连续刷到串口）。
 */
#include "bsp_key.h"
#include "main.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * 私有状态
 * ---------------------------------------------------------------------- */
typedef struct
{
    /* START_KEY 去抖 */
    volatile bool    key_irq_pending;     /* 中断触发了一次下降沿 */
    uint32_t         key_press_start_ms;  /* 下降沿时刻 */
    bool             key_debouncing;      /* 正在计数去抖时间 */

    key_limit_state_t state;
} key_ctx_t;

static key_ctx_t s_ctx;

/* -------------------------------------------------------------------------
 * 私有：重新配置 EXTI 边沿（覆盖 CubeMX 设置）
 * ---------------------------------------------------------------------- */
static void key_reconfig_exti(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* PE0 START_KEY：下降沿（低有效） */
    gpio.Pin  = START_KEY_Pin;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(START_KEY_GPIO_Port, &gpio);

    /* PE5 LIMIT_MIN：双边沿 */
    gpio.Pin  = LIMIT_MIN_Pin;
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(LIMIT_MIN_GPIO_Port, &gpio);

    /* PE6 LIMIT_MAX：双边沿 */
    gpio.Pin  = LIMIT_MAX_Pin;
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(LIMIT_MAX_GPIO_Port, &gpio);

    /* 使能 NVIC（CubeMX 可能未使能 EXTI0/5/6，统一在此确认） */
    HAL_NVIC_SetPriority(EXTI0_IRQn,   3U, 0U);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);

    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 3U, 0U);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

/* -------------------------------------------------------------------------
 * 公开接口实现
 * ---------------------------------------------------------------------- */

void BSP_Key_Init(void)
{
    (void)memset(&s_ctx, 0, sizeof(s_ctx));
    key_reconfig_exti();

    /* 读取当前引脚电平作为初始状态 */
    s_ctx.state.limit_min_active =
        (HAL_GPIO_ReadPin(LIMIT_MIN_GPIO_Port, LIMIT_MIN_Pin) == GPIO_PIN_SET);
    s_ctx.state.limit_max_active =
        (HAL_GPIO_ReadPin(LIMIT_MAX_GPIO_Port, LIMIT_MAX_Pin) == GPIO_PIN_SET);
}

void BSP_Key_Process(void)
{
    /* START_KEY 去抖处理（每 1 ms 调用） */
    if (s_ctx.key_irq_pending)
    {
        if (!s_ctx.key_debouncing)
        {
            /* 开始计时 */
            s_ctx.key_press_start_ms = HAL_GetTick();
            s_ctx.key_debouncing     = true;
        }
        s_ctx.key_irq_pending = false;
    }

    if (s_ctx.key_debouncing)
    {
        /* 检查按键是否仍然按下 */
        bool still_pressed =
            (HAL_GPIO_ReadPin(START_KEY_GPIO_Port, START_KEY_Pin) == GPIO_PIN_RESET);
        uint32_t elapsed = HAL_GetTick() - s_ctx.key_press_start_ms;

        if (!still_pressed)
        {
            /* 松开了，取消去抖 */
            s_ctx.key_debouncing = false;
        }
        else if (elapsed >= 20U)
        {
            /* 连续按下 20 ms，产生一次事件 */
            s_ctx.state.start_key_pressed = true;
            s_ctx.key_debouncing          = false;
        }
        else
        {
            /* 继续等待 */
        }
    }
}

void BSP_Key_StartKeyIsr(void)
{
    /* 中断上下文：只记录标志，不执行去抖 */
    s_ctx.key_irq_pending = true;
}

void BSP_Key_LimitMinIsr(void)
{
    /* 双边沿：读取稳定电平 */
    bool active = (HAL_GPIO_ReadPin(LIMIT_MIN_GPIO_Port, LIMIT_MIN_Pin) == GPIO_PIN_SET);
    s_ctx.state.limit_min_active = active;
    if (active)
    {
        s_ctx.state.limit_min_triggered = true;
    }
}

void BSP_Key_LimitMaxIsr(void)
{
    bool active = (HAL_GPIO_ReadPin(LIMIT_MAX_GPIO_Port, LIMIT_MAX_Pin) == GPIO_PIN_SET);
    s_ctx.state.limit_max_active = active;
    if (active)
    {
        s_ctx.state.limit_max_triggered = true;
    }
}

void BSP_Key_GetState(key_limit_state_t *out)
{
    if (out != NULL)
    {
        *out = s_ctx.state;
    }
}

bool BSP_Key_ConsumeStartEvent(void)
{
    bool evt = s_ctx.state.start_key_pressed;
    s_ctx.state.start_key_pressed = false;
    return evt;
}
