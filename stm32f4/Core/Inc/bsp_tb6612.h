#ifndef BSP_TB6612_H
#define BSP_TB6612_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ======================== 类型定义 ======================== */

typedef enum {
    BSP_TB6612_A = 0,
    BSP_TB6612_B,
    BSP_TB6612_C,
    BSP_TB6612_D,
    BSP_TB6612_COUNT
} bsp_tb6612_id_t;

typedef enum {
    BSP_TB6612_MODE_COAST = 0,
    BSP_TB6612_MODE_BRAKE,
} bsp_tb6612_stop_mode_t;

typedef struct {
    TIM_HandleTypeDef *htim;     // PWM 定时器句柄
    uint32_t          pwm_ch;    // TIM_CHANNEL_1~4
    GPIO_TypeDef      *in1_port; // IN1 GPIO 端口
    uint16_t          in1_pin;   // IN1 GPIO 引脚
    GPIO_TypeDef      *in2_port; // IN2 GPIO 端口
    uint16_t          in2_pin;   // IN2 GPIO 引脚
    int8_t            dir_sign;  // 方向修正：1 或 -1
} bsp_tb6612_config_t;

/* ======================== API 函数 ======================== */

int  bsp_tb6612_init(const bsp_tb6612_config_t *cfg, uint32_t count,
                     uint32_t pwm_period, uint32_t command_max);

int  bsp_tb6612_power_enable(void);
void bsp_tb6612_power_disable(void);
bool bsp_tb6612_power_is_enabled(void);
uint32_t bsp_tb6612_get_command_max(void);

int  bsp_tb6612_set_speed(bsp_tb6612_id_t motor, int32_t command);
int  bsp_tb6612_stop(bsp_tb6612_id_t motor, bsp_tb6612_stop_mode_t mode);
void bsp_tb6612_stop_all(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_TB6612_H */
