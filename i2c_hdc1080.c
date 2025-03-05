#include <stdint.h>
#include <stdbool.h>
#include "i2c_hdc1080.h"
#include "systimer.h"
#include "twi_driver.h"


#define HDC1080_ADDR (0b10000000)

#define HDC1080_REG_TEMPERATURE (0x00)
#define HDC1080_REG_HUMIDITY    (0x01)
#define HDC1080_REG_CONFIG      (0x02)
    #define HDC1080_REG_CONFIG_RST           (1 << 15)   // Software reset
    #define HDC1080_REG_CONFIG_HEAT          (1 << 13)   // Heater
    #define HDC1080_REG_CONFIG_MODE_T_OR_H   (0 << 12)   // Mode of acquisition
    #define HDC1080_REG_CONFIG_MODE_T_AND_H  (1 << 12)   // Mode of acquisition
    #define HDC1080_REG_CONFIG_BTST          (1 << 11)   // Battery Status
    #define HDC1080_REG_CONFIG_TRES_14_BIT   (0 << 10)   // Temperature Measurement Resolution
    #define HDC1080_REG_CONFIG_TRES_11_BIT   (1 << 10)   // Temperature Measurement Resolution
    #define HDC1080_REG_CONFIG_HRES_14_BIT   (0 << 8)    // Humidity Measurement Resolution
    #define HDC1080_REG_CONFIG_HRES_11_BIT   (1 << 8)    // Humidity Measurement Resolution
    #define HDC1080_REG_CONFIG_HRES_8_BIT    (2 << 8)    // Humidity Measurement Resolution
#define HDC1080_REG_SERIAL_ID_2 (0xFB)
#define HDC1080_REG_SERIAL_ID_1 (0xFC)
#define HDC1080_REG_SERIAL_ID_0 (0xFD)
#define HDC1080_REG_MANUF_ID    (0xFE)
#define HDC1080_REG_DEVICE_ID   (0xFF)


static twi_driver_msg_t hdc_1080_twi_driver_msg;



bool i2c_hdc1080_init(void) {
    uint16_t config_reg;
    uint8_t tx_data[3];


    hdc_1080_twi_driver_msg.slave_addr = HDC1080_ADDR;
    hdc_1080_twi_driver_msg.timeout_ms = 1000;

    config_reg = HDC1080_REG_CONFIG_MODE_T_AND_H | HDC1080_REG_CONFIG_TRES_14_BIT | HDC1080_REG_CONFIG_HRES_14_BIT;

    hdc_1080_twi_driver_msg.rx_data_max_qty = 0;
    hdc_1080_twi_driver_msg.tx_data = tx_data;
    tx_data[0] = HDC1080_REG_CONFIG;
    tx_data[1] = config_reg >> 8;
    tx_data[2] = (uint8_t)config_reg;
    hdc_1080_twi_driver_msg.tx_data_qty = 3;
    if (twi_driver_transmit(&hdc_1080_twi_driver_msg) != 0) return true;
    return false;
}


bool i2c_hdc1080_start_one_meas(void) {
    uint8_t tx_data[1];


    hdc_1080_twi_driver_msg.rx_data_max_qty = 0;
    hdc_1080_twi_driver_msg.tx_data = tx_data;
    tx_data[0] = HDC1080_REG_TEMPERATURE;
    hdc_1080_twi_driver_msg.tx_data_qty = 1;
    if (twi_driver_transmit(&hdc_1080_twi_driver_msg) != 0) return true;
    return false;
}


bool i2c_hdc1080_read_meas(int8_t *temperature_c, uint8_t *humidity_pct) {
    uint8_t rx_data[4];
    uint8_t rx_data_qty;
    uint32_t temperature_unsigned;
    uint32_t humidity;


    hdc_1080_twi_driver_msg.rx_data_max_qty = 4;
    hdc_1080_twi_driver_msg.received_data_qty = &rx_data_qty;
    hdc_1080_twi_driver_msg.rx_data = rx_data;
    hdc_1080_twi_driver_msg.tx_data_qty = 0;
    if (twi_driver_transmit(&hdc_1080_twi_driver_msg) != 0) return true;

    // Convertation
    temperature_unsigned = (uint16_t)(rx_data[0] << 8) | rx_data[1];
    temperature_unsigned *= 165;
    temperature_unsigned = temperature_unsigned >> 16;
    *temperature_c = temperature_unsigned - 40;

    humidity = (uint16_t)(rx_data[2] << 8) | rx_data[3];
    humidity *= 100;
    humidity = humidity >> 16;
    *humidity_pct = humidity;

    return false;
}


