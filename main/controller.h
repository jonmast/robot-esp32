#pragma once

#include "./motor.h"
#include "./server.h"

// Indexes of x and y in remote_position vector
#define X_IDX 0
#define Y_IDX 1

typedef struct {
  motor left_motor;
  motor right_motor;
  float remote_position[2];
} controller;

void update_state(remote_event event);
void control_loop(void *pvParameters);

extern controller global_controller;
