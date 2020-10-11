#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/queue.h"

#include "./controller.h"
#include "./server.h"

static char *TAG = "robot-server";

static esp_err_t get_handler(httpd_req_t *req) {
  extern const char *remote_html_start asm("_binary_remote_html_start");
  extern const char *remote_html_end asm("_binary_remote_html_end");
  const size_t remote_html_len = (remote_html_end - remote_html_start);
  httpd_resp_send(req, remote_html_start, remote_html_len);

  return ESP_OK;
}

static void handle_message(char *payload) {
  cJSON *msg = cJSON_Parse(payload);
  cJSON *jsonPosition = cJSON_GetObjectItem(msg, "position");

  if (jsonPosition) {
    ESP_LOGI(TAG, "Got packet with message: %s", payload);
    float x = cJSON_GetObjectItem(jsonPosition, "x")->valuedouble;
    float y = cJSON_GetObjectItem(jsonPosition, "y")->valuedouble;

    remote_event event = {.new_position = {x, y}};

    assert(control_queue != NULL);
    if (!xQueueSend(control_queue, &event, 10 / portTICK_PERIOD_MS)) {
      ESP_LOGE(TAG, "Control queue is full, dropping event");
    }
  }

  cJSON_Delete(msg);
}

static char *current_state_json() {
  cJSON *msg = cJSON_CreateObject();

  cJSON_AddNumberToObject(msg, "left",
                          global_controller.left_motor.current_speed);
  cJSON_AddNumberToObject(msg, "right",
                          global_controller.right_motor.current_speed);
  cJSON_AddNumberToObject(msg, "front_distance",
                          global_controller.front_distance);

  char *result = cJSON_Print(msg);

  cJSON_Delete(msg);

  return result;
}

static esp_err_t send_ws_response(httpd_req_t *req) {
  char *data = current_state_json();

  httpd_ws_frame_t ws_response = {.payload = (uint8_t *)data,
                                  .len = strlen(data),
                                  .type = HTTPD_WS_TYPE_TEXT,
                                  .final = true};

  esp_err_t ret = httpd_ws_send_frame(req, &ws_response);
  free(data);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
  }

  return ret;
}

static esp_err_t ws_handler(httpd_req_t *req) {
  uint8_t buf[128] = {0};
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.payload = buf;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 128);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
    return ret;
  }
  if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
    handle_message((char *)ws_pkt.payload);
  }

  return send_ws_response(req);
}

httpd_uri_t uri_root = {
    .uri = "/", .method = HTTP_GET, .handler = get_handler, .user_ctx = NULL};

static const httpd_uri_t ws = {.uri = "/websocket",
                               .method = HTTP_GET,
                               .handler = ws_handler,
                               .user_ctx = NULL,
                               .is_websocket = true};

httpd_handle_t start_webserver() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t server = NULL;

  esp_err_t result = httpd_start(&server, &config);
  if (result == ESP_OK) {
    ESP_LOGI(TAG, "serving requests");
    httpd_register_uri_handler(server, &uri_root);
    httpd_register_uri_handler(server, &ws);
  } else {
    ESP_LOGE(TAG, "failed to initialize server");

    ESP_LOGW(TAG, "Got error: %s", esp_err_to_name(result));
  }

  return server;
}
