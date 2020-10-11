#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "controller.h"
#include "ultrasonic.h"

#define ESP_INTR_FLAG_DEFAULT 0

static const char *TAG = "robot-ultrasonic";

// Select pin number as a GPIO and set it to output mode
static void initialize_output_gpio(int pin_number) {
  gpio_pad_select_gpio(pin_number);
  gpio_set_direction(pin_number, GPIO_MODE_OUTPUT);
}

// Select pin number as a GPIO and set it to output mode
static void initialize_input_gpio(int pin_number) {
  gpio_pad_select_gpio(pin_number);
  gpio_set_direction(pin_number, GPIO_MODE_INPUT);
}

#define RING_BUFFER_SIZE 3
typedef struct {
  float values[RING_BUFFER_SIZE];
  int current_index;
} ring_buffer;

void ring_buffer_push(ring_buffer *buffer, float value) {
  buffer->current_index = (buffer->current_index + 1) % RING_BUFFER_SIZE;

  buffer->values[buffer->current_index] = value;
}

float median_of(float a, float b, float c) {
  int x = a - b;
  int y = b - c;
  int z = a - c;
  if (x * y > 0)
    return b;
  if (x * z > 0)
    return c;
  return a;
}

float ring_buffer_median(ring_buffer *buffer) {
  return median_of(buffer->values[0], buffer->values[1], buffer->values[2]);
}

typedef struct {
  gpio_num_t pin;
  int64_t timestamp;
} gpio_event;

static void IRAM_ATTR gpio_isr_handler(void *arg) {
  ultrasonic_sensor *sensor = (ultrasonic_sensor *)arg;
  gpio_event event = {.pin = sensor->echo, .timestamp = esp_timer_get_time()};

  xQueueSendFromISR(sensor->event_queue, &event, NULL);
}

static const float CM_ROUNDTRIP_US = 58;

static void process_gpio_events(void *arg) {
  ultrasonic_sensor *sensor = (ultrasonic_sensor *)arg;
  gpio_event event;
  int64_t pulse_start = 0;
  ring_buffer running_average = {};
  int idx = 0;
  int last_printed = -10;

  for (;;) {
    if (xQueueReceive(sensor->event_queue, &event, portMAX_DELAY)) {
      idx++;
      int pin_state = gpio_get_level(event.pin);

      if (pin_state == 1 && sensor->state == triggered) {
        sensor->state = reading;
        pulse_start = event.timestamp;
      } else if (pin_state == 0 && sensor->state == reading) {
        sensor->state = idle;
        float distance = (event.timestamp - pulse_start) / CM_ROUNDTRIP_US;
        ring_buffer_push(&running_average, distance);
        float median = ring_buffer_median(&running_average);
        global_controller.front_distance = median;

        if ((idx - last_printed) >= 10) {
          ESP_LOGI(TAG, "Raw: %f Running Median: %f", distance, median);
          last_printed = idx;
        }
      } else {
        /* ESP_LOGW(TAG, "Sensor is in invalid state: pin = %d, state = %d", */
        /*          pin_state, sensor->state); */
        sensor->state = idle; // Reset to default state
      }
    }
  }
}

static void initialize_ultrasonic_sensor(ultrasonic_sensor *sensor,
                                         gpio_num_t trig, gpio_num_t echo) {
  sensor->trig = trig;
  sensor->echo = echo;
  sensor->event_queue = xQueueCreate(3, sizeof(gpio_event));
  sensor->state = idle;

  initialize_output_gpio(trig);
  gpio_set_level(trig, 0);

  xTaskCreate(process_gpio_events, "process_gpio_events", 2048, (void *)sensor,
              10, NULL);

  initialize_input_gpio(echo);
  gpio_set_intr_type(echo, GPIO_INTR_ANYEDGE);
  gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
}

// Sleep for the specified number of microseconds
static void delay_usecs(int usecs) {
  float delay_msecs = 10 / 1000;
  vTaskDelay(delay_msecs / portTICK_PERIOD_MS);
}

#define timeout_expired(start, time) ((esp_timer_get_time() - start) >= time)

// Initiate cycle on trigger pin which will then trigger interrupts on our
// echo pin
static void trigger_read(ultrasonic_sensor *sensor) {
  if (sensor->state == idle) {
    // Set trigger high for 10 microseconds, then low to start cycle
    ESP_ERROR_CHECK(gpio_set_level(sensor->trig, 1));
    delay_usecs(10);
    ESP_ERROR_CHECK(gpio_set_level(sensor->trig, 0));
    sensor->state = triggered;
  } else {
    ESP_LOGW(TAG, "Sensor is not idle, skipping read. State: %i",
             sensor->state);
  }
}

#define TRIG_GPIO 25
#define ECHO_GPIO 33

void poll_distance() {
  ultrasonic_sensor sensor = {};
  initialize_ultrasonic_sensor(&sensor, TRIG_GPIO, ECHO_GPIO);

  gpio_isr_handler_add(sensor.echo, gpio_isr_handler, (void *)&sensor);

  while (true) {
    vTaskDelay(100 / portTICK_PERIOD_MS);

    trigger_read(&sensor);
  }
}
