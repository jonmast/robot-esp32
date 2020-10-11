#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "math.h"

#include "controller.h"
#include "motor.h"
#include "server.h"

void update_state(remote_event *event) {
  global_controller.remote_position[X_IDX] = event->new_position[X_IDX];
  global_controller.remote_position[Y_IDX] = event->new_position[Y_IDX];

  if (event->new_position[X_IDX] == 0 && event->new_position[Y_IDX] == 0) {
    stop_motor(&global_controller.left_motor);
  }
}

static float clamped(float x) {
  float upper = 100;
  float lower = -100;
  return fmin(upper, fmax(x, lower));
}

static float X_FACTOR = 2;

static float left_target() {
  float x = global_controller.remote_position[X_IDX];
  float y = global_controller.remote_position[Y_IDX];

  return clamped(y + (x / X_FACTOR));
}

static float right_target() {
  float x = global_controller.remote_position[X_IDX];
  float y = global_controller.remote_position[Y_IDX];

  return clamped(y - (x / X_FACTOR));
}

static float MIN_SPEED = 7;

void control_sync() {
  {
    float target_speed = left_target();

    if (fabsf(target_speed) < MIN_SPEED) {
      stop_motor(&global_controller.left_motor);
    } else {
      set_motor_speed(&global_controller.left_motor, target_speed);
    }
  }
  {
    float target_speed = right_target();

    if (fabsf(target_speed) < 10) {
      stop_motor(&global_controller.right_motor);
    } else {
      set_motor_speed(&global_controller.right_motor, target_speed);
    }
  }
}

void control_loop() {
  remote_event event;
  while (true) {
    if (xQueueReceive(control_queue, &event, portMAX_DELAY)) {
      update_state(&event);
    }
  }
}

void control_init() {
  control_queue = xQueueCreate(3, sizeof(remote_event));

  xTaskCreate(control_loop, "control_loop", 2048, NULL, 10, NULL);
}
