#include "pid_controller.h"
#include "esp_timer.h"
#include <math.h>

void pid_controller_init(pid_controller_t *pid, const pid_config_t *config) {
    pid->config = *config;
    pid->integral = 0.0f;
    pid->last_error = 0.0f;
    pid->last_input = 0.0f;
    pid->last_time = 0;
    pid->first_run = true;
}

float pid_controller_compute(pid_controller_t *pid, float setpoint, float input) {
    uint32_t now = esp_timer_get_time() / 1000;  // Convert to ms
    
    // Skip if not enough time has passed
    if (!pid->first_run && (now - pid->last_time) < pid->config.sample_time_ms) {
        return 0.0f;
    }
    
    float dt = pid->first_run ? (pid->config.sample_time_ms / 1000.0f) : 
               ((now - pid->last_time) / 1000.0f);
    
    // Calculate error
    float error = setpoint - input;
    
    // Proportional term
    float p_term = pid->config.kp * error;
    
    // Integral term with anti-windup
    pid->integral += error * dt;
    
    // Limit integral to prevent windup
    float integral_limit = pid->config.output_max / pid->config.ki;
    if (pid->integral > integral_limit) {
        pid->integral = integral_limit;
    } else if (pid->integral < -integral_limit) {
        pid->integral = -integral_limit;
    }
    
    float i_term = pid->config.ki * pid->integral;
    
    // Derivative term (on measurement to avoid derivative kick)
    float d_input = pid->first_run ? 0.0f : (input - pid->last_input) / dt;
    float d_term = -pid->config.kd * d_input;
    
    // Calculate total output
    float output = p_term + i_term + d_term;
    
    // Apply output limits
    if (output > pid->config.output_max) {
        output = pid->config.output_max;
        // Back-calculate integral for anti-windup
        if (pid->config.ki != 0) {
            pid->integral = (output - p_term - d_term) / pid->config.ki;
        }
    } else if (output < pid->config.output_min) {
        output = pid->config.output_min;
        // Back-calculate integral for anti-windup
        if (pid->config.ki != 0) {
            pid->integral = (output - p_term - d_term) / pid->config.ki;
        }
    }
    
    // Update state
    pid->last_error = error;
    pid->last_input = input;
    pid->last_time = now;
    pid->first_run = false;
    
    return output;
}

void pid_controller_reset(pid_controller_t *pid) {
    pid->integral = 0.0f;
    pid->last_error = 0.0f;
    pid->last_input = 0.0f;
    pid->first_run = true;
}

void pid_controller_set_tunings(pid_controller_t *pid, float kp, float ki, float kd) {
    if (kp < 0 || ki < 0 || kd < 0) return;
    
    pid->config.kp = kp;
    pid->config.ki = ki;
    pid->config.kd = kd;
}

void pid_controller_set_output_limits(pid_controller_t *pid, float min, float max) {
    if (min >= max) return;
    
    pid->config.output_min = min;
    pid->config.output_max = max;
    
    // Apply limits to integral
    float integral_limit = max / pid->config.ki;
    if (pid->integral > integral_limit) {
        pid->integral = integral_limit;
    } else if (pid->integral < -integral_limit) {
        pid->integral = -integral_limit;
    }
}

void pid_controller_set_sample_time(pid_controller_t *pid, uint32_t sample_time_ms) {
    if (sample_time_ms > 0) {
        pid->config.sample_time_ms = sample_time_ms;
    }
}

void pid_controller_get_tunings(pid_controller_t *pid, float *kp, float *ki, float *kd) {
    if (kp) *kp = pid->config.kp;
    if (ki) *ki = pid->config.ki;
    if (kd) *kd = pid->config.kd;
}