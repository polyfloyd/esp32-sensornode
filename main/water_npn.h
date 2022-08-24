#ifndef _WATER_NPN_H
#define _WATER_NPN_H

#include <esp_err.h>
#include <driver/uart.h>
#include <driver/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t water_npn_err_t;

#define WATER_NPN_ERR_BASE 0xf90000

typedef struct {
    gpio_num_t pin;
    volatile int counts;
} water_npn_t;

typedef struct {
    int total_liters;
} water_npn_measurements_t;

water_npn_err_t water_npn_init(water_npn_t *handle, gpio_num_t pin);
water_npn_err_t water_npn_measurements(water_npn_t *handle, water_npn_measurements_t *measurements);

#ifdef __cplusplus
}
#endif

#endif

