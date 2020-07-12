#pragma once

#include "driver/mcpwm.h"

typedef struct {
  int in1;
  int in2;
  int pwm;
  mcpwm_unit_t pwm_unit;
  mcpwm_timer_t pwm_timer;
  mcpwm_operator_t pwm_op;
  float current_speed;
} motor;

motor initialize_motor(int in1, int in2, int stdb, int pwm,
                       mcpwm_unit_t pwm_unit, mcpwm_timer_t pwm_timer,
                       mcpwm_operator_t pwm_op);

void set_motor_speed(motor *m, float speed);
void brake_motor(motor *m);
void stop_motor(motor *m);
