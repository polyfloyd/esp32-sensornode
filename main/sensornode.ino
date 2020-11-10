#include <driver/i2c.h>
#include <esp_event_loop.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_wifi.h>
#include <lwip/apps/sntp.h>
#include <nvs_flash.h>
#include <string.h>

#include "prometheus.h"
#include "prometheus_esp32.h"

prom_counter_t metric_errors;

#ifdef CONFIG_SENSOR_BME280
#include "bme280.h"
bme280_t bme280;
prom_gauge_t metric_pressure;
#ifndef CONFIG_SENSOR_DS18B20
prom_gauge_t metric_temperature;
#endif
prom_gauge_t metric_rel_humidity;
#endif

#ifdef CONFIG_SENSOR_DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>
static OneWire one_wire_bus(4);
static DallasTemperature ds18b20(&one_wire_bus);
prom_gauge_t metric_temperature;
#endif

#ifdef CONFIG_SENSOR_GEIGER
#include "geiger.h"
prom_counter_t metric_geiger;
#endif

#ifdef CONFIG_SENSOR_MHZ19
#include <MHZ19.h>
HardwareSerial mhz19_hwserial(1);
MHZ19 mhz19;
prom_gauge_t metric_co2;
#endif

#ifdef CONFIG_SENSOR_PMS7003
#include <PMS.h>
HardwareSerial pms7003_hwserial(2);
PMS pms7003(pms7003_hwserial);
prom_gauge_t metric_dust_mass;
#endif

#ifdef CONFIG_SENSOR_PZEM004T
#include "pzem004t.h"
pzem004t_t pzem004t;
prom_gauge_t metric_power_voltage;
prom_gauge_t metric_power_current;
prom_gauge_t metric_power_wattage;
prom_gauge_t metric_power_frequency;
#endif

#ifdef CONFIG_SENSOR_SGP30
#include "sgp30.h"
sgp30_t sgp30;
prom_gauge_t metric_tvoc;
#endif

