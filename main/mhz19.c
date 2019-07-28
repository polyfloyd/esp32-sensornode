#include "mhz19.h"
#include <string.h>
#include "util.h"

mhz19_err_t mhz19_init(mhz19_t *handle, uart_port_t port, gpio_num_t tx_pin, gpio_num_t rx_pin) {
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

uint8_t mhz19_checksum(uint8_t *data) {
    uint8_t checksum = 0;
    for (int i = 1; i < 8; i++) {
        checksum += data[i];
    }
    return (0xff - checksum) + 1;
}

mhz19_err_t mhz19_cmd(mhz19_t *handle, uint8_t cmd, uint8_t *cmd_data, uint8_t *resp_data) {
    uint8_t req_bytes[9] = { 0 };
    req_bytes[0] = 0xff; // Fixed start byte.
    req_bytes[1] = 1;    // Sensor num.
    req_bytes[2] = cmd;
    if (cmd_data) {
        memcpy(req_bytes, &req_bytes[3], 5);
    }
    req_bytes[8] = mhz19_checksum(req_bytes);

    int w = uart_write_bytes(handle->port, (const char*)req_bytes, sizeof(req_bytes));
    if (w < 0) {
        return MHZ19_ERR_UART_W;
    }

    if (resp_data) {
        uint8_t resp_bytes[9] = { 0 };
        int r = uart_read_bytes(handle->port, resp_bytes, sizeof(resp_bytes), 100 / portTICK_RATE_MS);
        if (r < 0) {
            return MHZ19_ERR_UART_R;
        }
        memcpy(resp_data, &resp_bytes[2], 6);
        if (resp_bytes[8] != mhz19_checksum(resp_bytes)) {
            return MHZ19_ERR_CHECKSUM;
        }
    }

    return ESP_OK;
}

mhz19_err_t mhz19_gas_concentration(mhz19_t *handle, uint16_t *co2) {
    uint8_t resp[6] = { 0 };
    TRY(mhz19_cmd(handle, 0x86, NULL, resp));
    *co2 = resp[0]<<8 | resp[1];
    return ESP_OK;
}
