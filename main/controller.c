#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "math.h"

#include "controller.h"
#include "motor.h"
#include "server.h"

static const char *TAG = "robot-controller";

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

static void control_sync() {
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

static void update_state(remote_event *event) {
  global_controller.remote_position[X_IDX] = event->new_position[X_IDX];
  global_controller.remote_position[Y_IDX] = event->new_position[Y_IDX];

  if (event->new_position[X_IDX] == 0 && event->new_position[Y_IDX] == 0) {
    stop_motor(&global_controller.left_motor);
  }

  control_sync();
}

static void remote_input() {
  remote_event event;
  while (true) {
    if (xQueueReceive(control_queue, &event, portMAX_DELAY)) {
      update_state(&event);
    }
  }
}

typedef struct {
  int64_t last_changed;
  enum { forward_motion, turning } status;
} autonomous_state;

static void go_forward(autonomous_state *state, const int64_t current_time) {
  state->status = forward_motion;
  state->last_changed = current_time;

  set_motor_speed(&global_controller.left_motor, 40);
  set_motor_speed(&global_controller.right_motor, 40);
}

static void forward_turn(autonomous_state *state, const int64_t current_time) {
  state->status = turning;
  state->last_changed = current_time;

  if ((esp_random() % 2) == 0) {
    set_motor_speed(&global_controller.left_motor, 50);
    stop_motor(&global_controller.right_motor);
  } else {
    stop_motor(&global_controller.left_motor);
    set_motor_speed(&global_controller.right_motor, 50);
  }
}

static void backward_turn(autonomous_state *state, const int64_t current_time) {
  state->status = turning;
  state->last_changed = current_time;

  if ((esp_random() % 2) == 0) {
    stop_motor(&global_controller.left_motor);
    set_motor_speed(&global_controller.right_motor, -50);
  } else {
    set_motor_speed(&global_controller.left_motor, -50);
    stop_motor(&global_controller.right_motor);
  }
}

static void autonomous_loop() {
  autonomous_state state = {.status = forward_motion,
                            .last_changed = esp_timer_get_time()};
  go_forward(&state, esp_timer_get_time());

  while (true) {
    vTaskDelay(10 / portTICK_PERIOD_MS);

    int64_t current_time = esp_timer_get_time();

    int64_t time_in_mode = current_time - state.last_changed;
    bool obstructed = global_controller.front_distance < 60;

    switch (state.status) {
    case forward_motion:
      if (obstructed) {
        ESP_LOGI(TAG, "Seeing obstacle ahead, backing away");
        backward_turn(&state, current_time);
      } else if (time_in_mode >= 3000000) {
        ESP_LOGI(TAG, "Getting bored of this course, switching it up");
        forward_turn(&state, current_time);
      }
      break;
    case turning:
      if (time_in_mode >= 1500000 && !obstructed) {
        ESP_LOGI(TAG, "Turn complete, resuming course");
        go_forward(&state, current_time);
      }
      break;
    default:
      break;
    }
  }
}

void control_init() {
  control_queue = xQueueCreate(3, sizeof(remote_event));

  xTaskCreate(remote_input, "remote_input", 2048, NULL, 10, NULL);
  xTaskCreate(autonomous_loop, "autonomous_loop", 2048, NULL, 10, NULL);
}
