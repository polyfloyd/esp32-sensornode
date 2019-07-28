#include "pms7003.h"
#include "util.h"

#define MAX_ATTEMPTS 8

pms7003_err_t pms7003_init(pms7003_t *handle, uart_port_t port, gpio_num_t tx_pin, gpio_num_t rx_pin) {
    uart_config_t uart_config = {
        .baud_rate           = 9600,
        .data_bits           = UART_DATA_8_BITS,
        .parity              = UART_PARITY_DISABLE,
        .stop_bits           = UART_STOP_BITS_1,
        .flow_ctrl           = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };
    TRY(uart_param_config(port, &uart_config));
    TRY(uart_set_pin(port, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    TRY(uart_driver_install(port, 1024, 0, 0, NULL, 0));

    handle->port = port;
    return ESP_OK;
}

pms7003_err_t pms7003_measure(pms7003_t *handle, pms7003_measurement_t *measurement) {
    uint8_t resp_bytes[32] = { 0 };
    int attempt = 0;
    for (; attempt < MAX_ATTEMPTS; attempt++) {
        int r = uart_read_bytes(handle->port, resp_bytes, sizeof(resp_bytes), 150 / portTICK_RATE_MS);
        if (r < 0) {
            return PMS7003_ERR_UART_R;
        }
        if (r != 32 || resp_bytes[0] != 0x42 || resp_bytes[1] != 0x4d) {
            continue;
        }
        break;
    }
    if (attempt == MAX_ATTEMPTS) {
        return PMS7003_ERR_READ_ERROR;
    }

    uint16_t checksum = 0;
    for (int i = 0; i < 30; i++) {
        checksum += resp_bytes[i];
    }
    if (checksum != (resp_bytes[30]<<8 | resp_bytes[31])) {
        return PMS7003_ERR_CHECKSUM;
    }

    uint8_t errorcode = resp_bytes[29];
    if (errorcode) {
        return PMS7003_ERR_BASE | errorcode<<8;
    }

    // Data 0: Start marker, 0x424d
    // Data 1 indicates PM1.0 concentration (CF = 1, standard particles), Unit μ g / m3
    // Data 2 indicates PM2.5 concentration (CF = 1, standard particulate matter), Unit μ g / m3
    // Data 3 indicates PM10 concentration (CF = 1, standard particulate matter), Unit μ g / m3
    // Data 4 indicates PM1.0 concentration (in atmospheric environment), Unit μ g / m3
    // Data 5 indicates PM2.5 concentration (in atmospheric environment), Unit μ g / m3
    // Data 6 indicates PM10 concentration (in atmospheric environment), Unit μ g / m3
    // Data 7 indicates that 0.1 liter of air has a diameter above 0.3um, The number of particles
    // Data 8 indicates that 0.1 liter of air has a diameter of 0.5um or more, The number of particles
    // Data 9 indicates that 0.1 liter of air has a diameter of 1.0um or more, The number of particles
    // Data 10 indicates that the diameter of 0.1 liter of air is above 2.5um, The number of particles
    // Data 11 indicates that 0.1 liter of air has a diameter of 5.0um or more, The number of particlesPage 12
    // Data 12 indicates that 0.1 liter of air has a diameter above 10um, The number of particles
    // Data 13 high octet: version number
    // Data 13 low octets: error code
    // Data and check high eight: Check code = start character 1 + start character 2 + ... .. + data 13 low
    // Data and check low eight: Eight
    measurement->pm10 = resp_bytes[2*4]<<8 | resp_bytes[2*4+1];
    measurement->pm25 = resp_bytes[2*5]<<8 | resp_bytes[2*5+1];
    measurement->pm100 = resp_bytes[2*6]<<8 | resp_bytes[2*6+1];

    return ESP_OK;
}
