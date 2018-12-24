#if !defined(SOFT_TIMER_H)
#define SOFT_TIMER_H

#if defined(__INTELLISENSE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <signal.h>
#include <time.h>

#define MAX_TIMERS 100

typedef void timer_handler(void *args);

struct timer {
    int used;
    time_t time_s;
    time_t set_time_s;
    timer_handler *tr_handler;
    void *args;
};

void timer_init(void);
struct timer *create_timer(time_t time, timer_handler *handler, void *args);
void destory_timer(struct timer *timer);

#endif