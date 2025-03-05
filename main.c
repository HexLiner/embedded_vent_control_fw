#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "gpio_driver.h"
#include "systimer.h"
#include "encoder_driver.h"
#include "ssd1306.h"
#include "twi_driver.h"
#include "i2c_hdc1080.h"
#include "eeprom_driver.h"




#define LOAD_ENABLE_PAUSE_TIME_MS (5 * 1000)
#define EEPROM_LOG_BASE_ADDR      (0x000C)
#define EEPROM_LOG_SIZE           (110 * 2)   // Wlog = 128-18 = 110 -> Tlog = 110/4 = 27 hours


typedef union {
    struct {
        uint8_t min_humidity_pct;
        uint8_t vent_offset_time_m;
        uint8_t vent_en_time_m;
        uint8_t max_temperature_nm_c;
        uint8_t max_temperature_m_c;
        uint8_t heat_offset_time_m;
        uint8_t heat_en_time_m;
        uint8_t vent_heat_hysteresis;
        uint8_t log_period_m;
    };
    uint8_t config[9];
} device_config_t;

timer_t heat_vent_process_timer_s;
uint16_t heat_vent_process_timer_m;
uint16_t heat_cycle_time_m;
uint16_t vent_cycle_time_m;
bool is_heater_en;
bool is_vent_en;
device_config_t device_config;
uint8_t min_temperature_c, max_temperature_c;
uint8_t max_humidity_pct;
bool is_heater_en_request, is_vent_en_request;
timer_t load_enable_timer_ms;
uint8_t curr_humidity_pct;
uint8_t curr_temperature_c;

typedef enum {
    LCD_INTRO = 0,
    LCD_STATE_MAIN_SCREEN,
    LCD_STATE_MENU_ITEMS_LIST,
    LCD_STATE_MENU_ITEM_VALUE,
    LCD_STATE_SHOW_LOG,
    LCD_STATE_SHOW_STAT,
    LCD_STATE_OFF
} lcd_state_t;
lcd_state_t lcd_state = LCD_INTRO;
bool is_lcd_state_init = true;
timer_t lcd_update_timer_s, lcd_off_timer_s;
bool is_man_mode = false;
uint8_t lcd_show_log_humidity;

// ! имена пунктов меню и ПУcТОЙ cТРОКИ д.б. одинаковой длины !
const char *lcd_menu_empty_str = "              ";
#define LCD_MENU_CONFIG_ITEMS_OFFSET (5)
#define LCD_MENU_ITEMS_QTY (14)
char *lcd_menu_items[] = {
    "1. show stat  ",    // actions
    "2. clr stat h ",    // actions
    "3. clr stat t ",    // actions
    "4. show log   ",    // actions
    "5. clear log  ",    // actions
    "6. min hum    ",    // config
    "7. vent oft t ",
    "8. vent en t  ",
    "9. max temp NM",
    "10. max temp M",
    "11. heat oft t",
    "12. heat en t ",
    "13. hyst      ",
    "14. log period"
};

timer_t meas_timer_ms;

uint8_t lcd_menu_curr_item;
int8_t enc_step;
uint16_t i_int;
uint8_t i, y, z;

timer_t statistics_process_timer_s;
uint16_t statistics_time_common_minutes;
uint16_t statistics_time_heater_en_minutes;
uint16_t statistics_time_vent_en_minutes;
uint8_t statistics_humidity_min, statistics_humidity_max;

bool is_log_full;
timer_t log_process_timer_s;
uint16_t log_process_eeprom_index;
uint8_t log_process_minutes_cnt;
uint16_t log_process_humidity_avg;
uint8_t log_process_heat_vent_states;



static void vent_en(void);
static void vent_dis(void);
static void heater_en(void);
static void heater_dis(void);




