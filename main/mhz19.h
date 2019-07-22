#ifndef _MHZ19_H
#define _MHZ19_H

#include <esp_err.h>
#include <driver/uart.h>

typedef esp_err_t mhz19_err_t;

#define MHZ19_ERR_BASE     0x860000
#define MHZ19_ERR_UART_W   (MHZ19_ERR_BASE+1)
#define MHZ19_ERR_UART_R   (MHZ19_ERR_BASE+2)
#define MHZ19_ERR_CHECKSUM (MHZ19_ERR_BASE+3)

typedef struct {
    uart_port_t port;
} mhz19_t;

mhz19_err_t mhz19_init(mhz19_t *handle, uart_port_t uart_num, gpio_num_t tx_pin, gpio_num_t rx_pin);
mhz19_err_t mhz19_gas_concentration(mhz19_t *handle, uint16_t *co2);

#endif
