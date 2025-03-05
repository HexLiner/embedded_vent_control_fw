

typedef enum {
    VENT_STATE_DISABLE,
    VENT_STATE_ENABLE,
} vent_state_t;

typedef enum {
    HEATER_STATE_DISABLE,
    HEATER_STATE_ENABLE,
} heater_state_t;


#define MIN_HUMIDITY_PCT      (40)
#define MAX_HUMIDITY_PCT      (45)
#define MIN_TEMPERATURE       (20)
#define MAX_TEMPERATURE       (24)

#define MAX_HEATING_TIME_MS   (15 * 60 * 1000)
#define HEATING_PAUSE_TIME_MS (15 * 60 * 1000)
#define MAX_VENT_TIME_MS      (20 * 60 * 1000)
#define VENT_PAUSE_TIME_MS    (15 * 60 * 1000)

#define LOAD_ENABLE_PAUSE_TIME_MS (10 * 1000)

#define EEPROM_LOG_TEMPERATURE_BASE_ADDR (0x000F)
#define EEPROM_LOG_HUMIDITY_BASE_ADDR (0x00EF)
#define EEPROM_LOG_HEATER_EN_TIME_BASE_ADDR (0x01CF)
#define EEPROM_LOG_VENT_EN_TIME_BASE_ADDR (0x01EB)
#define LOG_TEMPERATURE_HUMIDITY_RECORDS_QTY (224)
#define LOG_HEATER_VENT_EN_TIME_RECORDS_QTY (28)


vent_en() {
    is_vent_en_request = true;
}
vent_dis() {
    is_vent_en_request = false;
    VENT_DIS;
}
heater_en() {
    is_heater_en_request = true;
}
heater_dis() {
    is_heater_en_request = false;
    HEATER_DIS;
}
bool is_alone_mode();
uint8_t get_humidity_pct();
int8_t get_temperature_deg();


bool is_heater_en_request, is_vent_en_request;
timer_t load_enable_timer;
uint8_t curr_humidity_pct;
int8_t curr_temperature_deg;
timer_t heating_timer, vent_timer;
heater_state_t heater_state;
vent_state_t vent_state;

timer_t lcd_update_timer;

timer_t log_process_timer;
bool is_log_full;
uint8_t log_process_half_hour_log_eeprom_index;
uint8_t log_process_four_hours_log_eeprom_index;
uint8_t log_process_minutes_cnt;
uint8_t log_process_half_hours_cnt;
uint8_t log_process_vent_en_minutes_cnt;
uint8_t log_process_heater_en_minutes_cnt;
uint8_t log_process_temperature_avg, log_process_humidity_avg;






// init
heater_dis();
heater_state = HEATER_STATE_DISABLE;
heating_timert = timer_set(30 * 1000);
vent_dis();
vent_state = VENT_STATE_DISABLE;
vent_timer = timer_set(30 * 1000);

log_process_timer = timer_set(60 * 1000);
is_log_full = ...
log_process_half_hour_log_eeprom_index = ...
log_process_four_hours_log_eeprom_index = ...
log_process_minutes_cnt = 0;
log_process_half_hours_cnt = 0;
log_process_vent_en_minutes_cnt = 0;
log_process_heater_en_minutes_cnt = 0;
log_process_temperature_avg = 0;
log_process_humidity_avg = 0;

lcd_update_timer = time_set(1000);
ssd1306_print_str("H=   %", 2, 0, 0);
ssd1306_print_str("T=   C", 2, 0, 16);




curr_humidity_pct = get_humidity_pct();
curr_temperature_deg = get_temperature_deg();


switch (heater_state) {
    case HEATER_STATE_DISABLE:
        if (timer_trig(heating_timer)) {
            if ((curr_temperature_deg < MIN_TEMPERATURE) || ((curr_humidity_pct > MAX_HUMIDITY_PCT) && (curr_temperature_deg < MAX_TEMPERATURE))) {
                heating_timer = timer_set(MAX_HEATING_TIME_S);
                heater_en();
                heater_state = HEATER_STATE_ENABLE;
            }
        }
        break;


    case HEATER_STATE_ENABLE:
        if (timer_trig(heating_timer) || (curr_temperature_deg > MAX_TEMPERATURE)) {
            heating_timer = timer_set(HEATING_PAUSE_TIME_S);
            heater_dis();
            heater_state = HEATER_STATE_DISABLE;
        }
        break;
}


