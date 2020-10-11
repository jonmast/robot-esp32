#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <stdio.h>

#define BIN1_GPIO 23
#define BIN2_GPIO 22
#define PWMB_GPIO 13

#define AIN1_GPIO 14
#define AIN2_GPIO 27
#define PWMA_GPIO 26

#define STDBY_GPIO 12

#include "./controller.h"
#include "./motor.h"
#include "./server.h"
#include "./ultrasonic.h"
#include "./wifi.h"

controller global_controller = {.remote_position = {0, 0}};

void app_main(void) {
  printf("Hello world!\n");

  /* Print chip information */
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  printf("This is %s chip with %d CPU cores, WiFi%s%s, ", CONFIG_IDF_TARGET,
         chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
         (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

  printf("silicon revision %d, ", chip_info.revision);

  printf("%dMB %s flash\n", (int)spi_flash_get_chip_size() / (1024 * 1024),
         (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded"
                                                       : "external");

  printf("Free heap: %d\n", esp_get_free_heap_size());

  // Initialize NVS
  // required for wifi
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  motor left_motor =
      initialize_motor(BIN1_GPIO, BIN2_GPIO, STDBY_GPIO, PWMB_GPIO,
                       MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
  global_controller.left_motor = left_motor;

  motor right_motor =
      initialize_motor(AIN1_GPIO, AIN2_GPIO, STDBY_GPIO, PWMA_GPIO,
                       MCPWM_UNIT_1, MCPWM_TIMER_1, MCPWM_OPR_B);
  global_controller.right_motor = right_motor;

  xTaskCreate(poll_distance, "poll_distance", 2048, NULL, 10, NULL);

  control_init();
  init_wifi(&start_webserver);
}
