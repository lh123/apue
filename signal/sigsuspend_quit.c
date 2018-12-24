#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

static volatile sig_atomic_t quitflag;

static void sig_int(int signo);
static void err_sys(const char *str);
static void (* my_signal(int sig, void (*func)(int signo)))(int signo);

int main(void) {
    sigset_t newmask, oldmask, zeromask;

    if (my_signal(SIGINT, sig_int) == SIG_ERR) {
        err_sys("signal(SIGINT) error");
    }
    if (my_signal(SIGQUIT, sig_int) == SIG_ERR) {
        err_sys("signal(SIGQUIT) error");
    }

    sigemptyset(&zeromask);
    sigemptyset(&newmask);
    sigaddset(&newmask, SIGQUIT);

    if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0) {
        err_sys("SIG_BLCOK error");
    }

    while (quitflag == 0) {
        sigsuspend(&zeromask);
    }

    quitflag = 0;

    if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {
        err_sys("SIG_SETMASK error");
    }
    return 0;
}

static void sig_int(int signo) {
    if (signo == SIGINT) {
        printf("\ninterrupt\n");
    } else if (signo == SIGQUIT) {
        quitflag = 1;
    }
}

static void err_sys(const char *str) {
    perror(str);
    exit(-1);
}

static void (* my_signal(int sig, void (*func)(int signo)))(int signo) {
    struct sigaction act, oact;

    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    if (sigaction(sig, &act, &oact) < 0) {
        return SIG_ERR;
    }
    return oact.sa_handler;
}
