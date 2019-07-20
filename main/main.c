#include <driver/i2c.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <string.h>

#include "sgp30.h"

void app_main(void) {
    nvs_flash_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    const i2c_config_t i2c_conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = GPIO_NUM_21,
        .scl_io_num       = GPIO_NUM_22,
        .sda_pullup_en    = GPIO_PULLUP_DISABLE,
        .scl_pullup_en    = GPIO_PULLUP_DISABLE,
        .master.clk_speed = 100000,
    };
    ESP_ERROR_CHECK(i2c_param_config(SGP30_I2C_PORT, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(SGP30_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));

    ESP_ERROR_CHECK(sgp30_measure_test());
    ESP_ERROR_CHECK(sgp30_init_air_quality());
    ESP_LOGI("main", "sgp30 inititalized");

    while (true) {
        uint16_t eco2, tvoc;
        sgp30_measure_air_quality(&eco2, &tvoc);
        ESP_LOGI("main", "sgp30 measurement: eco2=%d, tvoc=%d", eco2, tvoc);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
