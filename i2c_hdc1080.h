#ifndef I2C_HDC1080_H_
#define I2C_HDC1080_H_

#include <stdint.h>
#include <stdbool.h>


bool i2c_hdc1080_init(void);
bool i2c_hdc1080_start_one_meas(void);
bool i2c_hdc1080_read_meas(int8_t *temperature_c, uint8_t *humidity_pct);


#endif   // I2C_HDC1080_H_
