#include "esp_log.h"
#include "nvs.h"

char *TAG = "NVS_HELPER";

void store_wifi_credentials(const char *ssid, const char *password) {
  nvs_handle_t nvs_handle;

  esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle", esp_err_to_name(err));
    return;
  }

  err = nvs_set_str(nvs_handle, "ssid", ssid);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to write SSID: %s", esp_err_to_name(err));
  }

  err = nvs_set_str(nvs_handle, "password", password);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to write password: %s", esp_err_to_name(err));
  }

  err = nvs_commit(nvs_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
  }

  nvs_close(nvs_handle);
}

void read_wifi_credentials(char *ssid, size_t ssid_len, char *password,
                           size_t password_len) {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle", esp_err_to_name(err));
    return;
  }

  err = nvs_get_str(nvs_handle, "ssid", ssid, &ssid_len);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGE(TAG, "Error reading SSID: %s", esp_err_to_name(err));
  }
  err = nvs_get_str(nvs_handle, "password", password, &password_len);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGE(TAG, "Error reading password: %s", esp_err_to_name(err));
  }

  nvs_close(nvs_handle);
}
