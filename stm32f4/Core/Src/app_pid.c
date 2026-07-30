#include "app_pid.h"
#include <math.h>

static inline float clamp_f(float val, float min_val, float max_val)
{
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

void app_pid_init(app_pid_t *pid, float kp, float ki, float kd,
                   app_pid_mode_t mode, float out_min, float out_max)
{
    pid->kp = kp; pid->ki = ki; pid->kd = kd; pid->mode = mode;
    pid->ff_kp = kp; pid->ff_ki = ki; pid->ff_kd = 0.0f; pid->use_ff = false;
    pid->out_min = out_min; pid->out_max = out_max;
    pid->integral_min = out_min; pid->integral_max = out_max;
    pid->ff_integral_min = out_min; pid->ff_integral_max = out_max;
    pid->d_filter_coeff = 0.0f;
    app_pid_reset(pid);
}

void app_pid_set_params(app_pid_t *pid, float kp, float ki, float kd)
{ pid->kp = kp; pid->ki = ki; pid->kd = kd; }

void app_pid_set_integral_limit(app_pid_t *pid, float integral_min, float integral_max)
{ pid->integral_min = integral_min; pid->integral_max = integral_max; }

void app_pid_set_d_filter(app_pid_t *pid, float coeff)
{ pid->d_filter_coeff = clamp_f(coeff, 0.0f, 1.0f); }

void app_pid_set_setpoint(app_pid_t *pid, float setpoint)
{ pid->setpoint = setpoint; }

float app_pid_compute(app_pid_t *pid, float feedback, float dt_s)
{
    float error = pid->setpoint - feedback;
    float output = 0.0f;
    float kp, ki, kd, int_min, int_max;
    if (pid->use_ff) {
        kp = pid->ff_kp; ki = pid->ff_ki; kd = pid->ff_kd;
        int_min = pid->ff_integral_min; int_max = pid->ff_integral_max;
    } else {
        kp = pid->kp; ki = pid->ki; kd = pid->kd;
        int_min = pid->integral_min; int_max = pid->integral_max;
    }

    if (pid->use_ff) {
        if (!pid->is_first_run && dt_s > 0.0f)
            pid->integral += (error + pid->last_error) * 0.5f * dt_s;
        pid->integral = clamp_f(pid->integral, int_min, int_max);
        float derivative = 0.0f;
        if (!pid->is_first_run && dt_s > 0.0f) {
            derivative = (error - pid->last_error) / dt_s;
            if (pid->d_filter_coeff > 0.0f)
                derivative = pid->last_derivative + (1.0f - pid->d_filter_coeff) * (derivative - pid->last_derivative);
        }
        pid->last_derivative = derivative;
        output = kp * error + ki * pid->integral + kd * derivative;
    } else if (pid->mode == APP_PID_MODE_POSITION) {
        if (!pid->is_first_run && dt_s > 0.0f)
            pid->integral += (error + pid->last_error) * 0.5f * dt_s;
        pid->integral = clamp_f(pid->integral, int_min, int_max);
        float derivative = 0.0f;
        if (!pid->is_first_run && dt_s > 0.0f) {
            derivative = (error - pid->last_error) / dt_s;
            if (pid->d_filter_coeff > 0.0f)
                derivative = pid->last_derivative + (1.0f - pid->d_filter_coeff) * (derivative - pid->last_derivative);
        }
        pid->last_derivative = derivative;
        output = kp * error + ki * pid->integral + kd * derivative;
    } else {
        if (pid->is_first_run) {
            output = kp * error;
            if (dt_s > 0.0f) output += ki * error * dt_s;
        } else {
            float delta_error = error - pid->last_error;
            float delta2_error = 0.0f;
            if (dt_s > 0.0f) delta2_error = error - 2.0f * pid->last_error + pid->last_last_error;
            float delta_out = kp * delta_error;
            if (dt_s > 0.0f) delta_out += ki * error * dt_s + kd * delta2_error / dt_s;
            pid->integral = clamp_f(pid->integral + delta_out, int_min, int_max);
            output = clamp_f(pid->integral, pid->out_min, pid->out_max);
        }
        pid->integral = output;
        pid->last_last_error = pid->last_error;
    }

    pid->last_error = error;
    pid->is_first_run = false;
    output = clamp_f(output, pid->out_min, pid->out_max);

    if (!isfinite(output) || !isfinite(pid->integral) || !isfinite(pid->last_error)) {
        app_pid_reset(pid);
        output = 0.0f;
    }
    return output;
}

void app_pid_reset(app_pid_t *pid)
{
    pid->last_error = 0.0f; pid->last_last_error = 0.0f;
    pid->integral = 0.0f; pid->last_derivative = 0.0f;
    pid->is_first_run = true;
}

float app_pid_get_error(const app_pid_t *pid) { return pid->last_error; }
float app_pid_get_integral(const app_pid_t *pid) { return pid->integral; }
