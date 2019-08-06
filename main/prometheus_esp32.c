#include "prometheus_esp32.h"
#include <esp_event_loop.h>
#include <esp_heap_caps.h>
#include "prometheus.h"


prom_gauge_t _metric_heap_info;
prom_gauge_t _metric_task_num;

void _esp32_metrics_task(void *pvParameters) {
    while (true) {
        multi_heap_info_t heap_info;
        heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);
        prom_gauge_set(&_metric_heap_info, heap_info.total_free_bytes, "free");
        prom_gauge_set(&_metric_heap_info, heap_info.total_allocated_bytes, "alloc");

        int num_tasks = uxTaskGetNumberOfTasks();
        prom_gauge_set(&_metric_task_num, num_tasks);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void init_metrics_esp32(prom_registry_t *registry) {
    prom_strings_t mem_strings = {
        .namespace = NULL,
        .subsystem = "esp32",
        .name      = "heap_bytes",
        .help      = "Usage of the system's heap in bytes",
    };
    const char *mem_labels[] = { "type" };
    prom_gauge_init_vec(&_metric_heap_info, mem_strings, mem_labels, 1);
    prom_register_gauge(registry, &_metric_heap_info);

    prom_strings_t task_num_strings = {
        .namespace = NULL,
        .subsystem = "esp32",
        .name      = "task_num",
        .help      = "The current number of tasks",
    };
    prom_gauge_init(&_metric_task_num, task_num_strings);
    prom_register_gauge(registry, &_metric_task_num);

    TaskHandle_t h = NULL;
    xTaskCreate(_esp32_metrics_task, "prometheus-system", 2048, NULL, tskIDLE_PRIORITY, &h);
    assert(h);
}
