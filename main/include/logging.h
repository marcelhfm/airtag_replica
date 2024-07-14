#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>

void udp_socket_init();
int custom_log_handler(const char *format, va_list args);

#endif // LOGGING_H
