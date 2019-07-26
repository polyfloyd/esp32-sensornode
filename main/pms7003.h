#ifndef _PMS7003_H
#define _PMS7003_H

#include <esp_err.h>
#include <driver/uart.h>

typedef esp_err_t pms7003_err_t;

#define PMS7003_ERR_BASE       0x420000
#define PMS7003_ERR_UART_R     (PMS7003_ERR_BASE+2)
#define PMS7003_ERR_READ_ERROR (PMS7003_ERR_BASE+3)
#define PMS7003_ERR_CHECKSUM   (PMS7003_ERR_BASE+4)

typedef struct {
    uart_port_t port;
} pms7003_t;

typedef struct {
    uint16_t pm10;  // PM1.0 in μg/m³
    uint16_t pm25;  // PM2.5 in μg/m³
    uint16_t pm100; // PM10 in μg/m³
} pms7003_measurement_t;

pms7003_err_t pms7003_init(pms7003_t *handle, uart_port_t port, gpio_num_t tx_pin, gpio_num_t rx_pin);
pms7003_err_t pms7003_measure(pms7003_t *handle, pms7003_measurement_t *measurement);

#endif
