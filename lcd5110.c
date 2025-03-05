
#include <stdint.h>
#include <stdbool.h>
#include "lcd5110.h"
#include "gpio_driver.h"
#include "systimer.h"


#define LCD5110_RST_SET   (GPIOC_SET(2))
#define LCD5110_RST_RESET (GPIOC_RESET(2))
#define LCD5110_CE_SET    (GPIOC_SET(1))
#define LCD5110_CE_RESET  (GPIOC_RESET(1))
#define LCD5110_DC_SET    (GPIOB_SET(0))
#define LCD5110_DC_RESET  (GPIOB_RESET(0))
#define LCD5110_DIN_SET   (GPIOB_SET(1))
#define LCD5110_DIN_RESET (GPIOB_RESET(1))
#define LCD5110_CLK_SET   (GPIOB_SET(2))
#define LCD5110_CLK_RESET (GPIOB_RESET(2))

#define LCD5110_WIDTH  (84)
#define LCD5110_HEIGHT (48)

static const unsigned short ASCII[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 20
    {0x00, 0x00, 0x5f, 0x00, 0x00}, // 21 !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // 22 "
    {0x14, 0x7f, 0x14, 0x7f, 0x14}, // 23 #
    {0x24, 0x2a, 0x7f, 0x2a, 0x12}, // 24 $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // 25 %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // 26 &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // 27 '
    {0x00, 0x1c, 0x22, 0x41, 0x00}, // 28 (
    {0x00, 0x41, 0x22, 0x1c, 0x00}, // 29 )
    {0x14, 0x08, 0x3e, 0x08, 0x14}, // 2a *
    {0x08, 0x08, 0x3e, 0x08, 0x08}, // 2b +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // 2c ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // 2d -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // 2e .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // 2f /
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, // 30 0
    {0x00, 0x42, 0x7f, 0x40, 0x00}, // 31 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 32 2
    {0x21, 0x41, 0x45, 0x4b, 0x31}, // 33 3
    {0x18, 0x14, 0x12, 0x7f, 0x10}, // 34 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 35 5
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, // 36 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 37 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 38 8
    {0x06, 0x49, 0x49, 0x29, 0x1e}, // 39 9
};




void lcd5110_init(void) {
    // set GPIOs
    LCD5110_CE_RESET;
    LCD5110_CLK_RESET;
    LCD5110_RST_RESET;
    systimer_delay_ms(2);
    LCD5110_RST_SET;

    // init LCD
    lcd5110_send_byte(false, 0x21);    // LCD Extended Commands
    lcd5110_send_byte(false, 0xb1);    // Set LCD Cop (Contrast).    //0xb1
    lcd5110_send_byte(false, 0x04);    // Set Temp coefficent.       //0x04
    lcd5110_send_byte(false, 0x14);    // LCD bias mode 1:48.        //0x13
    lcd5110_send_byte(false, 0x0c);    // LCD in normal mode. 0x0d inverse mode
    lcd5110_send_byte(false, 0x20);
    lcd5110_send_byte(false, 0x0c);

    lcd5110_clear();
}


void lcd5110_send_byte(bool is_data, uint8_t data) {
    uint8_t pattern = 0b10000000;
	uint8_t i;


    if (is_data) LCD5110_DC_SET;
    else LCD5110_DC_RESET;

    for (i = 0; i < 8; i++) {
        LCD5110_CLK_RESET;
        if(data & pattern) LCD5110_DIN_SET;
        else LCD5110_DIN_RESET;
        LCD5110_CLK_SET;
        pattern >>= 1;
    }
}


void lcd5110_clear(void) {
	uint16_t i;


    for (i = 0; i < (LCD5110_WIDTH * LCD5110_HEIGHT / 8); i++) {
        lcd5110_send_byte(true, 0x00);
	}

    lcd5110_send_byte(false, 0x80 | 0); // set x coordinate to 0
    lcd5110_send_byte(false, 0x40 | 0); // set y coordinate to 0
}


void lcd5110_print_char(char data) {
	uint8_t i;


    lcd5110_send_byte(true, 0x00);
    for (i = 0; i < 5; i++) {
        lcd5110_send_byte(true, ASCII[data-0x20][i]);
	}
    lcd5110_send_byte(true, 0x00);
}


void lcd5110_print_string(char *data) {
    while(*data) {
        lcd5110_print_char(*data++);
	}
}
