#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "math.h"

#include "controller.h"
#include "motor.h"
#include "server.h"

static char *TAG = "robot-controller";

void update_state(remote_event event) {
  if (event.action == End) {
    ESP_LOGI(TAG, "Stopping");
    if (event.direction == Right || event.direction == Left) {
      global_controller.remote_position[X_IDX] = 0;
    } else if (event.direction == Down || event.direction == Up) {
      global_controller.remote_position[Y_IDX] = 0;
    }
    // TODO turning
    stop_motor(&global_controller.left_motor);
  } else if (event.action == Start) {
    if (event.direction == Up) {
      ESP_LOGI(TAG, "Going forward");

      global_controller.remote_position[Y_IDX] = 100;
    } else if (event.direction == Right) {
      ESP_LOGI(TAG, "Going right");

      global_controller.remote_position[X_IDX] = 100;
    } else if (event.direction == Down) {
      ESP_LOGI(TAG, "Going backward");

      global_controller.remote_position[Y_IDX] = -100;
    } else if (event.direction == Left) {
      ESP_LOGI(TAG, "Going left");

      global_controller.remote_position[X_IDX] = -100;
    } else {
      ESP_LOGE(TAG, "Invalid direction");
    }
  } else {
    ESP_LOGE(TAG, "Invalid action");
  }
}

float sign(float n) {
  if (n > 0)
    return 1;
  if (n < 0)
    return -1;

  return 0;
}

float left_target() {
  float x = global_controller.remote_position[X_IDX];
  float y = global_controller.remote_position[Y_IDX];

  if (x >= 0) {
    return y;
  } else {
    return y * (100 - fabsf(x));
  }
}

float right_target() {
  float x = global_controller.remote_position[X_IDX];
  float y = global_controller.remote_position[Y_IDX];

  if (x <= 0) {
    return y;
  } else {
    return y * (100 - fabsf(x));
  }
}

// Adjust speed by 5% every 10ms
static int TICK_MS = 10;
static float TICK_RAMP = 1;

void control_loop(void *pvParameters) {
  // TODO: This should really be event driven
  while (true) {
    vTaskDelay(TICK_MS / portTICK_PERIOD_MS);

    {
      float target_speed = left_target();
      float current_speed = global_controller.left_motor.current_speed;
      if (current_speed != target_speed) {
        float ramped = current_speed + (sign(target_speed) * TICK_RAMP);
        if (fabsf(ramped) > fabsf(target_speed)) {
          ESP_LOGI(TAG, "clamping to target %f", target_speed);
          set_motor_speed(&global_controller.left_motor, target_speed);
        } else {
          ESP_LOGI(TAG, "left");
          set_motor_speed(&global_controller.left_motor, ramped);
        }
      }
    }
    {
      float target_speed = right_target();
      float current_speed = global_controller.right_motor.current_speed;
      if (current_speed != target_speed) {
        float ramped = current_speed + (sign(target_speed) * TICK_RAMP);
        if (fabsf(ramped) > fabsf(target_speed)) {
          ESP_LOGI(TAG, "clamping to target %f", target_speed);
          set_motor_speed(&global_controller.right_motor, target_speed);
        } else {
          ESP_LOGI(TAG, "right");
          set_motor_speed(&global_controller.right_motor, ramped);
        }
      }
    }
  }
}
