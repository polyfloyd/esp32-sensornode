ESP32 Sensornode
================

**NOTE: This app has been replaced by esphome and is thus unmaintained**

This is code for an ESP32 to export the data from a bunch of sensors over MQTT.

Supported peripherals:
* BH1750 (light intensity)
* BME280 (relative humidity, temperature, barometric pressure)
* MH-Z19 (CO2)
* PMS7003 (fine dust)
* PZEM-004T v3 (AC power)
* SGP30 (volatile organic gas)
* NPN Sensor based water consumption

Peripherals and configs are selected through platformio.ini.


![Grafana Dashboard Example](grafana.png)
