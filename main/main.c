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
#include "prometheus.h"

prom_counter_t metric_geiger;
prom_gauge_t   metric_co2;
prom_gauge_t   metric_dust_mass;
prom_gauge_t   metric_tvoc;
prom_gauge_t   metric_pressure;
prom_gauge_t   metric_temperature;
prom_gauge_t   metric_rel_humidity;

void init_metrics() {
    prom_strings_t geiger_strings = {
        .namespace = NULL,
        .subsystem = "sensors",
        .name      = "geiger_count",
        .help      = "The total number of ionizing particles as measured by the geiger counter",
    };
    prom_counter_init(&metric_geiger, geiger_strings);
    prom_register_counter(prom_default_registry(), &metric_geiger);

    prom_strings_t co2_strings = {
        .namespace = NULL,
        .subsystem = "sensors",
        .name      = "co2_ppm",
        .help      = "The ambient concentration of CO2 in the air as parts per million",
    };
    prom_gauge_init(&metric_co2, co2_strings);
    prom_register_gauge(prom_default_registry(), &metric_co2);

    prom_strings_t dust_mass_strings = {
        .namespace = NULL,
        .subsystem = "sensors",
        .name      = "dust_mass",
        .help      = "The ambient concentration of fine dust in the air as micrograms per cubic meter",
    };
    const char *dust_mass_labels[] = { "size" };
    prom_gauge_init_vec(&metric_dust_mass, dust_mass_strings, dust_mass_labels, 1);
    prom_register_gauge(prom_default_registry(), &metric_dust_mass);

    prom_strings_t tvoc_strings = {
        .namespace = NULL,
        .subsystem = "sensors",
        .name      = "tvoc_ppb",
        .help      = "The ambient concentration of volatile organic compounds in the air as parts per billion",
    };
    prom_gauge_init(&metric_tvoc, tvoc_strings);
    prom_register_gauge(prom_default_registry(), &metric_tvoc);

    prom_strings_t pressure_strings = {
        .namespace = NULL,
        .subsystem = "sensors",
        .name      = "pressure_hpa",
        .help      = "The barometric pressure in hPa",
    };
    prom_gauge_init(&metric_pressure, pressure_strings);
    prom_register_gauge(prom_default_registry(), &metric_pressure);

    prom_strings_t temperature_strings = {
        .namespace = NULL,
        .subsystem = "sensors",
        .name      = "temperature_c",
        .help      = "The ambient temperature in degrees celsius",
    };
    prom_gauge_init(&metric_temperature, temperature_strings);
    prom_register_gauge(prom_default_registry(), &metric_temperature);

    prom_strings_t rel_humidity_strings = {
        .namespace = NULL,
        .subsystem = "sensors",
        .name      = "relative_humidity_pct",
        .help      = "The relative humidity precentage",
    };
    prom_gauge_init(&metric_rel_humidity, rel_humidity_strings);
    prom_register_gauge(prom_default_registry(), &metric_rel_humidity);
}


void geiger_cb() {
    prom_counter_inc(&metric_geiger);
    ESP_LOGI("geiger", "tick");
}

void app_main(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = ESP_OK;
    }
    ESP_ERROR_CHECK(err);

    init_metrics();

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
            prom_gauge_set(&metric_tvoc, tvoc);
            ESP_LOGI("main", "sgp30 measurement: eco2=%d, tvoc=%d", eco2, tvoc);
        }

        bme280_measuremnt_t bme280_measurement;
        if ((err = bme280_measure(&bme280, &bme280_measurement))) {
            ESP_LOGE("main", "bme280 err: 0x%x", err);
        } else {
            prom_gauge_set(&metric_pressure, bme280_measurement.pressure_pa / 100.0);
            prom_gauge_set(&metric_temperature, bme280_measurement.temperature_c);
            prom_gauge_set(&metric_rel_humidity, bme280_measurement.humidity * 100.0);
            ESP_LOGI("main", "bme280 measurement: pressure=%.2fhPa, temperature=%.2f°C, rel. humidity=%.2f%%",
                bme280_measurement.pressure_pa / 100.0,
                bme280_measurement.temperature_c,
                bme280_measurement.humidity * 100.0);
        }

        uint16_t co2;
        if ((err = mhz19_gas_concentration(&mhz19, &co2))) {
            ESP_LOGE("main", "mhz19 err: 0x%x", err);
        } else {
            prom_gauge_set(&metric_co2, co2);
            ESP_LOGI("main", "mhz19 measurement: co2=%d", co2);
        }

        pms7003_measurement_t dust_mass;
        if ((err = pms7003_measure(&pms7003, &dust_mass))) {
            ESP_LOGE("main", "pms7003 err: 0x%x", err);
        } else {
            prom_gauge_set(&metric_dust_mass, dust_mass.pm10, "PM1.0");
            prom_gauge_set(&metric_dust_mass, dust_mass.pm25, "PM2.5");
            prom_gauge_set(&metric_dust_mass, dust_mass.pm100, "PM10.0");
            ESP_LOGI("main", "pms7003 measurement: PM1.0=%dμg/m³, PM2.5=%dμg/m³, PM10=%dμg/m³",
                dust_mass.pm10, dust_mass.pm25, dust_mass.pm100);
        }

        char buf[2048] = { 0 };
        int len = prom_format(prom_default_registry(), buf, sizeof(buf));
        if (len < 0) {
            ESP_LOGE("main", "need at least %d bytes for prometheus format buffer", -len);
        } else {
            printf("%s", buf);
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
