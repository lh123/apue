#include "soft_timer.h"
#include <stdio.h>
#include <unistd.h>

static struct timer timers[MAX_TIMERS];
static struct timer *current_timer;

static void sig_alrm_handler(int signo);

//更新定时器，并且设置current_timer
static void update_timer(void);

void timer_init(void) {
    int i;
    for (i = 0; i < MAX_TIMERS; i++) {
        timers[i].used = 0;
    }

    struct sigaction act, oact;
    act.sa_flags = 0;
    sigfillset(&act.sa_mask);
    act.sa_handler = sig_alrm_handler;

    if (sigaction(SIGALRM, &act, &oact) < 0) {
        perror("sigaction error");
        return;
    }
}

struct timer *create_timer(time_t time_s, timer_handler *handler, void *args) {
    int i;
    for (i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].used == 0) {
            break;
        }
    }
    if (i == MAX_TIMERS) {
        return NULL;
    }

    timers[i].used = 1;
    timers[i].tr_handler = handler;
    timers[i].args = args;
    timers[i].time_s = time_s;
    timers[i].set_time_s = time(NULL);

    if (current_timer == NULL) {
        current_timer = &timers[i];
        alarm(current_timer->time_s);
    } else if (timers[i].set_time_s + timers[i].time_s < current_timer->time_s + current_timer->set_time_s) {
        update_timer();
    }
    return &timers[i];
}

void destory_timer(struct timer *timer) {
    if (timer->used == 0) {
        return;
    }
    timer->used = 0;
    if (timer == current_timer) {
        update_timer();
    }
}

static void update_timer(void) {
    int i;
    current_timer = NULL;
    for (i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].used == 0) {
            continue;
        }
        if (current_timer == NULL) {
            current_timer = &timers[i];
        } else if (timers[i].time_s + timers[i].set_time_s < current_timer->time_s + current_timer->set_time_s) {
            current_timer = &timers[i];
        }
    }
    if (current_timer != NULL) {
        alarm(current_timer->set_time_s + current_timer->time_s - time(NULL));
    }
}

static void sig_alrm_handler(int signo) {
    current_timer->tr_handler(current_timer->args);
    current_timer->used = 0;
    update_timer();
}
