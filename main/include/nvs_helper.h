#ifndef NVS_HELPER_H
#define NVS_HELPER_H

#include "nvs.h"

void store_wifi_credentials(const char *ssid, const char *password);

void read_wifi_credentials(char *ssid, size_t ssid_len, char *password,
                           size_t password_len);

#endif // NVS_HELPER_H
