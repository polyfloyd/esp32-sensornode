#include "prometheus_esp32.h"
#include <esp_event_loop.h>
#include <esp_heap_caps.h>
#include <string.h>
#include "prometheus.h"


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

void init_metrics_esp32(prom_registry_t *registry) {
    static prom_strings_t mem_strings = {
        .namespace = NULL,
        .subsystem = "esp32",
        .name      = "heap_bytes",
        .help      = "Usage of the system's heap in bytes",
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
        .namespace = NULL,
        .subsystem = "esp32",
        .name      = "task_num",
        .help      = "The current number of tasks",
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