void init_metrics() {
    init_metrics_esp32(prom_default_registry());

    prom_strings_t errors_strings = {
        .name_space = NULL,
        .subsystem = "sensors",
        .name      = "errors",
        .help      = "The total number of errors that occurred while performing measurements per sensor",
    };
    const char *errors_labels[] = { "sensor", "code" };
    prom_counter_init_vec(&metric_errors, errors_strings, errors_labels, 2);
    prom_register_counter(prom_default_registry(), &metric_errors);

#ifdef CONFIG_SENSOR_BME280
    prom_strings_t pressure_strings = {
        .name_space = NULL,
        .subsystem = "sensors",
        .name      = "pressure_hpa",
        .help      = "The barometric pressure in hPa",
    };
    prom_gauge_init(&metric_pressure, pressure_strings);
    prom_register_gauge(prom_default_registry(), &metric_pressure);
#ifndef CONFIG_SENSOR_DS18B20
    prom_strings_t temperature_strings = {
        .name_space = NULL,
        .subsystem = "sensors",
        .name      = "temperature_c",
        .help      = "The ambient temperature in degrees celsius",
    };
    prom_gauge_init(&metric_temperature, temperature_strings);
    prom_register_gauge(prom_default_registry(), &metric_temperature);
#endif
    prom_strings_t rel_humidity_strings = {
        .name_space = NULL,
        .subsystem = "sensors",
        .name      = "relative_humidity_pct",
        .help      = "The relative humidity precentage",
    };
    prom_gauge_init(&metric_rel_humidity, rel_humidity_strings);
    prom_register_gauge(prom_default_registry(), &metric_rel_humidity);
#endif
#ifdef CONFIG_SENSOR_DS18B20
    prom_strings_t temperature_strings = {
        .name_space = NULL,
        .subsystem = "sensors",
        .name      = "temperature_c",
        .help      = "The ambient temperature in degrees celsius",
    };
    prom_gauge_init(&metric_temperature, temperature_strings);
    prom_register_gauge(prom_default_registry(), &metric_temperature);
#endif
#ifdef CONFIG_SENSOR_GEIGER
    prom_strings_t geiger_strings = {
        .name_space = NULL,
        .subsystem = "sensors",
        .name      = "geiger_count",
        .help      = "The total number of ionizing particles as measured by the geiger counter",
    };
    prom_counter_init(&metric_geiger, geiger_strings);
    prom_register_counter(prom_default_registry(), &metric_geiger);
#endif
#ifdef CONFIG_SENSOR_MHZ19
    prom_strings_t co2_strings = {
        .name_space = NULL,
        .subsystem = "sensors",
        .name      = "co2_ppm",
        .help      = "The ambient concentration of CO2 in the air as parts per million",
    };
    prom_gauge_init(&metric_co2, co2_strings);
    prom_register_gauge(prom_default_registry(), &metric_co2);
#endif
#ifdef CONFIG_SENSOR_PMS7003
    prom_strings_t dust_mass_strings = {
        .name_space = NULL,
        .subsystem = "sensors",
        .name      = "dust_mass",
        .help      = "The ambient concentration of fine dust in the air as micrograms per cubic meter",
    };
    const char *dust_mass_labels[] = { "size" };
    prom_gauge_init_vec(&metric_dust_mass, dust_mass_strings, dust_mass_labels, 1);
    prom_register_gauge(prom_default_registry(), &metric_dust_mass);
#endif
#ifdef CONFIG_SENSOR_PZEM004T
    prom_strings_t power_voltage_strings = {
        .name_space = NULL,
        .subsystem = "sensors",
        .name      = "power_voltage",
        .help      = "The net voltage",
    };
    prom_gauge_init(&metric_power_voltage, power_voltage_strings);
    prom_register_gauge(prom_default_registry(), &metric_power_voltage);
    prom_strings_t power_current_strings = {
        .name_space = NULL,
        .subsystem = "sensors",
        .name      = "power_current_a",
        .help      = "The current in Amperes",
    };
    prom_gauge_init(&metric_power_current, power_current_strings);
    prom_register_gauge(prom_default_registry(), &metric_power_current);
    prom_strings_t power_wattage_strings = {
        .name_space = NULL,
        .subsystem = "sensors",
        .name      = "power_wattage",
        .help      = "The actual power usage in Watts",
    };
    prom_gauge_init(&metric_power_wattage, power_wattage_strings);
    prom_register_gauge(prom_default_registry(), &metric_power_wattage);
    prom_strings_t power_frequency_strings = {
        .name_space = NULL,
        .subsystem = "sensors",
        .name      = "power_frequency_hz",
        .help      = "The net frequency in Hertz",
    };
    prom_gauge_init(&metric_power_frequency, power_frequency_strings);
    prom_register_gauge(prom_default_registry(), &metric_power_frequency);
#endif
#ifdef CONFIG_SENSOR_SGP30
    prom_strings_t tvoc_strings = {
        .name_space = NULL,
        .subsystem = "sensors",
        .name      = "tvoc_ppb",
        .help      = "The ambient concentration of volatile organic compounds in the air as parts per billion",
    };
    prom_gauge_init(&metric_tvoc, tvoc_strings);
    prom_register_gauge(prom_default_registry(), &metric_tvoc);
#endif
}

