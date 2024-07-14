#include "esp_log.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include <stdio.h>
#include <sys/_timeval.h>

char *TAG = "LOGGING";

static struct sockaddr_in server_addr;
int udp_socket;

void udp_socket_init() {
  udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_socket <= 0) {
    ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(CONFIG_HOME_SERVER_UDP_PORT);
  inet_pton(AF_INET, CONFIG_HOME_SERVER_IP, &server_addr.sin_addr.s_addr);

  ESP_LOGI(TAG, "UDP socket initialized.");
}

const char *log_level_to_str(esp_log_level_t level) {
  switch (level) {
  case ESP_LOG_ERROR:
    return "ERROR";
  case ESP_LOG_WARN:
    return "WARN";
  case ESP_LOG_INFO:
    return "INFO";
  case ESP_LOG_DEBUG:
    return "DEBUG";
  case ESP_LOG_VERBOSE:
    return "VERBOSE";
  default:
    return "";
  }
}

static void udp_log_handler(const char *message) {
  sendto(udp_socket, message, strlen(message), 0,
         (struct sockaddr *)&server_addr, sizeof(server_addr));
}

int custom_log_handler(const char *format, va_list args) {
  vprintf(format, args);

  char log_buffer[256];
  int len = vsnprintf(log_buffer, sizeof(log_buffer), format, args);

  if (len > 0) {
    char message[300];
    snprintf(message, sizeof(message), "2;%s", log_buffer);
    udp_log_handler(message);
  }

  return 0;
}
