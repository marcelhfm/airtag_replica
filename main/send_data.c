#include "send_data.h"
#include "FreeRTOSConfig.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "include/main.h"
#include "portmacro.h"
#include <sys/time.h>

extern QueueHandle_t measurements_queue;

static const char *S_TAG = "SEND_DATA";

void send_data_task(void *pvParameters) {
  float measurement;

  if (xQueueReceive(measurements_queue, &measurement, portMAX_DELAY) ==
      pdTRUE) {
    ESP_LOGI(S_TAG, "Received measurement %0.2f", measurement);
  }

  enter_deep_sleep();
}
