#include "sgp30.h"
#include <driver/i2c.h>
#include <esp_log.h>
#include <stdint.h>
#include <string.h>
#include "util.h"

#define SGP30_ADDR 0x58

uint8_t crc8(uint8_t *data, size_t len) {
    uint8_t poly = 0x31;
    uint8_t crc = 0xff;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc&0x80) ? (((crc<<1)^poly)&0xff) : (crc<<1);
        }
    }
    return crc;
}

sgp30_err_t sgp30_cmd(uint16_t opcode, uint16_t *response, size_t response_len) {
    uint8_t req_bytes[2] = { opcode>>8, opcode&0xff };
    i2c_cmd_handle_t wcmd = i2c_cmd_link_create();
    TRY(i2c_master_start(wcmd));
    TRY(i2c_master_write_byte(wcmd, (SGP30_ADDR<<1) | I2C_MASTER_WRITE, true));
    TRY(i2c_master_write(wcmd, req_bytes, 2, true));
    TRY(i2c_master_stop(wcmd));
    esp_err_t err = i2c_master_cmd_begin(SGP30_I2C_PORT, wcmd, 500 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(wcmd);
    TRY(err);

    // Wait for the sensor to prepare the response. The wait time is determined
    // by the command that takes the longest to complete.
    vTaskDelay(240 / portTICK_PERIOD_MS);

    uint8_t resp_bytes[response_len*3];
    memset(resp_bytes, 0, response_len*3);
    i2c_cmd_handle_t rcmd = i2c_cmd_link_create();
    TRY(i2c_master_start(rcmd));
    TRY(i2c_master_write_byte(rcmd, (SGP30_ADDR<<1) | I2C_MASTER_READ, true));
    if (response_len) {
        TRY(i2c_master_read(rcmd, resp_bytes, response_len*3, I2C_MASTER_LAST_NACK));
    }
    TRY(i2c_master_stop(rcmd));
    err = i2c_master_cmd_begin(SGP30_I2C_PORT, rcmd, 500 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(rcmd);
    TRY(err);

    for (int i = 0; i < response_len; i++) {
        if (crc8(&resp_bytes[i*3], 2) != resp_bytes[i*3+2]) {
            return SGP30_CRC_FAIL;
        }
        response[i] = (resp_bytes[i*3]<<8) | resp_bytes[i*3+1];
    }

    return 0;
}

sgp30_err_t sgp30_init_air_quality() {
    return sgp30_cmd(0x2003, NULL, 0);
}

sgp30_err_t sgp30_measure_air_quality(uint16_t *eco2_ppm, uint16_t *tvoc_ppb) {
    uint16_t resp[2] = { 0 };
    TRY(sgp30_cmd(0x2008, resp, 2));
    *eco2_ppm = resp[0];
    *tvoc_ppb = resp[1];
    return 0;
}

sgp30_err_t sgp30_measure_test() {
    uint16_t resp = 0;
    TRY(sgp30_cmd(0x2032, &resp, 1));
    return resp == 0xd400 ? 0 : SGP30_MEASURE_TEST_FAIL;
}
