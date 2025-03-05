#include <stdint.h>
#include <stdbool.h>
#include "device_registers.h"
#include "eeprom_driver.h"


static uint8_t drvice_reg_cmd = 0;


// BE - HHLL
uint8_t *device_registers_ptr[DEVICE_RAM_REG_QTY] = {
    // 0
    (uint8_t*)&drvice_reg_cmd,
};


void drvice_registers_proc(void) {
    switch (drvice_reg_cmd) {
        case 1:
            // clear log

            break;
    }
    drvice_reg_cmd = 0;
}
