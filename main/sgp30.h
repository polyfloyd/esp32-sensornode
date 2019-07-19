#ifndef _SGP30_H
#define _SGP30_H

#include <esp_err.h>

#define SGP30_I2C_PORT I2C_NUM_0

typedef esp_err_t sgp30_err_t;

#define SGP30_ERR_BASE          0x580000
#define SGP30_MEASURE_TEST_FAIL (SGP30_ERR_BASE+1)
#define SGP30_CRC_FAIL          (SGP30_ERR_BASE+2)

sgp30_err_t sgp30_measure_test();

#endif
