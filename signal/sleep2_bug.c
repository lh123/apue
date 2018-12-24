#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <time.h>
#include <unistd.h>

static jmp_buf env_alrm;

static void pr_time(void);
unsigned int sleep2(unsigned int seconds);

int main(void) {
    printf("before sleep\n");
    pr_time();
    putchar('\n');
    sleep2(2);
    printf("after sleep\n");
    pr_time();
    putchar('\n');
}

static void sig_alrm(int signo) {
    longjmp(env_alrm, 1);
}

unsigned int sleep2(unsigned int seconds) {
    if (signal(SIGALRM, sig_alrm) == SIG_ERR) {
        return seconds;
    }
    if (setjmp(env_alrm) == 0) {
        alarm(seconds);
        pause();
    }
    return alarm(0);
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
