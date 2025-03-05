#include "systimer.h"
#include <stdint.h>
#include <stdbool.h>

static volatile uint32_t systimer_int_counter_ms = 0;
static volatile uint32_t systimer_int_counter_s = 0;


void systimer_process(void) {
    static uint8_t ms_div_cnt = 0;
    static uint16_t s_div_cnt = 0;
    

    ms_div_cnt++;
    if (ms_div_cnt >= SYSTIMER_PROCESS_CALLS_IN_1MS) {
        systimer_int_counter_ms++;
        ms_div_cnt = 0;
    }

    s_div_cnt++;
    if (s_div_cnt >= SYSTIMER_PROCESS_CALLS_IN_1S) {
        systimer_int_counter_s++;
        s_div_cnt = 0;
    }
}


timer_t systimer_set_ms(uint32_t time_ms) {
    return (systimer_int_counter_ms + time_ms);
}


bool systimer_triggered_ms(timer_t timeout) {
    return (systimer_int_counter_ms >= timeout);
}


void systimer_delay_ms(uint32_t time_ms) {
    timer_t timer;

    timer = systimer_set_ms(time_ms);
    while (!systimer_triggered_ms(timer)) ;
}


timer_t systimer_set_s(uint32_t time_s) {
    return (systimer_int_counter_s + time_s);
}


bool systimer_triggered_s(timer_t timeout) {
    return (systimer_int_counter_s >= timeout);
}


void systimer_delay_s(uint32_t time_s) {
    timer_t timer;

    timer = systimer_set_s(time_s);
    while (!systimer_triggered_s(timer)) ;
}