switch (vent_state) {
    case VENT_STATE_DISABLE:
        if (timer_trig(vent_timer)) {
            if (curr_humidity_pct > MAX_HUMIDITY_PCT) {
                vent_timer = timer_set(MAX_VENT_TIME_S);
                vent_en();
                vent_state = VENT_STATE_ENABLE;
            }
        }

        if (!is_alone_mode()) {
            vent_en();
            vent_state = VENT_STATE_ENABLE;
        }
        break;


    case VENT_STATE_ENABLE:
        if ((timer_trig(vent_timer) || (curr_humidity_pct < MIN_HUMIDITY_PCT)) && !is_alone_mode()) {
            vent_timer = timer_set(VENT_PAUSE_TIME_S);
            vent_dis();
            vent_state = VENT_STATE_DISABLE;
        }
        break;
}


// LCD process
if (timer_trig(lcd_update_timer)) {
    lcd_update_timer = time_set(1000);

    // k = 2 -> 11 x 2 simw  (0, 12, 24, 36, 48, 60, 72, 84, 96, 108)
    ssd1306_print_str(..., 2, 24, 0);  // hum
    ssd1306_print_str(..., 2, 24, 16);  // temp

    if (is_log_full) ssd1306_print_str("LF", 2, 96, 0);
    else ssd1306_print_str("  ", 2, 96, 0);

    if (!is_alone_mode()) ssd1306_print_str("M", 2, 84, 16);
    else ssd1306_print_str(" ", 2, 84, 16);
    if (heater_state == HEATER_STATE_ENABLE) ssd1306_print_str("H", 2, 96, 16);
    else ssd1306_print_str(" ", 2, 96, 16);
    if (vent_state == VENT_STATE_ENABLE) ssd1306_print_str("V", 2, 108, 16);
    else ssd1306_print_str(" ", 2, 84, 16);
}


// Load enabe sequence
if (timer_trig(load_enable_timer)) {
    load_enable_timer = time_set(LOAD_ENABLE_PAUSE_TIME_MS);
    if (is_heater_en_request) {
        HEATER_EN;
        is_heater_en_request = false;
    }
    else if (is_vent_en_request) {
        VENT_EN;
        is_vent_en_request = false;
    }
    else {
        load_enable_timer = 0;
    }
}


// Log process
if (timer_trig(log_process_timer)) {
    log_process_timer = timer_set(60 * 1000);
    log_process_minutes_cnt++;

    log_process_humidity_avg += curr_humidity_pct;
    log_process_temperature_avg += curr_temperature_deg;

    if (heater_state == HEATER_STATE_ENABLE) log_process_heater_en_minutes_cnt++;
    if (vent_state == VENT_STATE_ENABLE) log_process_vent_en_minutes_cnt++;

    if (log_process_minutes_cnt >= 30) {
        log_process_minutes_cnt = 0;
        log_process_half_hours_cnt++;

        if (log_process_half_hour_log_eeprom_index < LOG_TEMPERATURE_HUMIDITY_RECORDS_QTY) {
            log_process_temperature_avg = log_process_temperature_avg / 30;
            log_process_humidity_avg = log_process_humidity_avg / 30;
            eeprom_write_u8((EEPROM_LOG_TEMPERATURE_BASE_ADDR + log_process_half_hour_log_eeprom_index), (uint8_t)log_process_temperature_avg);
            eeprom_write_u8((EEPROM_LOG_HUMIDITY_BASE_ADDR + log_process_half_hour_log_eeprom_index), (uint8_t)log_process_humidity_avg);

            log_process_temperature_avg = 0;
            log_process_humidity_avg = 0;
            log_process_half_hour_log_eeprom_index++;
        }
        else {
            is_log_full = true;
        }

        if (log_process_half_hours_cnt >= 8) {
            if (log_process_four_hours_log_eeprom_index < LOG_HEATER_VENT_EN_TIME_RECORDS_QTY) {
                eeprom_write_u8((EEPROM_LOG_HEATER_EN_TIME_BASE_ADDR + log_process_four_hours_log_eeprom_index), log_process_heater_en_minutes_cnt);
                eeprom_write_u8((EEPROM_LOG_VENT_EN_TIME_BASE_ADDR + log_process_four_hours_log_eeprom_index), log_process_vent_en_minutes_cnt);
                log_process_four_hours_log_eeprom_index++;
            }
            else {
                is_log_full = true;
            }

            log_process_half_hours_cnt = 0;
            log_process_heater_en_minutes_cnt = 0;
            log_process_vent_en_minutes_cnt = 0;
        }
    }
}
