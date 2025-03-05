#include <stdint.h>
#include <stdbool.h>


#ifndef LCD5110_H
#define LCD5110_H


void lcd5110_init(void);
void lcd5110_send_byte(bool is_cmd, uint8_t data);
void lcd5110_clear(void);
void lcd5110_print_char(char data);
void lcd5110_print_string(char *data);


#endif   // LCD5110
