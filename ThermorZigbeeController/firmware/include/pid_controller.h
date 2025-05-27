#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

// PID controller configuration
typedef struct {
    float kp;                // Proportional gain
    float ki;                // Integral gain
    float kd;                // Derivative gain
    float output_min;        // Minimum output value
    float output_max;        // Maximum output value
    uint32_t sample_time_ms; // Sample time in milliseconds
} pid_config_t;

// PID controller state
typedef struct {
    pid_config_t config;
    float integral;          // Integral accumulator
    float last_error;        // Previous error for derivative
    float last_input;        // Previous input for derivative on measurement
    uint32_t last_time;      // Last computation time
    bool first_run;          // First run flag
} pid_controller_t;

// Function prototypes
void pid_controller_init(pid_controller_t *pid, const pid_config_t *config);
float pid_controller_compute(pid_controller_t *pid, float setpoint, float input);
void pid_controller_reset(pid_controller_t *pid);
void pid_controller_set_tunings(pid_controller_t *pid, float kp, float ki, float kd);
void pid_controller_set_output_limits(pid_controller_t *pid, float min, float max);
void pid_controller_set_sample_time(pid_controller_t *pid, uint32_t sample_time_ms);

// Advanced features
void pid_controller_set_auto_tune(pid_controller_t *pid, bool enable);
void pid_controller_get_tunings(pid_controller_t *pid, float *kp, float *ki, float *kd);

#endif // PID_CONTROLLER_H