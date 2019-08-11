#include "pzem004t.h"
#include "util.h"

// The CRC code is borrowed from:
// https://github.com/mandulaj/PZEM-004T-v30/blob/master/PZEM004Tv30.cpp
uint16_t crc16(const uint8_t *data, uint16_t len) {
    // Pre computed CRC table
    static const uint16_t crcTable[] = {
        0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241,
        0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
        0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40,
        0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
        0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40,
        0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
        0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641,
        0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
        0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240,
        0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
        0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41,
        0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
        0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41,
        0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
        0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640,
        0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
        0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240,
        0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
        0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41,
        0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
        0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41,
        0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
        0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640,
        0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
        0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241,
        0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
        0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40,
        0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
        0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40,
        0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
        0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641,
        0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040 };

    uint8_t nTemp; // CRC table index
    uint16_t crc = 0xffff; // Default value

    while (len--) {
        nTemp = *data++ ^ crc;
        crc >>= 8;
        crc ^= crcTable[nTemp];
    }
    return crc;
}

pzem004t_err_t pzem004t_cmd(pzem004t_t *handle, uint16_t reg_addr, uint16_t num_reg, uint8_t *resp_bytes, size_t resp_len) {
    uint8_t req_bytes[8] = {
        0xf8, // slave addr
        0x04,
        reg_addr>>8,
        reg_addr&0xff,
        num_reg>>8,
        num_reg&0xff,
        0x00, // crc
        0x00, // crc
    };
    uint16_t crc = crc16(req_bytes, 6);
    req_bytes[6] = crc&0xff;
    req_bytes[7] = crc>>8;

    int w = uart_write_bytes(handle->port, (const char*)req_bytes, sizeof(req_bytes));
    if (w < 0) {
        return PZEM004T_ERR_UART_W;
    }
    if (!resp_len) {
        return ESP_OK;
    }

    int r = uart_read_bytes(handle->port, resp_bytes, resp_len, 100 / portTICK_RATE_MS);
    if (r < 0) {
        return PZEM004T_ERR_UART_R;
    }
    if (r < resp_len) {
        return PZEM004T_ERR_READ_ERROR;
    }

    if (resp_len >= 2) {
        uint16_t crc = crc16(resp_bytes, resp_len-2);
        if ((resp_bytes[resp_len-1]<<8 | resp_bytes[resp_len-2]) != crc) {
            return PZEM004T_ERR_CHECKSUM;
        }
    }
    return ESP_OK;
}

pzem004t_err_t pzem004t_init(pzem004t_t *handle, uart_port_t port, gpio_num_t tx_pin, gpio_num_t rx_pin) {
    uart_config_t uart_config = {
        .baud_rate           = 9600,
        .data_bits           = UART_DATA_8_BITS,
        .parity              = UART_PARITY_DISABLE,
        .stop_bits           = UART_STOP_BITS_1,
        .flow_ctrl           = UART_HW_FLOWCTRL_DISABLE,
    };
    TRY(uart_param_config(port, &uart_config));
    TRY(uart_set_pin(port, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    TRY(uart_driver_install(port, 1024, 0, 0, NULL, 0));

    handle->port = port;

    return pzem004t_cmd(handle, 0, 1, NULL, 0);
}

pzem004t_err_t pzem004t_measurements(pzem004t_t *handle, pzem004t_measurements_t *measurements) {
    uint8_t resp_bytes[25] = { 0 };
    TRY(pzem004t_cmd(handle, 0, 10, resp_bytes, sizeof(resp_bytes)));

    uint16_t voltage_raw   = resp_bytes[3]<<8 | resp_bytes[4];
    uint32_t milliamp_raw  = resp_bytes[5]<<8 | resp_bytes[6] | resp_bytes[7]<<24 | resp_bytes[8]<<16;
    uint32_t power_raw     = resp_bytes[9]<<8 | resp_bytes[10] | resp_bytes[11]<<24 | resp_bytes[12]<<16;
    uint32_t frequency_raw = resp_bytes[17]<<8 | resp_bytes[18];
    measurements->voltage      = (float)voltage_raw / 10.0;
    measurements->current_a    = (float)milliamp_raw / 1000.0;
    measurements->power_w      = (float)power_raw / 10.0;
    measurements->frequency_hz = (float)frequency_raw / 10.0;
    return ESP_OK;
}
