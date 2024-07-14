#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
static const char *R_TAG = "READ_MEASUREMENTS";

static int adc_raw;

extern QueueHandle_t measurements_queue;
extern TaskHandle_t send_data_task_handle;

void read_measurements_task(void *pvParameters) {
  adc_oneshot_unit_handle_t adc1_handle;
  adc_oneshot_unit_init_cfg_t init_config = {.unit_id = ADC_UNIT_1};
  adc_oneshot_chan_cfg_t config = {.bitwidth = ADC_BITWIDTH_DEFAULT,
                                   .atten = ADC_ATTEN_DB_12};

  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

  ESP_ERROR_CHECK(
      adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_2, &config));

  float moisture_percent;
  float sum = 0.0;

  for (int i = 0; i < 5; i++) {
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_2, &adc_raw));
    ESP_LOGI(R_TAG, "Moisture raw data: %d", adc_raw);

    moisture_percent = 100.0 - ((float)adc_raw / 4095.0 * 100.0);
    ESP_LOGI(R_TAG, "Moisture percentage %.2f", moisture_percent);

    sum = sum + moisture_percent;
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  float avg = sum / 5;
  ESP_LOGI(R_TAG, "Moisture average %.2f", avg);

  ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));

  xQueueSend(measurements_queue, &avg, 0);
  vTaskDelete(NULL);
}
