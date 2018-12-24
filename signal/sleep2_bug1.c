/**
 * sleep2 会打断正在执行的中断
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <time.h>
#include <unistd.h>

static jmp_buf env_alrm;

unsigned int sleep2(unsigned int seconds);
static void sig_int(int signo);

int main(void) {
    unsigned int unslept;

    if (signal(SIGINT, sig_int) == SIG_ERR) {
        perror("signal error");
        exit(-1);
    }

    unslept = sleep2(1);
    printf("sleep2 returned: %u\n", unslept);
    return 0;
}

static void sig_int(int signo) {
    volatile int k;

    printf("\nsig_int starting\n");
    for (int i = 0; i < 300000; i++) {
        for (int j = 0; j < 4000; j++) {
            k = i * j;
        }
    }
    printf("sig_int finished\n");
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
