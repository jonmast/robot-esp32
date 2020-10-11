#include "driver/gpio.h"
#include "driver/mcpwm.h"
#include "esp_log.h"

#include "motor.h"

static char *TAG = "robot-motor";

// Select pin number as a GPIO and set it to output mode
static void initialize_output_gpio(int pin_number) {
  gpio_pad_select_gpio(pin_number);
  gpio_set_direction(pin_number, GPIO_MODE_OUTPUT);
}

static void initialize_pwm_output(int pin_number, int pwm_unit, int pwm_timer,
                                  mcpwm_operator_t pwm_op) {
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, pin_number);

  mcpwm_config_t pwm_config;
  pwm_config.frequency = 10000;
  pwm_config.cmpr_a = 0;
  pwm_config.cmpr_b = 0;
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
  mcpwm_init(pwm_unit, pwm_timer, &pwm_config);
  /* mcpwm_set_signal_low(pwm_unit, pwm_timer, pwm_op); */
}

void stop_motor(motor *m) {
  gpio_set_level(m->in1, 0);
  gpio_set_level(m->in2, 0);
  m->current_speed = 0;
}

void brake_motor(motor *m) {
  gpio_set_level(m->in1, 1);
  gpio_set_level(m->in2, 1);
  m->current_speed = 0;
}

motor initialize_motor(int in1, int in2, int stdb, int pwm,
                       mcpwm_unit_t pwm_unit, mcpwm_timer_t pwm_timer,
                       mcpwm_operator_t pwm_op) {
  motor new_motor = {
      .in1 = in1,
      .in2 = in2,
      .pwm = pwm,
      .pwm_unit = pwm_unit,
      .pwm_timer = pwm_timer,
      .pwm_op = pwm_op,
      .current_speed = 0,
  };

  initialize_output_gpio(in1);
  initialize_output_gpio(in2);
  // are we allowed to re-initialize when sharing between motors?
  initialize_output_gpio(stdb);
  stop_motor(&new_motor);
  initialize_pwm_output(pwm, pwm_unit, pwm_timer, pwm_op);

  // Disable standby mode
  gpio_set_level(stdb, 1);

  return new_motor;
}

// Set motor to run at a percentage of it's maximum speed
//
// Can be specified as negative for backwards motion
void set_motor_speed(motor *m, float speed) {
  assert(speed <= 100);
  assert(speed >= -100);

  ESP_LOGI(TAG, "Setting motor speed to %f", speed);

  if (speed >= 0) {
    // Forward motion
    gpio_set_level(m->in1, 0);
    gpio_set_level(m->in2, 1);
    mcpwm_set_duty(m->pwm_unit, m->pwm_timer, m->pwm_op, speed);
  } else {
    // Backward motion
    gpio_set_level(m->in1, 1);
    gpio_set_level(m->in2, 0);
    mcpwm_set_duty(m->pwm_unit, m->pwm_timer, m->pwm_op, -speed);
  }

  m->current_speed = speed;
}
