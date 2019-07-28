#ifndef _SGP30_H
#define _SGP30_H

#include <esp_err.h>
#include <driver/i2c.h>

typedef esp_err_t sgp30_err_t;

#define SGP30_ERR_BASE          0x580000
#define SGP30_MEASURE_TEST_FAIL (SGP30_ERR_BASE+1)
#define SGP30_CRC_FAIL          (SGP30_ERR_BASE+2)
#define SGP30_INITIALIZING      (SGP30_ERR_BASE+3)

typedef struct {
    i2c_port_t port;
    bool initializing;
} sgp30_t;

sgp30_err_t sgp30_init(sgp30_t *handle, i2c_port_t port);
sgp30_err_t sgp30_measure_air_quality(sgp30_t *handle, uint16_t *eco2_ppm, uint16_t *tvoc_ppb);

#endif
