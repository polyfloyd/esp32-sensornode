#ifndef _GEIGER_H
#define _GEIGER_H

#include <driver/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*geiger_callback_fn)();

typedef struct {
    geiger_callback_fn callback;
} geiger_t;

esp_err_t geiger_init(geiger_t *handle, gpio_num_t int_pin, geiger_callback_fn callback);

#ifdef __cplusplus
}
#endif

#endif
