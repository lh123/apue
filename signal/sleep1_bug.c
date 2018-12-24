#if defined(__INTELLISENSE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

static int sleep1(unsigned int seconds);
static void pr_time(void);

int main(void) {
    printf("before sleep\n");
    pr_time();
    putchar('\n');
    sleep1(2);
    printf("after sleep\n");
    pr_time();
    putchar('\n');
}

static void sig_alarm(int signo) {
    printf("received SIGALRM\n");
}

static void pr_time(void) {
    struct timespec tim;
    if (clock_gettime(CLOCK_REALTIME, &tim) < 0) {
        perror("clock_gettime error");
        exit(-1);
    }
    struct tm *tmptr = localtime(&tim.tv_sec);
    if (tmptr == NULL) {
        perror("localtime error");
        exit(-1);
    }
    char timestr[60];
    strftime(timestr, sizeof(timestr), "%Y.%m.%d %H:%M:%S", tmptr);
    printf("%s", timestr);
}

static int sleep1(unsigned int seconds) {
    if (signal(SIGALRM, sig_alarm) == SIG_ERR) {
        return seconds;
    }
    alarm(seconds);
    pause();
    return alarm(0);
}
