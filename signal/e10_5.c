#include "soft_timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static sig_atomic_t quit_flag;

static void pr_timer(void);
static void timer_hander1(void *args);
static void timer_hander2(void *args);
static void timer_hander3(void *args);
static void timer_hander4(void *args);

int main(void) {

    timer_init();
    printf("start time\n");
    pr_timer();
    printf("\n");
    struct timer * __attribute__((unused)) t8 =create_timer(8, timer_hander4, "time: 8s");
    struct timer * __attribute__((unused)) t6 = create_timer(6, timer_hander3, "time: 6s");
    struct timer * __attribute__((unused)) t4 = create_timer(4, timer_hander2, "time: 4s");
    struct timer * __attribute__((unused)) t2 = create_timer(2, timer_hander1, "time: 2s");

    while (quit_flag == 0) {
        pause();
    }
}

static void timer_hander1(void *args) {
    printf("\n%s\n", (char *)args);
    pr_timer();
    printf("\n");
}

static void timer_hander2(void *args) {
    printf("\n%s\n", (char *)args);
    pr_timer();
    printf("\n");
}

static void timer_hander3(void *args) {
    printf("\n%s\n", (char *)args);
    pr_timer();
    printf("\n");
}

static void timer_hander4(void *args) {
    printf("\n%s\n", (char *)args);
    pr_timer();
    printf("\n");
    quit_flag = 1;
}

static void pr_timer(void) {
    time_t current_time = time(NULL);
    char str_time[60];
    strftime(str_time, sizeof(str_time), "%H:%M:%S", localtime(&current_time));
    printf("%s", str_time);
}
