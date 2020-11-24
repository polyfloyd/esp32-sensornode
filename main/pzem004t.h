#ifndef _PZEM004T_H
#define _PZEM004T_H

#include <esp_err.h>
#include <driver/uart.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t pzem004t_err_t;

#define PZEM004T_ERR_BASE       0xf80000
#define PZEM004T_ERR_UART_W     (PZEM004T_ERR_BASE+1)
#define PZEM004T_ERR_UART_R     (PZEM004T_ERR_BASE+2)
#define PZEM004T_ERR_READ_ERROR (PZEM004T_ERR_BASE+3)
#define PZEM004T_ERR_CHECKSUM   (PZEM004T_ERR_BASE+4)

typedef struct {
    uart_port_t port;
} pzem004t_t;

typedef struct {
    float voltage;
    float current_a;
    float power_w;
    float frequency_hz;
} pzem004t_measurements_t;

pzem004t_err_t pzem004t_init(pzem004t_t *handle, uart_port_t port, gpio_num_t tx_pin, gpio_num_t rx_pin);
pzem004t_err_t pzem004t_measurements(pzem004t_t *handle, pzem004t_measurements_t *measurements);

#ifdef __cplusplus
}
#endif

#endif
