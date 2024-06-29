#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/idf_additions.h"
#include <string.h>

#define MAX_HTTP_RECV_BUFFER 4096

static const char *TAG = "OTA";

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
  switch (evt->event_id) {
  case HTTP_EVENT_ERROR:
    ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
    break;
  case HTTP_EVENT_ON_CONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
    break;
  case HTTP_EVENT_HEADER_SENT:
    ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
    break;
  case HTTP_EVENT_ON_HEADER:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key,
             evt->header_value);
    break;
  case HTTP_EVENT_ON_DATA:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
    break;
  case HTTP_EVENT_ON_FINISH:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
    break;
  case HTTP_EVENT_DISCONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
    break;
  case HTTP_EVENT_REDIRECT:
    ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
    break;
  }
  return ESP_OK;
}

char *http_get(const char *url) {
  char *buffer = malloc(MAX_HTTP_RECV_BUFFER + 1);
  if (buffer == NULL) {
    ESP_LOGE(TAG, "Cannot malloc http receive buffer");
    return NULL;
  }

  ESP_LOGI(TAG, "Attempting to fetch from %s", url);
  esp_http_client_config_t config = {.url = url,
                                     .event_handler = _http_event_handler,
                                     .user_agent = "ESP32-OTA-Client",
                                     .crt_bundle_attach = esp_crt_bundle_attach,
                                     .timeout_ms = 10000,
                                     .buffer_size_tx = 10240};

  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t err;

  if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    free(buffer);
    return NULL;
  }
  int content_length = esp_http_client_fetch_headers(client);
  int total_read_len = 0, read_len;
  if (total_read_len < content_length &&
      content_length <= MAX_HTTP_RECV_BUFFER) {
    read_len = esp_http_client_read(client, buffer, content_length);
    if (read_len <= 0) {
      ESP_LOGE(TAG, "Error read data");
    }
    buffer[read_len] = 0;
    ESP_LOGI(TAG, "read_len = %d", read_len);
  }
  ESP_LOGI(TAG, "Http Stream reader status = %d, content_length = %lld",
           esp_http_client_get_status_code(client),
           esp_http_client_get_content_length(client));

  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  return buffer;
}

char *extract_tag_name(const char *json) {
  const char *tag_key = "\"tag_name\":\"";
  char *tag_start = strstr(json, tag_key);
  if (!tag_start) {
    return NULL;
  }
  tag_start += strlen(tag_key);
  char *tag_end = strchr(tag_start, '"');
  if (!tag_end) {
    return NULL;
  }
  size_t tag_length = tag_end - tag_start;
  char *tag_name = (char *)malloc(tag_length + 1);
  if (!tag_name) {
    return NULL;
  }

  strncpy(tag_name, tag_start, tag_length);
  tag_name[tag_length] = '\0';
  return tag_name;
}

char *extract_core_version(const char *version) {
  char *core_version = (char *)malloc(strlen(version) + 1);
  if (!core_version) {
    return NULL;
  }

  strcpy(core_version, version);
  char *hyphen = strchr(core_version, '-');
  if (hyphen) {
    *hyphen = '\0';
  }
  return core_version;
}

int compare_versions(const char *v1, const char *v2) {
  int major1, minor1, patch1;
  int major2, minor2, patch2;

  sscanf(v1, "v%d.%d.%d", &major1, &minor1, &patch1);
  sscanf(v2, "v%d.%d.%d", &major2, &minor2, &patch2);

  if (major1 != major2)
    return major1 - major2;
  if (minor1 != minor2)
    return minor1 - minor2;
  return patch1 - patch2;
}

void perform_ota() {
  esp_http_client_config_t config = {.url = CONFIG_FIRMWARE_URL,
                                     .crt_bundle_attach = esp_crt_bundle_attach,
                                     .event_handler = _http_event_handler,
                                     .keep_alive_enable = true,
                                     .timeout_ms = 10000,
                                     .buffer_size_tx = 10240};

  config.skip_cert_common_name_check = true;

  esp_https_ota_config_t ota_config = {
      .http_config = &config,
  };

  ESP_LOGI(TAG, "Attempting to download update from %s", config.url);
  esp_err_t ret = esp_https_ota(&ota_config);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "OTA Succeed, Rebooting...");
    esp_restart();
  } else {
    ESP_LOGE(TAG, "Firmware upgrade failed");
  }
}

void ota_update_task(void *pvParameters) {
  ESP_LOGI(TAG, "Starting OTA example task");

  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_app_desc_t running_app_info;
  esp_ota_get_partition_description(running, &running_app_info);

  char *response = http_get(CONFIG_FIRMWARE_VERSION_URL);
  if (!response) {
    ESP_LOGE(TAG, "Failed to fetch from latest release info");
    vTaskDelete(NULL);
    return;
  }

  char *tag_name = extract_tag_name(response);
  free(response);
  if (!tag_name) {
    ESP_LOGE(TAG, "Failed to extract tag_name from response");
    vTaskDelete(NULL);
    return;
  }

  char *current_version = extract_core_version(running_app_info.version);
  char *latest_version = extract_core_version(tag_name);

  if (!current_version || !latest_version) {
    ESP_LOGE(TAG, "Failed to extract core version");
    free(tag_name);
    free(current_version);
    free(latest_version);
    vTaskDelete(NULL);
    return;
  }

  if (compare_versions(current_version, latest_version) >= 0) {
    ESP_LOGI(TAG,
             "Not updating to new version. The new firmware is not newer. "
             "Current: %s Latest: %s",
             current_version, latest_version);
    free(tag_name);
    free(current_version);
    free(latest_version);
    vTaskDelete(NULL);
    return;
  }

  ESP_LOGI(TAG, "Updating to version %s", latest_version);
  free(tag_name);
  free(current_version);
  free(latest_version);
  perform_ota();

  while (1) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
