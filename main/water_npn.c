#include "water_npn.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "util.h"

static xQueueHandle event_queue = NULL;

static void IRAM_ATTR isr_handler(void *arg) {
    water_npn_t *handle = (water_npn_t*)arg;
    xQueueSendFromISR(event_queue, &handle, NULL);
}

static void event_task(void *arg) {
    while (true) {
        water_npn_t *handle;
        if (xQueueReceive(event_queue, &handle, portMAX_DELAY)) {
            handle->counts++;
        }
    }
}

esp_err_t water_npn_init(water_npn_t *handle, gpio_num_t int_pin) {
    if (!event_queue) {
        event_queue = xQueueCreate(8, sizeof(water_npn_t*));
        xTaskCreate(event_task, "water_npn_event_task", 2048, NULL, 10, NULL);
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = 1<<int_pin,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_POSEDGE,
    };
    TRY(gpio_config(&io_conf));
    TRY(gpio_install_isr_service(0));
    TRY(gpio_isr_handler_add(int_pin, isr_handler, (void*)handle));

    return ESP_OK;
}

water_npn_err_t water_npn_measurements(water_npn_t *handle, water_npn_measurements_t *measurements) {
    // The interrupt triggers on both falling and rising edges and we count both events. So divide
    // by 2 to get the number of full cycles.
    measurements->total_liters = handle->counts >> 1;
}
