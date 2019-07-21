#ifndef _GEIGER_H
#define _GEIGER_H

#include <driver/gpio.h>

void geiger_init(gpio_num_t int_pin, void (*callback)());

#endif