int main(void) {
    // Tim 2 init for systimer
    #ifdef __AVR_ATmega8__
    TCCR2 = (2 << CS20);  // Clock Select: 0x00-0x05 -> 0/1/8/32/64/128/256/1024
    TIMSK |= (1 << TOIE2);  // TOIE2 irq en 
    #else
    TCCR2A = 0;
    TCCR2B = (2 << CS20);  // Clock Select: 0x00-0x05 -> 0/1/8/32/64/128/256/1024
    TIMSK2 = (1 << TOIE2);  // TOIE2 irq en 
    #endif

    sei();   // global IRQ enable

    gpio_init();
    encoder_init();
    twi_driver_init();

    systimer_delay_ms(1000);

    ssd1306_init();
    i2c_hdc1080_init();


    // Read config
    for (i = 0; i < sizeof(device_config_t); i++) {
        eeprom_driver_read_8(i, &device_config.config[i]);
    }

    heat_vent_process_timer_s = systimer_set_s(60);
    if (device_config.vent_offset_time_m > device_config.heat_offset_time_m) heat_vent_process_timer_m = device_config.heat_offset_time_m;
    else heat_vent_process_timer_m = device_config.vent_offset_time_m;

    heater_dis();
    heat_cycle_time_m = device_config.heat_offset_time_m;
    heat_cycle_time_m += device_config.heat_en_time_m;

    vent_dis();
    max_humidity_pct = device_config.min_humidity_pct + device_config.vent_heat_hysteresis;
    vent_cycle_time_m = device_config.vent_offset_time_m;
    vent_cycle_time_m += device_config.vent_en_time_m;

    for (log_process_eeprom_index = EEPROM_LOG_BASE_ADDR; log_process_eeprom_index < (EEPROM_LOG_BASE_ADDR + EEPROM_LOG_SIZE); log_process_eeprom_index += 2) {
        eeprom_driver_read_8(log_process_eeprom_index, &i);
        if (i == 0xFF) break;
    }
    if (log_process_eeprom_index >= (EEPROM_LOG_BASE_ADDR + EEPROM_LOG_SIZE)) is_log_full = true;
    else is_log_full = false;
    log_process_timer_s = systimer_set_s(60);
    log_process_minutes_cnt = 0;  
    log_process_humidity_avg = 0;
    log_process_heat_vent_states = 0;

    statistics_time_common_minutes = 0;
    statistics_time_heater_en_minutes = 0;
    statistics_time_vent_en_minutes = 0;
    statistics_humidity_min = 100;
    statistics_humidity_max = 0;

    i2c_hdc1080_start_one_meas();
    meas_timer_ms = systimer_set_ms(2000);




    while(1) {
        encoder_process();


        // Meas process
        if (systimer_triggered_ms(meas_timer_ms)) {
            i2c_hdc1080_read_meas((int8_t*)&curr_temperature_c, &curr_humidity_pct);
            i2c_hdc1080_start_one_meas();
            meas_timer_ms = systimer_set_ms(1000);

            if (curr_humidity_pct < statistics_humidity_min) statistics_humidity_min = curr_humidity_pct;
            if (curr_humidity_pct > statistics_humidity_max) statistics_humidity_max = curr_humidity_pct;
        }


        // Heater and vent process
        if (is_man_mode) max_temperature_c = device_config.max_temperature_m_c;
        else max_temperature_c = device_config.max_temperature_nm_c;
        min_temperature_c = max_temperature_c - device_config.vent_heat_hysteresis;

        if (curr_temperature_c > max_temperature_c) heater_dis();
        if ((curr_humidity_pct < device_config.min_humidity_pct) && !is_man_mode) vent_dis();

        if (systimer_triggered_s(heat_vent_process_timer_s)) {
            heat_vent_process_timer_s = systimer_set_s(60);
            heat_vent_process_timer_m++;
            
            // heater proc
            if ((heat_vent_process_timer_m <= heat_cycle_time_m) && (heat_vent_process_timer_m > device_config.heat_offset_time_m)) {
                if (curr_temperature_c < min_temperature_c) heater_en();
            }
            else heater_dis();

            // vent proc
            if ((heat_vent_process_timer_m <= vent_cycle_time_m) && (heat_vent_process_timer_m > device_config.vent_offset_time_m)) {
                if (curr_humidity_pct > max_humidity_pct) vent_en();
            }
            else if (!is_man_mode) vent_dis();

            // cycle reload
            if ((heat_vent_process_timer_m > heat_cycle_time_m) && (heat_vent_process_timer_m > vent_cycle_time_m)) {
                heat_vent_process_timer_m = 0;
            }
        }


        // Load enabe sequence process
        if (systimer_triggered_ms(load_enable_timer_ms)) {
            load_enable_timer_ms = systimer_set_ms(LOAD_ENABLE_PAUSE_TIME_MS);
            if (is_heater_en_request) {
                GPIOD_SET(7);
                is_heater_en_request = false;
            }
            else if (is_vent_en_request) {
                GPIOD_SET(6);
                is_vent_en_request = false;
            }
            else {
                load_enable_timer_ms = 0;
            }
        }

    
        // LCD process
        enc_step = encoder_get_step();
        switch (lcd_state) {
            case LCD_INTRO:
                if (is_lcd_state_init) {
                    is_lcd_state_init = false;

                    ssd1306_clear();
                    ssd1306_print_str("/E/", 2, 46, 8);
                    lcd_update_timer_s = systimer_set_s(2);
                }
                if (systimer_triggered_s(lcd_update_timer_s)) {
                    is_lcd_state_init = true;
                    lcd_state = LCD_STATE_MAIN_SCREEN;
                }
                break;


            case LCD_STATE_MAIN_SCREEN:
                if (is_lcd_state_init) {
                    is_lcd_state_init = false;

                    ssd1306_clear();
                    ssd1306_print_str("H=   %", 2, 0, 0);
                    ssd1306_print_str("T=   C", 2, 0, 16);
                    encoder_clear_all_events();
                    enc_step = 0;
                    lcd_update_timer_s = 0;
                    lcd_off_timer_s = systimer_set_s(60);
                }

                if (is_man_mode) ssd1306_print_simw('M', 2, 84, 16);
                else ssd1306_print_simw(' ', 2, 84, 16);
                if (is_heater_en) ssd1306_print_simw('H', 2, 96, 16);
                else ssd1306_print_simw(' ', 2, 96, 16);
                if (is_vent_en) ssd1306_print_simw('V', 2, 108, 16);
                else ssd1306_print_simw(' ', 2, 108, 16);
                if (is_log_full) ssd1306_print_simw('L', 2, 108, 0);
                else ssd1306_print_simw(' ', 2, 108, 0);

                if (systimer_triggered_s(lcd_update_timer_s)) {
                    lcd_update_timer_s = systimer_set_s(2);
                    ssd1306_print_digit(curr_humidity_pct, 3, 2, 24, 0);
                    ssd1306_print_digit(curr_temperature_c, 3, 2, 24, 16);
                }
                else if (encoder_is_press_event()) {
                    lcd_off_timer_s = systimer_set_s(60);
                    is_man_mode = !is_man_mode;

                    if (is_man_mode) vent_en();
                    else vent_dis();
                }
                else if (encoder_is_long_press_event()) {
                    lcd_menu_curr_item = 0;
                    is_lcd_state_init = true;
                    ssd1306_clear();
                    lcd_state = LCD_STATE_MENU_ITEMS_LIST;
                }
                else if (enc_step != 0) {
                    lcd_off_timer_s = systimer_set_s(60);
                }
                else if (systimer_triggered_s(lcd_off_timer_s)) {
                    is_lcd_state_init = true;
                    lcd_state = LCD_STATE_OFF;
                }
                break;


            case LCD_STATE_MENU_ITEMS_LIST:
                if (is_lcd_state_init) {
                    encoder_clear_all_events();
                    enc_step = 0;
                }

                if ((enc_step != 0) || is_lcd_state_init) {
                    lcd_off_timer_s = systimer_set_s(60);

                    if ((enc_step > 0) && (lcd_menu_curr_item < (LCD_MENU_ITEMS_QTY - 1))) lcd_menu_curr_item++;
                    else if ((enc_step < 0) && (lcd_menu_curr_item > 0)) lcd_menu_curr_item--;
                    else if (!is_lcd_state_init) break;

                    is_lcd_state_init = false;

                    if (lcd_menu_curr_item > 0) ssd1306_print_str(lcd_menu_items[lcd_menu_curr_item - 1], 1, 12, 0);
                    else ssd1306_print_str(lcd_menu_empty_str, 1, 12, 0);
                    for (i = 0; i < 3; i++) {
                        y = lcd_menu_curr_item + i;
                        if (y < LCD_MENU_ITEMS_QTY) ssd1306_print_str(lcd_menu_items[y], 1, 12, (8 * (i + 1)));
                        else ssd1306_print_str(lcd_menu_empty_str, 1, 12, (8 * (i + 1)));
                    }
                    ssd1306_print_simw('>', 1, 0, 8);
                    ssd1306_print_simw(' ', 1, 6, 8);
                }
                else if (encoder_is_press_event()) {
                    is_lcd_state_init = true;
                    ssd1306_print_simw('>', 1, 6, 8);
                    ssd1306_print_simw(' ', 1, 0, 8);
                    // show stat
                    if (lcd_menu_curr_item == 0) {
                        lcd_state = LCD_STATE_SHOW_STAT;
                    }
                    // clear stat h
                    else if (lcd_menu_curr_item == 1) {
                        statistics_humidity_min = 100;
                        statistics_humidity_max = 0;
                    }
                    // clear stat t
                    else if (lcd_menu_curr_item == 2) {
                        statistics_time_common_minutes = 0;
                        statistics_time_heater_en_minutes = 0;
                        statistics_time_vent_en_minutes = 0;
                    }
                    // show log
                    else if (lcd_menu_curr_item == 3) {
                        lcd_state = LCD_STATE_SHOW_LOG;
                    }
                    // clear log
                    else if (lcd_menu_curr_item == 4) {
                        ssd1306_clear();
                        ssd1306_print_str("Clear proc", 2, 0, 8);
                        lcd_update_timer_s = systimer_set_s(2);
                        for (log_process_eeprom_index = EEPROM_LOG_BASE_ADDR; log_process_eeprom_index < (EEPROM_LOG_BASE_ADDR + EEPROM_LOG_SIZE); log_process_eeprom_index++) {
                            eeprom_driver_write_8(log_process_eeprom_index, 0xFF);
                        }
                        log_process_eeprom_index = EEPROM_LOG_BASE_ADDR;
                        is_log_full = false;
                        log_process_timer_s = systimer_set_s(60);
                        log_process_minutes_cnt = 0;  
                        log_process_humidity_avg = 0;
                        log_process_heat_vent_states = 0;
                        while (!systimer_triggered_s(lcd_update_timer_s)) ;
                        ssd1306_clear();
                        lcd_menu_curr_item = 0;
                    }
                    else {
                        lcd_state = LCD_STATE_MENU_ITEM_VALUE;
                    }
                }
                else if (encoder_is_long_press_event() || systimer_triggered_s(lcd_off_timer_s)) {
                    is_lcd_state_init = true;
                    lcd_state = LCD_STATE_MAIN_SCREEN;
                }
                break;


            case LCD_STATE_MENU_ITEM_VALUE:
                if (is_lcd_state_init) {
                    encoder_clear_all_events();
                    enc_step = 0;
                    ssd1306_print_str(lcd_menu_empty_str, 1, 12, 0);
                    ssd1306_print_str(lcd_menu_empty_str, 1, 12, 16);
                    ssd1306_print_str(lcd_menu_empty_str, 1, 12, 24);

                    ssd1306_print_simw('>', 1, 0, 8);
                    ssd1306_print_simw(' ', 1, 6, 8);

                    eeprom_driver_read_8((lcd_menu_curr_item - LCD_MENU_CONFIG_ITEMS_OFFSET), &y);
                }

                if ((enc_step != 0) || is_lcd_state_init) {
                    is_lcd_state_init = false;
                    lcd_off_timer_s = systimer_set_s(60);

                    if (enc_step > 0) {
                        if ((0xFF - y) > enc_step) y += enc_step;
                        else y = 0xFF;
                    }
                    else {
                        enc_step = -enc_step;
                        if ((enc_step < y) && ((y - enc_step) >= 1)) y -= enc_step;
                        else y = 1;
                    }
                    
                    ssd1306_print_digit(y, 3, 1, 54, 24);
                }
                else if (encoder_is_press_event()) {
                    eeprom_driver_read_8((lcd_menu_curr_item - LCD_MENU_CONFIG_ITEMS_OFFSET), &i);
                    if (i != y) {
                        eeprom_driver_write_8((lcd_menu_curr_item - LCD_MENU_CONFIG_ITEMS_OFFSET), y);
                    }
                    is_lcd_state_init = true;
                    lcd_state = LCD_STATE_MENU_ITEMS_LIST;
                }
                else if (encoder_is_long_press_event()) {
                    is_lcd_state_init = true;
                    lcd_state = LCD_STATE_MENU_ITEMS_LIST;
                }
                else if (systimer_triggered_s(lcd_off_timer_s)) {
                    is_lcd_state_init = true;
                    lcd_state = LCD_STATE_MAIN_SCREEN;
                }
                break;


            case LCD_STATE_SHOW_LOG:
                if (is_lcd_state_init) {
                    is_lcd_state_init = false;

                    ssd1306_clear();
                    ssd1306_print_str("80", 1, 0, 0);
                    ssd1306_print_str("57", 1, 0, 8);
                    ssd1306_print_str("T", 1, 0, 24);
                    ssd1306_print_digit(device_config.log_period_m, 2, 1, 6, 24);

                    i = 17;
                    for (i_int = EEPROM_LOG_BASE_ADDR; i_int < log_process_eeprom_index; ) {
                        eeprom_driver_read_8(i_int, &lcd_show_log_humidity);
                        i_int++;
                        eeprom_driver_read_8(i_int, &y);
                        i_int++;

                        if (lcd_show_log_humidity > 80) lcd_show_log_humidity = 80;
                        else if (lcd_show_log_humidity < 57) lcd_show_log_humidity = 57;
                        lcd_show_log_humidity -= 57;
                        lcd_show_log_humidity = 23 - lcd_show_log_humidity;
                        z = 1 << (lcd_show_log_humidity - (lcd_show_log_humidity & 0xF8));
                        ssd1306_set_img(z, i, lcd_show_log_humidity);

                        ssd1306_set_img(y, i, 24);

                        i++;
                    }

                    encoder_clear_all_events();
                }

                if (encoder_is_long_press_event()) {
                    is_lcd_state_init = true;
                    ssd1306_clear();
                    lcd_state = LCD_STATE_MENU_ITEMS_LIST;
                }
                break;


            case LCD_STATE_SHOW_STAT:
                if (is_lcd_state_init) {
                    is_lcd_state_init = false;

                    ssd1306_clear();
                    ssd1306_print_str("Tc: ", 1, 0, 0);
                    ssd1306_print_digit(statistics_time_common_minutes, 5, 1, 24, 0);
                    ssd1306_print_str("Th: ", 1, 0, 8);
                    ssd1306_print_digit(statistics_time_heater_en_minutes, 5, 1, 24, 8);
                    ssd1306_print_str("Tv: ", 1, 0, 16);
                    ssd1306_print_digit(statistics_time_vent_en_minutes, 5, 1, 24, 16);

                    ssd1306_print_str("| Hmin: ", 1, 60, 0);
                    ssd1306_print_digit(statistics_humidity_min, 3, 1, 102, 0);
                    ssd1306_print_str("| Hmax: ", 1, 60, 8);
                    ssd1306_print_digit(statistics_humidity_max, 3, 1, 102, 8);

                    encoder_clear_all_events();
                }
                else if (encoder_is_long_press_event()) {
                    is_lcd_state_init = true;
                    ssd1306_clear();
                    lcd_state = LCD_STATE_MENU_ITEMS_LIST;
                }
                break;


            case LCD_STATE_OFF:
                if (is_lcd_state_init) {
                    is_lcd_state_init = false;
                    encoder_clear_all_events();
                    enc_step = 0;
                    ssd1306_clear();
                    ssd1306_print_str("LCD sleep.", 2, 0, 8);
                    lcd_update_timer_s = systimer_set_s(2);
                }

                if ((enc_step != 0) || encoder_is_press_event() || encoder_is_long_press_event()) {
                    is_lcd_state_init = true;
                    lcd_state = LCD_STATE_MAIN_SCREEN;
                }
                else if (systimer_triggered_s(lcd_update_timer_s)) {
                    lcd_update_timer_s = systimer_set_s(65535);
                    ssd1306_clear();
                }
                break;
        }


        // Log process
        if (systimer_triggered_s(log_process_timer_s)) {
            ////log_process_timer_s = systimer_set_s(1);    //// For test !!!!
            log_process_timer_s = systimer_set_s(60);
            log_process_minutes_cnt++;

            log_process_humidity_avg += curr_humidity_pct;

            statistics_time_common_minutes++;
            if (is_heater_en) {
                log_process_heat_vent_states |= 0x80 >> (log_process_minutes_cnt >> 3);
                statistics_time_heater_en_minutes++;
            }
            if (is_vent_en) {
                log_process_heat_vent_states |= 0x08 >> (log_process_minutes_cnt >> 3);
                statistics_time_vent_en_minutes++;
            }

            // Overload
            if (statistics_time_common_minutes == 0) {
                statistics_time_heater_en_minutes = 0;
                statistics_time_vent_en_minutes = 0;
            }

            if (log_process_minutes_cnt >= device_config.log_period_m) {
                log_process_minutes_cnt = 0;

                if (log_process_eeprom_index < (EEPROM_LOG_BASE_ADDR + EEPROM_LOG_SIZE)) {
                    log_process_humidity_avg = log_process_humidity_avg / device_config.log_period_m;

                    eeprom_driver_write_8(log_process_eeprom_index, (uint8_t)log_process_humidity_avg);
                    log_process_eeprom_index++;
                    eeprom_driver_write_8(log_process_eeprom_index, log_process_heat_vent_states);
                    log_process_eeprom_index++;
                }
                else {
                    is_log_full = true;
                }
                log_process_humidity_avg = 0;
                log_process_heat_vent_states = 0;
            }
        }

    }

}




static void vent_en(void) {
    is_vent_en = true;
    is_vent_en_request = true;
}
static void vent_dis(void) {
    is_vent_en = false;
    is_vent_en_request = false;
    GPIOD_RESET(6);
}
static void heater_en(void) {
    is_heater_en = true;
    is_heater_en_request = true;
}
static void heater_dis(void) {
    is_heater_en = false;
    is_heater_en_request = false;
    GPIOD_RESET(7);
}




ISR(TIMER2_OVF_vect) {
    systimer_process();
}
