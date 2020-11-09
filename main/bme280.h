#ifndef _BME280_H
#define _BME280_H

#include <esp_err.h>
#include <driver/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t bme280_err_t;

#define BME280_ERR_BASE 0x760000
#define BME280_ERR_BAD_CHIP_ID (BME280_ERR_BASE+1)

typedef struct {
    i2c_port_t port;
    uint16_t dig_t1;
    int16_t  dig_t2;
    int16_t  dig_t3;
    uint16_t dig_p1;
    int16_t  dig_p2;
    int16_t  dig_p3;
    int16_t  dig_p4;
    int16_t  dig_p5;
    int16_t  dig_p6;
    int16_t  dig_p7;
    int16_t  dig_p8;
    int16_t  dig_p9;
    uint8_t  dig_h1;
    int16_t  dig_h2;
    uint8_t  dig_h3;
    int16_t  dig_h4;
    int16_t  dig_h5;
    int8_t   dig_h6;
} bme280_t;

typedef struct {
    float pressure_pa;
    float temperature_c;
    float humidity;
} bme280_measuremnt_t;

bme280_err_t bme280_init(bme280_t *handle, i2c_port_t port);
bme280_err_t bme280_measure(bme280_t *handle, bme280_measuremnt_t *measurement);

#ifdef __cplusplus
}
#endif

#endif

