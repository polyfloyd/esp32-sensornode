#include "prometheus_esp32.h"
#include <esp_event_loop.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <stdint.h>
#include <string.h>
#include "prometheus.h"

const uint64_t MAX_CRAWL_INTERVAL = 10 * 60;
const char *NVS_REBOOT_COUNT_KEY = "reboot_count";

prom_gauge_t metric_wifi_rssi;

volatile time_t most_recent_scrape = 0;

size_t firmware_version_num_metrics(void *ctx) {
    return 1;
}

void firmware_version_collect(void *ctx, prom_metric_t *metrics, size_t num_metrics) {
    assert(num_metrics == 1);
    prom_metric_t *m_free = &metrics[0];
    m_free->value = 1;
    m_free->timestamp = prom_timestamp();
    const char *v = PROJECT_VERSION;
    strncpy(m_free->label_values, v, PROM_MAX_LABEL_VALUES_LENGTH);
}

size_t mem_num_metrics(void *ctx) {
    return 2;
}

void mem_collect(void *ctx, prom_metric_t *metrics, size_t num_metrics) {
    assert(num_metrics == 2);

    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);

    prom_metric_t *m_free = &metrics[0];
    m_free->value = heap_info.total_free_bytes;
    m_free->timestamp = prom_timestamp();
    strncpy(m_free->label_values, "free", PROM_MAX_LABEL_VALUES_LENGTH);

    prom_metric_t *m_alloc = &metrics[1];
    m_alloc->value = heap_info.total_allocated_bytes;
    m_alloc->timestamp = prom_timestamp();
    strncpy(m_alloc->label_values, "alloc", PROM_MAX_LABEL_VALUES_LENGTH);
}

size_t num_tasks_num_metrics(void *ctx) {
    return 1;
}

void num_tasks_collect(void *ctx, prom_metric_t *metrics, size_t num_metrics) {
    assert(num_metrics == 1);
    prom_metric_t *m = &metrics[0];
    m->value = uxTaskGetNumberOfTasks();
    m->timestamp = prom_timestamp();
    m->label_values[0] = 0;
}

size_t reboot_count_num_metrics(void *ctx) {
    return 1;
}

void reboot_count_collect(void *ctx, prom_metric_t *metrics, size_t num_metrics) {
    assert(num_metrics == 1);

    nvs_handle nvs_handle;
    ESP_ERROR_CHECK(nvs_open("prometheus_util", NVS_READONLY, &nvs_handle));
    uint64_t count;
    ESP_ERROR_CHECK(nvs_get_u64(nvs_handle, NVS_REBOOT_COUNT_KEY, &count));
    nvs_close(nvs_handle);

    prom_metric_t *m = &metrics[0];
    m->value = count;
    m->timestamp = prom_timestamp();
    m->label_values[0] = 0;
}

