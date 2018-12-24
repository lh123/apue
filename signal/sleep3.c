#if defined(__INTELLISENSE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static void sig_alrm(int signo) {
    // 什么都不做，仅仅返回并唤醒进程
}

static unsigned int my_sleep(unsigned int seconds);

int main(void) {
    printf("start\n");
    my_sleep(5);
    printf("end\n");
}

static unsigned int my_sleep(unsigned int seconds) {
    struct sigaction newact, oldact;
    sigset_t newmask, oldmask, suspmask;
    unsigned int unslept;

    newact.sa_handler = sig_alrm;
    sigemptyset(&newact.sa_mask);
    newact.sa_flags = 0;
    sigaction(SIGALRM, &newact, &oldact);

    // 阻塞SIGALRM并且保存当前的信号屏蔽字
    sigemptyset(&newmask);
    sigaddset(&newmask, SIGALRM);
    sigprocmask(SIG_BLOCK, &newmask, &oldmask);

    alarm(seconds);
    suspmask = oldmask;

    sigdelset(&suspmask, SIGALRM);

    sigsuspend(&suspmask);

    unslept = alarm(0);

    sigaction(SIGALRM, &oldact, NULL);

    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    return unslept;
}
