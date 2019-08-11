#include "geiger.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "util.h"

static xQueueHandle geiger_event_queue = NULL;

static void IRAM_ATTR geiger_isr_handler(void *arg) {
    geiger_t *handle = (geiger_t*)arg;
    xQueueSendFromISR(geiger_event_queue, &handle, NULL);
}

static void geiger_event_task(void *arg) {
    while (true) {
        geiger_t *handle;
        if (xQueueReceive(geiger_event_queue, &handle, portMAX_DELAY)) {
            handle->callback();
        }
    }
}

esp_err_t geiger_init(geiger_t *handle, gpio_num_t int_pin, geiger_callback_fn callback) {
    if (!geiger_event_queue) {
        geiger_event_queue = xQueueCreate(8, sizeof(geiger_t*));
        xTaskCreate(geiger_event_task, "geiger_event_task", 2048, NULL, 10, NULL);
    }

    handle->callback = callback;

    gpio_config_t io_conf = {
        .pin_bit_mask = 1<<int_pin,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_POSEDGE,
    };
    TRY(gpio_config(&io_conf));
    TRY(gpio_install_isr_service(0));
    TRY(gpio_isr_handler_add(int_pin, geiger_isr_handler, (void*)handle));

    return ESP_OK;
}
