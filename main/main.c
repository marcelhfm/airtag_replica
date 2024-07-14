#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "include/logging.h"
#include "include/nvs_helper.h"
#include "include/ota.h"
#include "include/read_measurements.h"
#include "include/send_data.h"
#include "include/wifi_helper.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <inttypes.h>
#include <stdint.h>
#include <sys/_timeval.h>
#include <sys/time.h>

static const char *TAG = "MAIN";

TaskHandle_t ota_task_handle = NULL;
TaskHandle_t read_measurements_task_handle = NULL;
TaskHandle_t send_data_task_handle = NULL;

QueueHandle_t measurements_queue;

static RTC_DATA_ATTR struct timeval sleep_enter_time;

void enter_deep_sleep() {
  ESP_LOGI(TAG, "Entering deep sleep zzz... zzz... zzz...");
  gettimeofday(&sleep_enter_time, NULL);

  esp_deep_sleep_start();
}

void start_tasks() {
  struct timeval now;
  gettimeofday(&now, NULL);
  int sleep_time_mins = (now.tv_sec - sleep_enter_time.tv_sec) / 60 +
                        (now.tv_usec - sleep_enter_time.tv_usec) / 1000000 / 60;

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    ESP_LOGI(TAG, "Wake up from timer. Time spent in deep sleep: %d mins",
             sleep_time_mins);
  } else {
    ESP_LOGW(TAG,
             "Unexpected wakeup cause: %d Time spent in deep sleep: %d mins",
             cause, sleep_time_mins);
  }

  xTaskCreate(&ota_update_task, "ota_task", 4096, NULL, 5, &ota_task_handle);
  xTaskCreate(&read_measurements_task, "read_measurements_task", 4096, NULL, 4,
              &read_measurements_task_handle);
  xTaskCreate(&send_data_task, "send_data_task", 4096, NULL, 5,
              &send_data_task_handle);
}

void register_rtc_timer_deep_sleep() {
  const long long wakeup_time_sec = 60 * 60;
  ESP_LOGI(TAG, "Enabling timer wakeup, %llds", wakeup_time_sec);
  ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000));
}

void app_main(void) {
  ESP_LOGI(TAG, "Hello from irrigation!");

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

  udp_socket_init();
  esp_log_set_vprintf(custom_log_handler);

  measurements_queue = xQueueCreate(5, sizeof(float));

  register_rtc_timer_deep_sleep();

  // Only this should be triggered when woken up
  start_tasks();
}
