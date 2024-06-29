#include "./include/nvs_helper.h"
#include "./include/ota.h"
#include "./include/wifi_helper.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <inttypes.h>
#include <stdint.h>

static const char *TAG = "MAIN";

void app_main(void) {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  char ssid[25];
  char password[25];

  read_wifi_credentials(ssid, sizeof(ssid), password, sizeof(password));

  wifi_init_sta(ssid, password);

  xTaskCreate(&ota_update_task, "ota_task", 8192, NULL, 5, NULL);

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