void record_sensor_error(const char *sensor, esp_err_t code) {
    char code_str[12];
    snprintf(code_str, sizeof(code_str), "0x%x", code);
    prom_counter_inc(&metric_errors, sensor, code_str);
    Serial.printf("%s: error 0x%x\n", sensor, code);
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event) {
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            Serial.println("wifi: SYSTEM_EVENT_STA_START\n");
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            Serial.println("SYSTEM_EVENT_STA_GOT_IP");
            Serial.printf("wifi: Got IP: '%s'\n",
                    ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            Serial.println("wifi: SYSTEM_EVENT_STA_DISCONNECTED");
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void init_wifi() {
    tcpip_adapter_init();

    char hostname[32];
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(hostname, sizeof(hostname), "sensornode-%x%x", mac[4], mac[5]);
    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname);
    Serial.printf("Hostname: %s\n", hostname);

    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config;
    strncpy((char*)wifi_config.sta.ssid, CONFIG_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, CONFIG_WIFI_PASSWORD, sizeof(wifi_config.sta.password));
    Serial.printf("Setting WiFi configuration SSID %s...\n", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void setup() {
    Serial.begin(115200);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = ESP_OK;
    }
    ESP_ERROR_CHECK(err);

    init_metrics();
    init_wifi();

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    char ntp_pool[16] = "pool.ntp.org";
    sntp_setservername(0, ntp_pool);
    sntp_init();

    httpd_handle_t http_server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&http_server, &config));
    httpd_uri_t prom_export_uri = prom_http_uri(prom_default_registry());
    ESP_ERROR_CHECK(httpd_register_uri_handler(http_server, &prom_export_uri));
    Serial.printf("Started server on port %d\n", config.server_port);

    i2c_config_t i2c_conf;
    i2c_conf.mode             = I2C_MODE_MASTER,
    i2c_conf.sda_io_num       = GPIO_NUM_23,
    i2c_conf.scl_io_num       = GPIO_NUM_13,
    i2c_conf.sda_pullup_en    = GPIO_PULLUP_ENABLE,
    i2c_conf.scl_pullup_en    = GPIO_PULLUP_ENABLE,
    i2c_conf.master.clk_speed = 100000;
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

#ifdef CONFIG_SENSOR_BME280
    ESP_ERROR_CHECK(bme280_init(&bme280, I2C_NUM_0))
    Serial.println("bme280: inititalized");
#endif
#ifdef CONFIG_SENSOR_DS18B20
    ds18b20.begin();
    ds18b20.setWaitForConversion(false);
#endif
#ifdef CONFIG_SENSOR_GEIGER
    void geiger_cb() {
        prom_counter_inc(&metric_geiger);
        Serial.println("geiger: tick");
    }
    geiger_t geiger;
    ESP_ERROR_CHECK(geiger_init(&geiger, GPIO_NUM_13, geiger_cb));
    Serial.println("geiger: inititalized");
#endif
#ifdef CONFIG_SENSOR_MHZ19
    mhz19_hwserial.begin(9600, SERIAL_8N1, 22, 21);
    mhz19.begin(mhz19_hwserial);
    mhz19.setFilter(true, true); // Filter out erroneous readings (set to 0)
    mhz19.autoCalibration();
    Serial.println("mhz19: inititalized");
#endif
#ifdef CONFIG_SENSOR_PMS7003
    pms7003_hwserial.begin(9600, SERIAL_8N1, 25, 32);
    pms7003.passiveMode();
    Serial.println("pms7003: inititalized");
#endif
#ifdef CONFIG_SENSOR_PZEM004T
    ESP_ERROR_CHECK(pzem004t_init(&pzem004t, UART_NUM_2, GPIO_NUM_17, GPIO_NUM_16));
    Serial.println("pzem004t: inititalized");
#endif
#ifdef CONFIG_SENSOR_SGP30
    ESP_ERROR_CHECK(sgp30_init(&sgp30, I2C_NUM_0))
    Serial.println("sgp30: inititalized");
#endif
}

