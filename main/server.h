#pragma once

#include "esp_http_server.h"

typedef struct {
  float new_position[2];
} remote_event;

httpd_handle_t start_webserver();