void esp32_metrics_task(void *pvParameters) {
    while (true) {
        wifi_ap_record_t ap_rec;
        esp_err_t err = esp_wifi_sta_get_ap_info(&ap_rec);
        if (err) {
            continue;
        }
        prom_gauge_set(&metric_wifi_rssi, ap_rec.rssi, ap_rec.ssid);

        // Watchdog to reboot if no crawling takes place for some too long time.
        time_t now = time(NULL);
        // Compare the time difference. If the difference is greater than a
        // year, assume that the clock jumped forward due to NTP
        // synchronisation and reset the counter.
        uint64_t tdiff = now - most_recent_scrape;
        if (tdiff > 365*24*3600) {
            most_recent_scrape = time(NULL);
        } else if (tdiff > MAX_CRAWL_INTERVAL) {
            esp_restart();
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void inc_reboot_count() {
    nvs_handle nvs_handle;
    ESP_ERROR_CHECK(nvs_open("prometheus_util", NVS_READWRITE, &nvs_handle));
    uint64_t count;
    esp_err_t err = nvs_get_u64(nvs_handle, NVS_REBOOT_COUNT_KEY, &count);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        count = 0, err = ESP_OK;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(nvs_set_u64(nvs_handle, NVS_REBOOT_COUNT_KEY, count + 1));
    nvs_close(nvs_handle);
}

void init_metrics_esp32(prom_registry_t *registry) {
    inc_reboot_count();

    static prom_strings_t firmware_version_strings = {
        .name_space = NULL,
        .subsystem  = "esp32",
        .name       = "firmware_version",
        .help       = "The current firmware revision",
    };
    static const char *firmware_version_labels[] = { "version" };
    prom_collector_t firmware_version_col = {
        .strings     = &firmware_version_strings,
        .num_labels  = 1,
        .labels      = firmware_version_labels,
        .type        = PROM_TYPE_GAUGE,
        .ctx         = NULL,
        .num_metrics = firmware_version_num_metrics,
        .collect     = firmware_version_collect,
    };
    prom_register(registry, firmware_version_col);

    static prom_strings_t reboot_count_strings = {
        .name_space = NULL,
        .subsystem  = "esp32",
        .name       = "reboot_count",
        .help       = "The total number of reboots",
    };
    prom_collector_t reboot_count_col = {
        .strings     = &reboot_count_strings,
        .num_labels  = 0,
        .labels      = NULL,
        .type        = PROM_TYPE_COUNTER,
        .ctx         = NULL,
        .num_metrics = reboot_count_num_metrics,
        .collect     = reboot_count_collect,
    };
    prom_register(registry, reboot_count_col);

    static prom_strings_t mem_strings = {
        .name_space = NULL,
        .subsystem  = "esp32",
        .name       = "heap_bytes",
        .help       = "Usage of the system's heap in bytes",
    };
    static const char *mem_labels[] = { "type" };
    prom_collector_t mem_col = {
        .strings     = &mem_strings,
        .num_labels  = 1,
        .labels      = mem_labels,
        .type        = PROM_TYPE_GAUGE,
        .ctx         = NULL,
        .num_metrics = mem_num_metrics,
        .collect     = mem_collect,
    };
    prom_register(registry, mem_col);

    static prom_strings_t num_tasks_strings = {
        .name_space = NULL,
        .subsystem  = "esp32",
        .name       = "task_num",
        .help       = "The current number of tasks",
    };
    prom_collector_t num_tasks_col = {
        .strings     = &num_tasks_strings,
        .num_labels  = 0,
        .labels      = NULL,
        .type        = PROM_TYPE_GAUGE,
        .ctx         = NULL,
        .num_metrics = num_tasks_num_metrics,
        .collect     = num_tasks_collect,
    };
    prom_register(registry, num_tasks_col);

    prom_strings_t wifi_rssi_strings = {
        .name_space = NULL,
        .subsystem  = "esp32",
        .name       = "wifi_rssi",
        .help       = "The signal strength of the AP the ESP is connected to",
    };
    static const char *wifi_rssi_labels[] = { "ssid" };
    prom_gauge_init_vec(&metric_wifi_rssi, wifi_rssi_strings, wifi_rssi_labels, 1);
    prom_register_gauge(registry, &metric_wifi_rssi);

    TaskHandle_t h = NULL;
    xTaskCreate(esp32_metrics_task, "prometheus-esp32", 2048, NULL, tskIDLE_PRIORITY, &h);
    assert(h);
}

esp_err_t export_http_handler(httpd_req_t *req) {
    // We just write to the file descriptor directly because:
    // * The HTTP library does not support writing to a FILE.
    // * Writing to an fmemopen buffer somehow truncates the output to 1024 bytes.
    FILE *w = fdopen(httpd_req_to_sockfd(req), "w");
    fprintf(w, "HTTP/1.1 200 OK\n");
    fprintf(w, "Content-Type: text/plain; version=0.0.4\n\n");

    prom_registry_export(req->user_ctx, w);

    httpd_sess_trigger_close(req->handle, fileno(w));
    fclose(w);

    most_recent_scrape = time(NULL);
    return ESP_OK;
}

httpd_uri_t prom_http_uri(prom_registry_t *registry) {
    httpd_uri_t uri =  {
        .uri       = "/metrics",
        .method    = HTTP_GET,
        .handler   = export_http_handler,
        .user_ctx  = registry,
    };
    return uri;
}
