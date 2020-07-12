#pragma once

#include "esp_http_server.h"

typedef struct {
  enum remote_direction { Up, Right, Down, Left } direction;
  enum remote_action { Start, End } action;
} remote_event;

httpd_handle_t start_webserver();

