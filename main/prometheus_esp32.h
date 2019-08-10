#ifndef _PROMETHEUS_ESP32_H_
#define _PROMETHEUS_ESP32_H_

#include "prometheus.h"
#include <esp_http_server.h>

void init_metrics_esp32(prom_registry_t *registry);
httpd_uri_t prom_http_uri(prom_registry_t *registry);

#endif