void loop() {
    esp_err_t err = 0;
#ifdef CONFIG_SENSOR_BME280
    bme280_measuremnt_t bme280_measurement;
    if ((err = bme280_measure(&bme280, &bme280_measurement))) {
        record_sensor_error("BME280", err);
    } else {
        prom_gauge_set(&metric_pressure, bme280_measurement.pressure_pa / 100.0);
#ifndef CONFIG_SENSOR_DS18B20
        prom_gauge_set(&metric_temperature, bme280_measurement.temperature_c);
#endif
        prom_gauge_set(&metric_rel_humidity, bme280_measurement.humidity * 100.0);
        Serial.printf("bme280: measurement: pressure=%.2fhPa, temperature=%.2f°C, rel. humidity=%.2f%%\n",
            bme280_measurement.pressure_pa / 100.0,
            bme280_measurement.temperature_c,
            bme280_measurement.humidity * 100.0);
    }
#endif
#ifdef CONFIG_SENSOR_DS18B20
    ds18b20.requestTemperatures();
    unsigned int timeout = millis() + 500;
    while (millis() < timeout && !ds18b20.isConversionComplete());
    for (int i = 0; i < 100 /* arbitrary maximum */; i++) {
        float celsius = ds18b20.getTempCByIndex(i);
        if (celsius == DEVICE_DISCONNECTED_C) break;
        if (celsius == 85) { // sensor returns 85.0 on errors.
            record_sensor_error("DS18B20", 85);
            break;
        } else {
            prom_gauge_set(&metric_temperature, celsius);
            Serial.printf("ds18b20: measurement: temperature=%.2f°C\n", celsius);
            break;
        }
    }
#endif
#ifdef CONFIG_SENSOR_MHZ19
    int co2 = mhz19.getCO2();
    if (co2) {
        prom_gauge_set(&metric_co2, co2);
        Serial.printf("mhz19: measurement: co2=%d\n", co2);
    } else {
        record_sensor_error("MH-Z19", 1);
    }
#endif
#ifdef CONFIG_SENSOR_PMS7003
    pms7003.requestRead();
    PMS::DATA data;
    if (pms7003.readUntil(data)) {
        prom_gauge_set(&metric_dust_mass, data.PM_AE_UG_1_0, "PM1.0");
        prom_gauge_set(&metric_dust_mass, data.PM_AE_UG_2_5, "PM2.5");
        prom_gauge_set(&metric_dust_mass, data.PM_AE_UG_10_0, "PM10.0");
        Serial.printf("pms7003: measurement: PM1.0=%dμg/m³, PM2.5=%dμg/m³, PM10=%dμg/m³\n",
            data.PM_AE_UG_1_0, data.PM_AE_UG_2_5, data.PM_AE_UG_10_0);
    } else {
        record_sensor_error("PMS7003", 1);
    }
#endif
#ifdef CONFIG_SENSOR_PZEM004T
    pzem004t_measurements_t power_info;
    if ((err = pzem004t_measurements(&pzem004t, &power_info))) {
        record_sensor_error("PZEM-004T", err);
    } else {
        prom_gauge_set(&metric_power_voltage, power_info.voltage);
        prom_gauge_set(&metric_power_current, power_info.current_a);
        prom_gauge_set(&metric_power_wattage, power_info.power_w);
        prom_gauge_set(&metric_power_frequency, power_info.frequency_hz);
        Serial.printf("pzem-004t: measurement: voltage=%.1fV, current=%.3fA, power=%.1fW, frequency=%.1fHz\n",
            power_info.voltage, power_info.current_a, power_info.power_w, power_info.frequency_hz);
    }
#endif
#ifdef CONFIG_SENSOR_SGP30
    uint16_t eco2, tvoc;
    esp_err_t err = ESP_OK;
    if ((err = sgp30_measure_air_quality(&sgp30, &eco2, &tvoc))) {
        if (err == SGP30_INITIALIZING) {
            Serial.printf("sgp30: still initializing\n");
        } else {
            record_sensor_error("SGP30", err);
        }
    } else {
        prom_gauge_set(&metric_tvoc, tvoc);
        Serial.printf("sgp30: measurement: eco2=%d, tvoc=%d\n", eco2, tvoc);
    }
#endif
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
