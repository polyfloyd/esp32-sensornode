#include "bme280.h"
#include "util.h"

#define BME280_ADDR 0x76 // SDO = GND

#define OVERSAMPLING_H 5 // 16x
#define OVERSAMPLING_T 5 // 16x
#define OVERSAMPLING_P 5 // 16x


// the bme280_compensate_* functions were copied from the datasheet.

typedef int32_t  BME280_S32_t;
typedef int64_t  BME280_S64_t;
typedef uint32_t BME280_U32_t;

// Returns temperature in DegC, resolution is 0.01 DegC. Output value of “5123”
// equals 51.23 DegC. t_fine carries fine temperature as global value.
BME280_S32_t bme280_compensate_t_int32(bme280_t *h, BME280_S32_t adc_T, BME280_S32_t *t_fine) {
    BME280_S32_t var1, var2, T;
    var1 = ((((adc_T>>3) -((BME280_S32_t)h->dig_t1<<1))) * ((BME280_S32_t)h->dig_t2)) >> 11;
    var2 = (((((adc_T>>4) -((BME280_S32_t)h->dig_t1)) * ((adc_T>>4) -((BME280_S32_t)h->dig_t1))) >> 12) * ((BME280_S32_t)h->dig_t3)) >> 14;
    *t_fine = var1 + var2;
    T = (*t_fine * 5 + 128) >> 8;
    return T;
}

