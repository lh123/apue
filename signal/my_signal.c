#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

typedef void Sigfunc(int);

Sigfunc *my_signal(int signo, Sigfunc *func);
static void sig_intr(int signo);

int main(void) {
    if (my_signal(SIGINT, sig_intr) == SIG_ERR) {
        perror("my_signal error\n");
        exit(-1);
    }
    printf("waiting SIGINT\n");
    pause();
}

static void sig_intr(int signo) {
    printf("\nReceived SIGINIT\n");
}

Sigfunc *my_signal(int signo, Sigfunc *func) {
    struct sigaction act, oact;
    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    if (signo == SIGALRM) {
#if defined(SA_INTERRUPT)
        act.sa_flags |= SA_INTERRUPT;
#endif
    } else {
        act.sa_flags |= SA_RESTART;
    }

    if (sigaction(signo, &act, &oact) < 0) {
        return SIG_ERR;
    }
    return oact.sa_handler;
}
