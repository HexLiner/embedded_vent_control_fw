#include "cli.h"
#include <stdint.h>
#include <stdbool.h>
#include "cli_uart.h"
#include "eeprom_driver.h"


#define DEVICE_EEPROM_REG_QTY (1024)

static void cli_cmd_processing(void);
static bool hex_text_to_u8(uint8_t *simw_ptr, uint8_t *result);
static void u8_to_hex_text(uint8_t value, uint8_t *result);


//// OPTIMIZED CLI !!!


void cli_init(void) {
    cli_uart_init();

    cli_uart_tx_buff[cli_uart_tx_size] = '\r';
    cli_uart_tx_size++;
    cli_uart_tx_buff[cli_uart_tx_size] = '\n';
    cli_uart_tx_size++;
    cli_uart_tx_buff[cli_uart_tx_size] = '\n';
    cli_uart_tx_size++;
    cli_uart_tx_size = 0;
    cli_uart_tx_buff[cli_uart_tx_size] = 'H';
    cli_uart_tx_size++;
    cli_uart_tx_buff[cli_uart_tx_size] = 'i';
    cli_uart_tx_size++;
    cli_uart_tx_buff[cli_uart_tx_size] = '\r';
    cli_uart_tx_size++;
    cli_uart_tx_buff[cli_uart_tx_size] = '\n';
    cli_uart_tx_size++;
    cli_uart_tx_buff[cli_uart_tx_size] = '@';
    cli_uart_tx_size++;
    cli_uart_tx_buff[cli_uart_tx_size] = ' ';
    cli_uart_tx_size++;
    cli_uart_send_answer();
}


void cli_process(void) {
    if (cli_uart_is_cmd_recived()) {
        cli_cmd_processing();
        cli_uart_send_answer();
        ////cli_process_state = CLI_PROCESS_STATE_SEND_ANSWER;
    }
}




static void cli_cmd_processing(void) {
    uint16_t addr;
    uint8_t value[4];
    bool is_error = true;


    cli_uart_tx_size = 0;
    cli_uart_tx_buff[cli_uart_tx_size] = '\r';
    cli_uart_tx_size++;
    cli_uart_tx_buff[cli_uart_tx_size] = '\n';
    cli_uart_tx_size++;

    if (cli_uart_rx_cnt > 0) {

        // ewAAAA XX | erAAAA (all in HEX)  - read/write EEPROM

        if (cli_uart_rx_buff[0] != 'e') {
            goto end;
        }

        if (cli_uart_rx_cnt >= 6) {
            addr = 0;
            if (!hex_text_to_u8(&cli_uart_rx_buff[2], ((uint8_t*)&addr + 1))) goto end;
            if (!hex_text_to_u8(&cli_uart_rx_buff[4], (uint8_t*)&addr)) goto end;
            

            if ((cli_uart_rx_buff[1] == 'r') && (cli_uart_rx_cnt == 6)) {
                if (addr > DEVICE_EEPROM_REG_QTY) goto end;
                
                eeprom_driver_read(addr, 1, value);

                u8_to_hex_text(value[0], &cli_uart_tx_buff[cli_uart_tx_size]);
                cli_uart_tx_size += 2;

                is_error = false;
            }
            else if (cli_uart_rx_buff[1] == 'w') {
                if (cli_uart_rx_cnt == 9) {
                    if (addr > DEVICE_EEPROM_REG_QTY) goto end;
                    if (!hex_text_to_u8(&cli_uart_rx_buff[7], &value[0])) goto end;

                    eeprom_driver_write(addr, 1, value);
                }
                else {
                    goto end;
                }

                cli_uart_tx_buff[cli_uart_tx_size] = 'O';
                cli_uart_tx_size++;
                cli_uart_tx_buff[cli_uart_tx_size] = 'K';
                cli_uart_tx_size++;

                is_error = false;
            }
        }

    }
    else {
        is_error = false;
    }


end:
    if (is_error) {
        cli_uart_tx_buff[cli_uart_tx_size] = 'E';
        cli_uart_tx_size++;
        cli_uart_tx_buff[cli_uart_tx_size] = 'r';
        cli_uart_tx_size++;
        cli_uart_tx_buff[cli_uart_tx_size] = 'r';
        cli_uart_tx_size++;
        cli_uart_tx_buff[cli_uart_tx_size] = '!';
        cli_uart_tx_size++;
    }
    cli_uart_tx_buff[cli_uart_tx_size] = '\r';
    cli_uart_tx_size++;
    cli_uart_tx_buff[cli_uart_tx_size] = '\n';
    cli_uart_tx_size++;
    cli_uart_tx_buff[cli_uart_tx_size] = '@';
    cli_uart_tx_size++;
    cli_uart_tx_buff[cli_uart_tx_size] = ' ';
    cli_uart_tx_size++;

    cli_uart_rx_cnt = 0;
}




static bool hex_text_to_u8(uint8_t *simw_ptr, uint8_t *result) {
    uint8_t result_tmp;


    result_tmp = simw_ptr[0];
    if ((simw_ptr[0] < '0') || (simw_ptr[0] > '9')) {
        if ((simw_ptr[0] < 'A') || (simw_ptr[0] > 'F')) return false;
        result_tmp -= ('A' - 10 - '0');
    }
    result_tmp -= '0';
    *result = result_tmp << 4;
    
    result_tmp = simw_ptr[1];
    if ((simw_ptr[1] < '0') || (simw_ptr[1] > '9')) {
        if ((simw_ptr[1] < 'A') || (simw_ptr[1] > 'F')) return false;
        result_tmp -= ('A' - 10 - '0');
    }
    result_tmp -= '0';
    *result |= result_tmp;

    return true;
}


static void u8_to_hex_text(uint8_t value, uint8_t *result) {
    uint8_t value_temp;


    value_temp = value & 0x0F;
    if (value_temp > 9) result[1] = ('A' - 10);
    else result[1] = '0';
    result[1] += value_temp;

    value_temp = value >> 4;
    if (value_temp > 9) result[0] = ('A' - 10);
    else result[0] = '0';
    result[0] += value_temp;
}
