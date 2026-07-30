#ifndef APP_PID_H
#define APP_PID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    APP_PID_MODE_POSITION = 0,
    APP_PID_MODE_INCREMENT,
} app_pid_mode_t;

typedef struct {
    float kp;
    float ki;
    float kd;
    app_pid_mode_t mode;
    float ff_kp;
    float ff_ki;
    float ff_kd;
    bool  use_ff;
    float out_min;
    float out_max;
    float integral_min;
    float integral_max;
    float ff_integral_min;
    float ff_integral_max;
    float d_filter_coeff;
    float setpoint;
    float last_error;
    float last_last_error;
    float integral;
    float last_derivative;
    bool  is_first_run;
} app_pid_t;

void app_pid_init(app_pid_t *pid, float kp, float ki, float kd,
                   app_pid_mode_t mode, float out_min, float out_max);
void app_pid_set_params(app_pid_t *pid, float kp, float ki, float kd);
void app_pid_set_integral_limit(app_pid_t *pid, float integral_min, float integral_max);
void app_pid_set_d_filter(app_pid_t *pid, float coeff);
void app_pid_set_setpoint(app_pid_t *pid, float setpoint);
float app_pid_compute(app_pid_t *pid, float feedback, float dt_s);
void app_pid_reset(app_pid_t *pid);
float app_pid_get_error(const app_pid_t *pid);
float app_pid_get_integral(const app_pid_t *pid);

#ifdef __cplusplus
}
#endif

#endif /* APP_PID_H */
