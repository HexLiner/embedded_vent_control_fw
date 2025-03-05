#ifndef SSD1306_H_
#define SSD1306_H_

#include <stdint.h>
#include <stdbool.h>


typedef enum {
    SSD1306_FOUNT_MODE_K1 = 1,
    SSD1306_FOUNT_MODE_K2 = 2,
    SSD1306_FOUNT_MODE_K3 = 3,
} ssd1306_fount_mode_t;


void ssd1306_init();
void ssd1306_clear();
void ssd1306_set_img(uint8_t data, uint8_t x, uint8_t y);
void ssd1306_print_digit(uint16_t digit, uint8_t digit_max_len, ssd1306_fount_mode_t ssd1306_fount_mode, uint8_t x, uint8_t y);
void ssd1306_print_str(const char *str, ssd1306_fount_mode_t ssd1306_fount_mode, uint8_t x, uint8_t y);
void ssd1306_print_simw(char simw, ssd1306_fount_mode_t ssd1306_fount_mode, uint8_t x, uint8_t y);

#endif   // SSD1306_H_