// Returns pressure in Pa as unsigned 32 bit integer in Q24.8 format (24
// integer bits and 8 fractional bits). Output value of “24674867” represents
// 24674867/256 = 96386.2 Pa = 963.862 hPa.
BME280_U32_t bme280_compensate_p_int64(bme280_t *h, BME280_S32_t adc_P, BME280_S32_t t_fine) {
    BME280_S64_t var1, var2, p;
    var1 = ((BME280_S64_t)t_fine) - 128000;
    var2 = var1 * var1 * (BME280_S64_t)h->dig_p6;
    var2 = var2 + ((var1*(BME280_S64_t)h->dig_p5)<<17);
    var2 = var2 + (((BME280_S64_t)h->dig_p4)<<35);
    var1 = ((var1 * var1 * (BME280_S64_t)h->dig_p3)>>8) + ((var1 * (BME280_S64_t)h->dig_p2)<<12);
    var1 = (((((BME280_S64_t)1)<<47)+var1))*((BME280_S64_t)h->dig_p1)>>33;
    if (var1 == 0) {
        // avoid exception caused by division by zero
        return 0;
    }
    p = 1048576-adc_P;
    p = (((p<<31)-var2)*3125)/var1;
    var1 = (((BME280_S64_t)h->dig_p9) * (p>>13) * (p>>13)) >> 25;
    var2 = (((BME280_S64_t)h->dig_p8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((BME280_S64_t)h->dig_p7)<<4);
    return (BME280_U32_t)p;
}

// Returns humidity in %RH as unsigned 32 bit integer in Q22.10 format (22
// integer and 10 fractional bits). Output value of “47445” represents
// 47445/1024 = 46.333 %RH.
BME280_U32_t bme280_compensate_h_int32(bme280_t *h, BME280_S32_t adc_H, BME280_S32_t t_fine) {
    BME280_S32_t v_x1_u32r;
    v_x1_u32r = (t_fine -((BME280_S32_t)76800));
    v_x1_u32r = (((((adc_H << 14) -(((BME280_S32_t)h->dig_h4) << 20) -(((BME280_S32_t)h->dig_h5) * v_x1_u32r)) + ((BME280_S32_t)16384)) >> 15) * (((((((v_x1_u32r * ((BME280_S32_t)h->dig_h6)) >> 10) * (((v_x1_u32r * ((BME280_S32_t)h->dig_h3)) >> 11) + ((BME280_S32_t)32768))) >> 10) + ((BME280_S32_t)2097152)) * ((BME280_S32_t)h->dig_h2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r -(((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((BME280_S32_t)h->dig_h1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400? 419430400: v_x1_u32r);
    return (BME280_U32_t)(v_x1_u32r>>12);
}


bme280_err_t bme280_write(bme280_t *handle, uint8_t addr, uint8_t *data, size_t data_len) {
    i2c_cmd_handle_t wcmd = i2c_cmd_link_create();
    TRY(i2c_master_start(wcmd));
    TRY(i2c_master_write_byte(wcmd, (BME280_ADDR<<1) | I2C_MASTER_WRITE, true));
    TRY(i2c_master_write_byte(wcmd, addr, true));
    if (data) {
        TRY(i2c_master_write(wcmd, data, data_len, true));
    }
    TRY(i2c_master_stop(wcmd));
    esp_err_t err = i2c_master_cmd_begin(handle->port, wcmd, 500 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(wcmd);
    return err;
}

bme280_err_t bme280_read(bme280_t *handle, uint8_t addr, uint8_t *resp, size_t resp_len) {
    TRY(bme280_write(handle, addr, NULL, 0));

    i2c_cmd_handle_t rcmd = i2c_cmd_link_create();
    TRY(i2c_master_start(rcmd));
    TRY(i2c_master_write_byte(rcmd, (BME280_ADDR<<1) | I2C_MASTER_READ, true));
    TRY(i2c_master_read(rcmd, resp, resp_len, I2C_MASTER_LAST_NACK));
    TRY(i2c_master_stop(rcmd));
    esp_err_t err = i2c_master_cmd_begin(handle->port, rcmd, 500 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(rcmd);
    return err;
}

bme280_err_t bme280_reset(bme280_t *handle) {
    uint8_t reset_v = 0xb6;
    return bme280_write(handle, 0xe0, &reset_v, 1);
}

bme280_err_t bme280_measure(bme280_t *handle, bme280_measuremnt_t *measurement) {
    uint8_t data[8] = { 0 };
    TRY(bme280_read(handle, 0xf7, data, sizeof(data)));
    uint32_t raw_pressure    = data[0]<<12 | data[1]<<4 | data[2]>>4;
    uint32_t raw_temperature = data[3]<<12 | data[4]<<4 | data[5]>>4;
    uint32_t raw_humidity    = data[6]<<8  | data[7];

    BME280_S32_t t_fine = 0;
    int32_t temperature = bme280_compensate_t_int32(handle, raw_temperature, &t_fine);
    uint32_t pressure   = bme280_compensate_p_int64(handle, raw_pressure, t_fine);
    uint32_t humidity   = bme280_compensate_h_int32(handle, raw_humidity, t_fine);

    measurement->temperature_c = (float)temperature/100.0;
    measurement->pressure_pa = (pressure>>8) + (float)(pressure&0xff)/256.0;
    measurement->humidity = ((humidity>>10) + (float)(humidity&0x3ff)/1024.0) / 100.0;
    return ESP_OK;
}

bme280_err_t bme280_load_compensation_table(bme280_t *handle) {
    uint8_t dig_tp[6+18] = { 0 };
    TRY(bme280_read(handle, 0x88, dig_tp, sizeof(dig_tp)));
    handle->dig_t1 = dig_tp[0]  | dig_tp[1]<<8;
    handle->dig_t2 = dig_tp[2]  | dig_tp[3]<<8;
    handle->dig_t3 = dig_tp[4]  | dig_tp[5]<<8;
    handle->dig_p1 = dig_tp[6]  | dig_tp[7]<<8;
    handle->dig_p2 = dig_tp[8]  | dig_tp[9]<<8;
    handle->dig_p3 = dig_tp[10] | dig_tp[11]<<8;
    handle->dig_p4 = dig_tp[12] | dig_tp[13]<<8;
    handle->dig_p5 = dig_tp[14] | dig_tp[15]<<8;
    handle->dig_p6 = dig_tp[16] | dig_tp[17]<<8;
    handle->dig_p7 = dig_tp[18] | dig_tp[19]<<8;
    handle->dig_p8 = dig_tp[20] | dig_tp[21]<<8;
    handle->dig_p9 = dig_tp[22] | dig_tp[23]<<8;

    TRY(bme280_read(handle, 0xa1, &handle->dig_h1, 1));
    uint8_t dig_h[8] = { 0 };
    TRY(bme280_read(handle, 0xe1, dig_h, sizeof(dig_h)));
    handle->dig_h2 = dig_h[0] | dig_h[1]<<8;
    handle->dig_h3 = dig_h[2];
    handle->dig_h4 = dig_h[3]<<4 | (dig_h[4]&7);
    handle->dig_h5 = dig_h[5]>>4 | dig_h[6]<<4;
    handle->dig_h6 = dig_h[7];

    return ESP_OK;
}

bme280_err_t bme280_init(bme280_t *handle, i2c_port_t port) {
    bme280_t h = {
        .port = port,
    };

    uint8_t chip_id = 0;
    TRY(bme280_read(&h, 0xd0, &chip_id, 1));
    if (chip_id != 0x60) {
        return BME280_ERR_BAD_CHIP_ID;
    }

    TRY(bme280_reset(&h));

    uint8_t osrs_h = OVERSAMPLING_H;
    TRY(bme280_write(&h, 0xf2, &osrs_h, 1));
    uint8_t ctrl_meas = OVERSAMPLING_T<<5 | OVERSAMPLING_P<<2 | 3;
    TRY(bme280_write(&h, 0xf4, &ctrl_meas, 1));

    TRY(bme280_load_compensation_table(&h));

    *handle = h;
    return ESP_OK;
}
