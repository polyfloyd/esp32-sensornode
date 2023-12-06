#include <WiFi.h>
#include <SPIFFS.h>
#include <WiFiSettings.h>
#include <MQTT.h>
#include <cstdio>
#include <esp_event.h>
#include <Wire.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <string.h>
#include <list>
#include <esp_heap_caps.h>
#include <esp_wifi.h>

#include "HardwareSerial.h"
#include "led.h"

using namespace std;

#ifdef CONFIG_SENSOR_BME280
#include <Adafruit_BME280.h>
#endif

#ifdef CONFIG_SENSOR_DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

#ifdef CONFIG_SENSOR_MHZ19
#include <MHZ19.h>
int co2_warning_threshold = 0;
int co2_alarm_threshold = 0;
#endif

#ifdef CONFIG_SENSOR_PMS7003
#include <PMS.h>
#endif

#ifdef CONFIG_SENSOR_PZEM004T
#include "pzem004t.h"
#endif

#ifdef CONFIG_SENSOR_SGP30
#include "Adafruit_SGP30.h"
#endif

#ifdef CONFIG_SENSOR_BH1750
#include <BH1750.h>
#endif

#ifdef CONFIG_SENSOR_WATER_NPN
#include "water_npn.h"
#endif


typedef function<void(const char*, const char*, float)> ReadUpdateFn;

struct Sensor {
    const char *name;
    function<void()> init;
    function<void(ReadUpdateFn)> read;
};

list<Sensor> sensors;
MQTTClient mqtt;
String mqtt_topic_prefix;
String location;

void record_sensor_error(const char *sensor, esp_err_t code) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s 0x%x", sensor, code);
    mqtt.publish(mqtt_topic_prefix + location + "/error", buf, false, 0);
    Serial.printf("error: %s\n", buf);
}

void init_sensors() {
    sensors.push_back(Sensor{
        .name = "esp32",
        .init = [](){},
        .read = [](ReadUpdateFn update) {
            multi_heap_info_t heap_info;
            heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);
            update("sys/heap/free", "B", heap_info.total_free_bytes);
            update("sys/heap/alloc", "B", heap_info.total_allocated_bytes);

            update("sys/num_tasks", "", uxTaskGetNumberOfTasks());

            wifi_ap_record_t ap_rec;
            esp_err_t err = esp_wifi_sta_get_ap_info(&ap_rec);
            update("wifi/rssi", "dBm", ap_rec.rssi);
        },
    });

#ifdef CONFIG_SENSOR_BME280
    static Adafruit_BME280 bme280;
    sensors.push_back(Sensor{
        .name = "bme280",
        .init = [](){
            unsigned status = bme280.begin(0x76);
            ESP_ERROR_CHECK(!status);
        },
        .read = [](ReadUpdateFn update) {
            float humidity_pct = bme280.readHumidity();
            float pressure_hpa = bme280.readPressure() / 100.0f;
            float temperature_c = bme280.readTemperature();
            update("humidity", "%", humidity_pct);
            update("pressure", "hPa", pressure_hpa);
#ifndef CONFIG_SENSOR_DS18B20
            update("temperature", "°C", temperature_c);
#endif
        },
    });
#endif

#ifdef CONFIG_SENSOR_DS18B20
    static OneWire one_wire_bus(4);
    static DallasTemperature ds18b20(&one_wire_bus);
    sensors.push_back(Sensor{
        .name = "ds18b20",
        .init = [](){},
        .read = [](ReadUpdateFn update) {
            // There's something wrong with the DS18B20 driver which causes the first
            // measurement after initialization to be re-read. Re-initializing the
            // driver is used as workaround.
            ds18b20.begin();
            ds18b20.requestTemperatures();
            float temperature_c = ds18b20.getTempCByIndex(0);
            if (temperature_c == DEVICE_DISCONNECTED_C) {
                record_sensor_error("DS18B20", 0x03);
                return;
            } else if (temperature_c == 85) { // sensor returns 85.0 on errors.
                record_sensor_error("DS18B20", 85);
                return;
            }
            update("temperature", "°C", temperature_c);
        },
    });
