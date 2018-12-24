/*
 * signal_intr 阻止被中断的系统调用自动重启
 */

#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <setjmp.h>
#include <unistd.h>

static jmp_buf env_alrm; 

typedef void Sigfunc(int signo);

Sigfunc *signal_intr(int signo, Sigfunc *func);
void sig_alarm(int signo);

int main(void) {
    if (signal_intr(SIGALRM, sig_alarm) == SIG_ERR) {
        perror("signal error");
        exit(-1);
    }

    if (setjmp(env_alrm) != 0) {
        printf("timeout\n");
        exit(-1);
    }

    alarm(5);

    int n;
    char line[4096];
    if ((n = read(STDIN_FILENO, line, sizeof(line))) < 0) {
        perror("read error");
        exit(-1);
    }
    alarm(0);
    write(STDOUT_FILENO, line, n);
    return 0;
}


void sig_alarm(int signo) {
    longjmp(env_alrm, 1);
}

Sigfunc *signal_intr(int signo, Sigfunc *func) {
    struct sigaction act, oact;
    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

#if defined(SA_INTERRUPT)
    act.sa_flags |= SA_INTERRUPT;
#endif
    if (sigaction(signo, &act, &oact) < 0) {
        return SIG_ERR;
    }
    return oact.sa_handler;
}

