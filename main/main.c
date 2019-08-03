#include <driver/i2c.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <string.h>

#include "bme280.h"
#include "geiger.h"
#include "mhz19.h"
#include "pms7003.h"
#include "sgp30.h"

void geiger_cb() {
    ESP_LOGI("geiger", "tick");
}

void app_main(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = ESP_OK;
    }
    ESP_ERROR_CHECK(err);

    const i2c_config_t i2c_conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = GPIO_NUM_21,
        .scl_io_num       = GPIO_NUM_22,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

    bme280_t bme280;
    ESP_ERROR_CHECK(bme280_init(&bme280, I2C_NUM_0))
    ESP_LOGI("main", "bme280 inititalized");

    sgp30_t sgp30;
    ESP_ERROR_CHECK(sgp30_init(&sgp30, I2C_NUM_0))
    ESP_LOGI("main", "sgp30 inititalized");

    ESP_ERROR_CHECK(geiger_init(GPIO_NUM_13, geiger_cb));
    ESP_LOGI("main", "geiger inititalized");

    pms7003_t pms7003;
    ESP_ERROR_CHECK(pms7003_init(&pms7003, UART_NUM_1, GPIO_NUM_27, GPIO_NUM_26));
    ESP_LOGI("main", "pms7003 inititalized");

    mhz19_t mhz19;
    ESP_ERROR_CHECK(mhz19_init(&mhz19, UART_NUM_2, GPIO_NUM_17, GPIO_NUM_16));
    ESP_LOGI("main", "mhz19 inititalized");

    while (true) {
        uint16_t eco2, tvoc;
        esp_err_t err = ESP_OK;
        if ((err = sgp30_measure_air_quality(&sgp30, &eco2, &tvoc))) {
            ESP_LOGE("main", "sgp30 err: 0x%x", err);
        } else {
            ESP_LOGI("main", "sgp30 measurement: eco2=%d, tvoc=%d", eco2, tvoc);
        }

        bme280_measuremnt_t measurement;
        if ((err = bme280_measure(&bme280, &measurement))) {
            ESP_LOGE("main", "bme280 err: 0x%x", err);
        } else {
            ESP_LOGI("main", "bme280 measurement: pressure=%.2fhPa, temperature=%.2f°C, rel. humidity=%.2f%%",
                measurement.pressure_pa/100.0, measurement.temperature_c, measurement.humidity*100.0);
        }

        uint16_t co2;
        if ((err = mhz19_gas_concentration(&mhz19, &co2))) {
            ESP_LOGE("main", "mhz19 err: 0x%x", err);
        } else {
            ESP_LOGI("main", "mhz19 measurement: co2=%d", co2);
        }

        pms7003_measurement_t dust_mass;
        if ((err = pms7003_measure(&pms7003, &dust_mass))) {
            ESP_LOGE("main", "pms7003 err: 0x%x", err);
        } else {
            ESP_LOGI("main", "pms7003 measurement: PM1.0=%dμg/m³, PM2.5=%dμg/m³, PM10=%dμg/m³",
                dust_mass.pm10, dust_mass.pm25, dust_mass.pm100);
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