#endif

#ifdef CONFIG_SENSOR_MHZ19
    static HardwareSerial mhz19_hwserial(1);
    static MHZ19 mhz19;
    sensors.push_back(Sensor {
        .name = "mhz19",
        .init = []() {
            mhz19_hwserial.begin(9600, SERIAL_8N1, 22, 21);
            mhz19.begin(mhz19_hwserial);
            mhz19.setFilter(true, true); // Filter out erroneous readings (set to 0)
            mhz19.autoCalibration();
        },
        .read = [](ReadUpdateFn update) {
            int co2 = mhz19.getCO2();
            if (!co2) {
                record_sensor_error("MH-Z19", 1);
                return;
            }
            update("co2", "ppm", co2);
            set_led_state(Warning, co2_warning_threshold > 0 && co2 > co2_warning_threshold);
            set_led_state(Alarm, co2_alarm_threshold > 0 && co2 > co2_alarm_threshold);
        },
    });
#endif

#ifdef CONFIG_SENSOR_PMS7003
    static HardwareSerial pms7003_hwserial(2);
    static PMS pms7003(pms7003_hwserial);
    sensors.push_back(Sensor {
        .name = "pms7003",
        .init = []() {
            pms7003_hwserial.begin(9600, SERIAL_8N1, 25, 32);
            pms7003.passiveMode();
        },
        .read = [](ReadUpdateFn update) {
            pms7003.requestRead();
            PMS::DATA data;
            if (!pms7003.readUntil(data)) {
                record_sensor_error("PMS7003", 1);
                return;
            }
            update("dust_mass/pm1.0", "μg/m³", data.PM_AE_UG_1_0);
            update("dust_mass/pm2.5", "μg/m³", data.PM_AE_UG_2_5);
            update("dust_mass/pm10.0", "μg/m³", data.PM_AE_UG_10_0);
        },
    });
#endif

#ifdef CONFIG_SENSOR_PZEM004T
    static pzem004t_t pzem004t;
    sensors.push_back(Sensor {
        .name = "pzem004t",
        .init = []() {
            ESP_ERROR_CHECK(pzem004t_init(&pzem004t, UART_NUM_2, GPIO_NUM_17, GPIO_NUM_16));
        },
        .read = [](ReadUpdateFn update) {
            esp_err_t err = ESP_OK;
            pzem004t_measurements_t power_info;
            if ((err = pzem004t_measurements(&pzem004t, &power_info))) {
                record_sensor_error("PZEM-004T", err);
                return;
            }
            update("voltage", "V", power_info.voltage);
            update("current", "A", power_info.current_a);
            update("power", "W", power_info.power_w);
            update("frequency", "Hz", power_info.frequency_hz);
        },
    });
#endif

#ifdef CONFIG_SENSOR_SGP30
    static Adafruit_SGP30 sgp30;
    static bool sgp30_init = false;
    sensors.push_back(Sensor {
        .name = "sgp30",
        .init = []() {
            ESP_ERROR_CHECK(!sgp30.begin());
        },
        .read = [](ReadUpdateFn update) {
            if (!sgp30.IAQmeasure()) {
                record_sensor_error("SGP30", 1);
                return;
            }
            if (!sgp30_init && sgp30.eCO2 == 400 && sgp30.TVOC == 0) {
                return;
            }
            sgp30_init = true;
            update("organic_compounds", "ppb", sgp30.TVOC);
        },
    });
#endif

#ifdef CONFIG_SENSOR_BH1750
    static BH1750 bh1750;
    sensors.push_back(Sensor {
        .name = "bh1750",
        .init = []() {
            ESP_ERROR_CHECK(!bh1750.begin());
        },
        .read = [](ReadUpdateFn update) {
            if (!bh1750.measurementReady()) {
                return;
            }
            float lux = bh1750.readLightLevel();
            update("light_intensity", "Lux", lux);
        },
    });
#endif

