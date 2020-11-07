#pragma once

#include "esp_http_server.h"

#include "controller.h"

enum remote_event_type { position, mode };

typedef struct {
  enum remote_event_type type;

  union {
    // Position update
    struct {
      float new_position[2];
    };
    // Mode update
    struct {
      enum control_mode new_mode;
    };
  };
} remote_event;

httpd_handle_t start_webserver();
