#include "bsp_tb6612.h"
#include <string.h>

#define TB6612_PWM_MAX_CH   (4U)

static const bsp_tb6612_config_t *s_cfg = NULL;
static uint32_t s_cfg_count = 0U;
static uint32_t s_pwm_period = 0U;
static uint32_t s_command_max = 0U;
static int8_t s_prev_dir[BSP_TB6612_COUNT] = {0};
static bool s_inited = false;
static bool s_power_enabled = false;

static int tb6612_write_pair(bsp_tb6612_id_t motor, bool in1, bool in2)
{
    const bsp_tb6612_config_t *c = &s_cfg[motor];
    HAL_GPIO_WritePin(c->in1_port, c->in1_pin, in1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(c->in2_port, c->in2_pin, in2 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return 0;
}

static int tb6612_set_pwm(bsp_tb6612_id_t motor, uint32_t duty)
{
    __HAL_TIM_SET_COMPARE(s_cfg[motor].htim, s_cfg[motor].pwm_ch, duty);
    return 0;
}

static int tb6612_set_direction(bsp_tb6612_id_t motor, int8_t direction)
{
    return (direction > 0) ? tb6612_write_pair(motor, true, false)
                           : tb6612_write_pair(motor, false, true);
}

static int tb6612_set_coast(bsp_tb6612_id_t motor)
{
    tb6612_set_pwm(motor, 0);
    tb6612_write_pair(motor, false, false);
    return tb6612_set_pwm(motor, s_pwm_period);
}

static int tb6612_set_brake(bsp_tb6612_id_t motor)
{
    tb6612_set_pwm(motor, 0);
    return tb6612_write_pair(motor, true, true);
}

static int tb6612_force_safe_all(void)
{
    int status = 0;
    for (uint32_t i = 0; i < s_cfg_count; i++) {
        tb6612_set_pwm((bsp_tb6612_id_t)i, 0);
    }
    for (uint32_t i = 0; i < s_cfg_count; i++) {
        tb6612_write_pair((bsp_tb6612_id_t)i, false, false);
        s_prev_dir[i] = 0;
    }
    if (status == 0) {
        for (uint32_t i = 0; i < s_cfg_count; i++)
            tb6612_set_pwm((bsp_tb6612_id_t)i, s_pwm_period);
    }
    return status;
}

int bsp_tb6612_init(const bsp_tb6612_config_t *cfg, uint32_t count,
                     uint32_t pwm_period, uint32_t command_max)
{
    if (s_inited) return 0;
    if (!cfg || count == 0 || count > BSP_TB6612_COUNT || pwm_period == 0 || command_max == 0)
        return -1;

    s_cfg = cfg; s_cfg_count = count;
    s_pwm_period = pwm_period; s_command_max = command_max;

    for (uint32_t i = 0; i < count; i++) {
        HAL_TIM_PWM_Start(cfg[i].htim, cfg[i].pwm_ch);
    }
    tb6612_force_safe_all();
    s_power_enabled = false;
    s_inited = true;
    return 0;
}

int bsp_tb6612_power_enable(void)
{
    if (!s_inited) return -1;
    __disable_irq();
    tb6612_force_safe_all();
    s_power_enabled = true;
    __enable_irq();
    return 0;
}

void bsp_tb6612_power_disable(void)
{
    __disable_irq();
    if (s_inited) tb6612_force_safe_all();
    s_power_enabled = false;
    __enable_irq();
}

bool bsp_tb6612_power_is_enabled(void)
{
    return s_inited && s_power_enabled;
}

uint32_t bsp_tb6612_get_command_max(void)
{
    return s_inited ? s_command_max : 0;
}

int bsp_tb6612_set_speed(bsp_tb6612_id_t motor, int32_t command)
{
    if (!s_inited) return -1;
    if ((uint32_t)motor >= s_cfg_count) return -1;

    if (command > (int32_t)s_command_max) command = (int32_t)s_command_max;
    else if (command < -(int32_t)s_command_max) command = -(int32_t)s_command_max;

    int32_t adjusted = (s_cfg[motor].dir_sign < 0) ? -command : command;
    if (adjusted == 0) return bsp_tb6612_stop(motor, BSP_TB6612_MODE_BRAKE);

    int8_t direction = (adjusted > 0) ? 1 : -1;
    uint32_t magnitude = (uint32_t)((adjusted > 0) ? adjusted : -adjusted);
    uint64_t numerator = (uint64_t)magnitude * (uint64_t)s_pwm_period;
    uint32_t duty = (uint32_t)((numerator + s_command_max - 1) / s_command_max);

    __disable_irq();
    if (!s_power_enabled) { __enable_irq(); return -1; }

    if (s_prev_dir[motor] != 0 && s_prev_dir[motor] != direction) {
        tb6612_set_coast(motor);
        s_prev_dir[motor] = 0;
        __enable_irq();
        return 0;
    }

    if (s_prev_dir[motor] == 0) {
        tb6612_set_pwm(motor, 0);
        tb6612_set_direction(motor, direction);
    }
    tb6612_set_pwm(motor, duty);
    s_prev_dir[motor] = direction;
    __enable_irq();
    return 0;
}

int bsp_tb6612_stop(bsp_tb6612_id_t motor, bsp_tb6612_stop_mode_t mode)
{
    if (!s_inited) return -1;
    if ((uint32_t)motor >= s_cfg_count) return -1;

    __disable_irq();
    int ret = (mode == BSP_TB6612_MODE_BRAKE) ? tb6612_set_brake(motor) : tb6612_set_coast(motor);
    s_prev_dir[motor] = 0;
    __enable_irq();
    return ret;
}

void bsp_tb6612_stop_all(void)
{
    if (!s_inited) return;
    for (uint32_t i = 0; i < s_cfg_count; i++)
        bsp_tb6612_stop((bsp_tb6612_id_t)i, BSP_TB6612_MODE_BRAKE);
}