#ifdef CONFIG_SENSOR_WATER_NPN
    static water_npn_t water_npn;
    static int prev_liters = 0;
    sensors.push_back(Sensor {
        .name = "water_npn",
        .init = []() {
            ESP_ERROR_CHECK(water_npn_init(&water_npn, GPIO_NUM_16));
        },
        .read = [](ReadUpdateFn update) {
            water_npn_measurements_t water_measurements;
            water_npn_measurements(&water_npn, &water_measurements);
            int inc = water_measurements.total_liters - prev_liters;
            prev_liters = water_measurements.total_liters;
            update("water_consumed_total", "L", water_measurements.total_liters);
            update("water_consumed", "L", inc);
        },
    });
#endif
}

#ifdef MENU_BUTTON_PIN
void check_portal_button() {
    if (digitalRead(MENU_BUTTON_PIN)) return;
    delay(50);
    if (digitalRead(MENU_BUTTON_PIN)) return;
    WiFiSettings.portal();
}
#else
// A null function is a bit hacky, but it avoids more ifdefs below.
void check_portal_button() {}
#endif

void setup() {
    Serial.begin(115200);
    SPIFFS.begin(true);
    Wire.begin(23 /* sda */, 13 /* scl */);
#ifdef MENU_BUTTON_PIN
    pinMode(MENU_BUTTON_PIN, INPUT);
#endif

    init_led_task();
    init_sensors();

    location = WiFiSettings.string("location", 64, "Room", "The name of the physical location this sensor node is located");
#ifdef CONFIG_SENSOR_MHZ19
    co2_warning_threshold = WiFiSettings.integer("co2_warning_threshold", 1000, "The CO2 concentration at which to enter the warning state (0 to disable)");
    co2_alarm_threshold = WiFiSettings.integer("co2_alarm_threshold", 1600, "The CO2 concentration at which to enter the alarm state (0 to disable)");
#endif
    WiFiSettings.heading("MQTT");
    mqtt_topic_prefix = WiFiSettings.string("mqtt_prefix", "sensors/", "Topic prefix");
    String mqtt_server = WiFiSettings.string("mqtt_server", 64, "test.mosquitto.org", "Broker");
    int mqtt_port = WiFiSettings.integer("mqtt_port", 0, 65535, 1883, "Broker TCP port");

    unsigned long portal_start = millis();
    WiFiSettings.onWaitLoop = []() {
        set_led_state(WiFiConnecting, 1);
        check_portal_button();
        return 50;
    };
    WiFiSettings.onPortalWaitLoop = [portal_start]() {
        set_led_state(WiFiPortal, 1);
        if (millis() - portal_start > 1000*60*5) {
            ESP.restart();
        }
    };
    WiFiSettings.onSuccess = []() {
        set_led_state(WiFiPortal, 0);
        set_led_state(WiFiConnecting, 0);
    };
    String hostname = String("sensornode-") + location + "-";
    hostname.toLowerCase();
    WiFiSettings.hostname = hostname;

    if (!WiFiSettings.connect()) ESP.restart();

    static WiFiClient wificlient;
    mqtt.begin(mqtt_server.c_str(), mqtt_port, wificlient);

    for (Sensor &sensor : sensors) {
        sensor.init();
        Serial.printf("%s: inititalized\n", sensor.name);
    }
}

void connect_mqtt() {
    if (mqtt.connected()) return;  // already/still connected

    static int failures = 0;
    if (mqtt.connect("")) {
        failures = 0;
        mqtt.publish(mqtt_topic_prefix + location + "/version", PROJECT_VERSION, true, 0);
    } else {
        failures++;
        if (failures >= 5) ESP.restart();
    }
}

void loop() {
    check_portal_button();

    connect_mqtt();
    mqtt.loop();

    for (Sensor &sensor : sensors) {
        sensor.read([sensor](const char *what, const char *unit, float value) {
            Serial.printf("%s: %s=%.2f %s\n", sensor.name, what, value, unit);
            char buf[64] = { 0 };
            snprintf(buf, sizeof(buf), "%.2f %s", value, unit);
            mqtt.publish(mqtt_topic_prefix + location + "/" + what, buf, true, 0);
        });
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