/*
void i2c_hdc1080_read_meas(int16_t *temperature_x100, uint16_t *humidity_x100) {
    uint8_t rx_data[4];
    uint8_t rx_data_qty;
    uint32_t temperature_unsigned;
    uint32_t humidity;


    hdc_1080_twi_driver_msg.rx_data_max_qty = 4;
    hdc_1080_twi_driver_msg.received_data_qty = &rx_data_qty;
    hdc_1080_twi_driver_msg.rx_data = rx_data;
    hdc_1080_twi_driver_msg.tx_data_qty = 0;
    twi_driver_transmit(&hdc_1080_twi_driver_msg);

    // Convertation
    temperature_unsigned = (uint16_t)(rx_data[0] << 8) | rx_data[1];
    temperature_unsigned *= 165 * 100;
    temperature_unsigned = temperature_unsigned >> 16;
    *temperature_x100 = temperature_unsigned - 4000;

    humidity = (uint16_t)(rx_data[2] << 8) | rx_data[3];
    humidity *= 100 * 100;
    humidity = humidity >> 16;
    *humidity_x100 = humidity;
}
*/


/*
//  ***************************************************************************
/// @brief  Check manufacturer ID of hdc1080.
/// @param  handle: Device handle.
/// @param  serial_number
/// @retval serial_number: serial number hdc1080.
/// @return result @ref error_t
//  ***************************************************************************
error_t i2c_hdc1080_check_manuf_id(handle_t handle) {
    i2c_hdc1080_internal_t  *dev;
    i2c_transaction_t       i2c_transaction;
    uint8_t                 tx_data[1];
    uint8_t                 rx_data[4];
    error_t                 result;
    timer_t                 timeout;


    // Checking input parameters
    dev = (i2c_hdc1080_internal_t*)handle;
    if (dev == NULL) return E_SOFTWARE_FLAG | E_SOURCE_DEVICE_DRIVER | E_NO_DEVICE;


    // I2C transaction preparation
    i2c_transaction.address = HDC1080_ADDR;
    i2c_transaction.tx_size = 1;
    i2c_transaction.tx_data = tx_data;
    i2c_transaction.rx_size = 2;
    i2c_transaction.rx_data = rx_data;

    // Reading manuf ID
    i2c_transaction.tx_data[0] = HDC1080_REG_MANUF_ID;
    timeout = timer_start_ms(dev->config->comm_timeout_ms);
    result = i2c_transfer(*(dev->config->i2c_handle), &i2c_transaction, dev->config->comm_retries, timeout);
    if (result != E_OK) return result;
    if (rx_data[0] != 0x54) return E_INVALID_ID;
    if (rx_data[1] != 0x49) return E_INVALID_ID;

    // Reading device ID
    i2c_transaction.tx_data[0] = HDC1080_REG_DEVICE_ID;
    timeout = timer_start_ms(dev->config->comm_timeout_ms);
    result = i2c_transfer(*(dev->config->i2c_handle), &i2c_transaction, dev->config->comm_retries, timeout);
    if (result != E_OK) return result;
    if (rx_data[0] != 0x10) return E_INVALID_ID;
    if (rx_data[1] != 0x50) return E_INVALID_ID;

    return E_OK;
}


//  ***************************************************************************
/// @brief  Software restart hdc1080.
/// @param  handle: Device handle.
/// @retval none
/// @return result @ref error_t
//  ***************************************************************************
error_t i2c_hdc1080_soft_reset(handle_t handle) {
    i2c_hdc1080_internal_t  *dev;
    i2c_transaction_t       i2c_transaction;
    uint8_t                 tx_data[3];
    error_t                 result;
    timer_t                 timeout;
    uint16_t                config_reg;


    // Checking input parameters
    dev = (i2c_hdc1080_internal_t*)handle;
    if (dev == NULL) return E_SOFTWARE_FLAG | E_SOURCE_DEVICE_DRIVER | E_NO_DEVICE;


    config_reg = HDC1080_REG_CONFIG_RST;

    // I2C transaction preparation
    i2c_transaction.address = HDC1080_ADDR;
    i2c_transaction.tx_size = 3;
    i2c_transaction.tx_data = tx_data;
    i2c_transaction.tx_data[0] = HDC1080_REG_CONFIG;
    i2c_transaction.tx_data[1] = config_reg >> 8;
    i2c_transaction.tx_data[2] = (uint8_t)config_reg;
    i2c_transaction.rx_size = 0;
    i2c_transaction.rx_data = NULL;

    timeout = timer_start_ms(dev->config->comm_timeout_ms);
    result = i2c_transfer(*(dev->config->i2c_handle), &i2c_transaction, dev->config->comm_retries, timeout);
    return result;
}
*/
