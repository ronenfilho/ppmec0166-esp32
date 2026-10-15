#ifndef CONNECT_H
#define CONNECT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

void wifi_init(void);

esp_err_t wifi_connect_sta(const char *ssid,
                           const char *pass,
                           int timeout);

void wifi_connect_ap(const char *ssid,
                     const char *pass);

void wifi_disconnect(void);

#ifdef __cplusplus
}
#endif

#endif /* CONNECT_H */
