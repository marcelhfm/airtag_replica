#include "send_data.h"
#include "FreeRTOSConfig.h"
#include "esp_event_base.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "include/main.h"
#include "mqtt_client.h"
#include "portmacro.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "mqtt_client.h"

extern QueueHandle_t measurements_queue;

static const char *S_TAG = "SEND_DATA";
static esp_mqtt_client_handle_t mqtt_client = NULL;

void log_error_if_nonzero(const char *message, int error_code) {
  if (error_code != 0) {
    ESP_LOGE(S_TAG, "Last error %s: 0x%x", message, error_code);
  }
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                        int32_t event_id, void *event_data) {
  ESP_LOGD(S_TAG, "Event dispatched from event loog base=%s event_id=%" PRIi32,
           base, event_id);
  esp_mqtt_event_handle_t event = event_data;
  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(S_TAG, "MQTT_EVENT_CONNECTED");
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(S_TAG, "MQTT_EVENT_DISCONNECTED");
    break;
  case MQTT_EVENT_PUBLISHED:
    ESP_LOGI(S_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGI(S_TAG, "MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
      log_error_if_nonzero("reported from esp-tls",
                           event->error_handle->esp_tls_last_esp_err);
      log_error_if_nonzero("reported from tls stack",
                           event->error_handle->esp_tls_stack_err);
      log_error_if_nonzero("reported as transport's socket errno",
                           event->error_handle->esp_transport_sock_errno);
      ESP_LOGI(S_TAG, "Last errno string (%s)",
               strerror(event->error_handle->esp_transport_sock_errno));
    }
    break;
  default:
    ESP_LOGI(S_TAG, "Other event id:%d", event->event_id);
    break;
  }
}

void mqtt_start(void) {
  esp_mqtt_client_config_t mqtt_cfg = {.broker.address.uri =
                                           CONFIG_HOME_SERVER_MQTT_URI};
  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                 mqtt_event_handler, NULL);
  esp_mqtt_client_start(mqtt_client);
}

void send_data_task(void *pvParameters) {
  float measurement;

  if (xQueueReceive(measurements_queue, &measurement, portMAX_DELAY) ==
      pdTRUE) {
    ESP_LOGI(S_TAG, "Received measurement %0.2f", measurement);
    char payload[50];
    snprintf(payload, sizeof(payload), "2,%0.2f", measurement);
    int msg_id = esp_mqtt_client_publish(mqtt_client, "/2/measurement", payload,
                                         0, 1, 0);
    ESP_LOGI(S_TAG, "Measurement sent, msg_id=%d", msg_id);
  }

  ESP_LOGI(S_TAG, "Waiting for other task to finish before going to sleep...");
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

  enter_deep_sleep();
}
