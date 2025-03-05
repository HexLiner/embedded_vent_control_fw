#ifndef _SYSTIMER_H_
#define _SYSTIMER_H_

#include <stdint.h>
#include <stdbool.h>


#define SYSTIMER_PROCESS_CALLS_IN_1MS (4)
#define SYSTIMER_PROCESS_CALLS_IN_1S  (SYSTIMER_PROCESS_CALLS_IN_1MS * 1000)


typedef uint32_t timer_t;


extern void systimer_process(void);

extern timer_t systimer_set_ms(uint32_t time_ms);
extern bool systimer_triggered_ms(timer_t timeout);
extern void systimer_delay_ms(uint32_t time_ms);

extern timer_t systimer_set_s(uint32_t time_s);
extern bool systimer_triggered_s(timer_t timeout);
extern void systimer_delay_s(uint32_t time_s);


#endif // _SYSTIMER_H_
