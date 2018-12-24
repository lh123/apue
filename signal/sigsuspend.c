#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

static void pr_mask(const char *str);
static void sig_int(int signo);
static void (* my_signal(int signo, void (*func)(int signo)))(int signo);

int main(void) {
    sigset_t newmask, oldmask, waitmask;

    pr_mask("program start: ");
    
    if (my_signal(SIGINT, sig_int) == SIG_ERR) {
        perror("signal error");
        exit(-1);
    }

    sigemptyset(&waitmask);
    sigaddset(&waitmask, SIGUSR1);
    sigemptyset(&newmask);
    sigaddset(&newmask, SIGINT);

    // Block SIGINT and save current signal mask.
    if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0) {
        perror("sigpromask error");
        exit(-1);
    }

    // Critical region of code.
    pr_mask("in critical region: ");

    // Pause, allowing all signal except SIGUSR1.
    if (sigsuspend(&waitmask) != -1) {
        perror("sigsuspend error");
        exit(-1);
    }

    pr_mask("after return from sigsuspend: ");

    // Reset signal mask which unblocks SIGINT
    if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {
        perror("SIG_SETMASK error");
        exit(-1);
    }

    pr_mask("program exit: ");
}

static void sig_int(int signo) {
    pr_mask("\nin sig_int: ");
}

static void pr_mask(const char *str) {
    sigset_t sigset;
    int errno_save;

    errno_save = errno;

    if (sigprocmask(0, NULL, &sigset) < 0) {
        perror("sigprocmask error");
    } else {
        printf("%s", str);
        if (sigismember(&sigset, SIGINT)) {
            printf(" SIGINT");
        }
        if (sigismember(&sigset, SIGALRM)) {
            printf(" SIGALRM");
        }
        if (sigismember(&sigset, SIGQUIT)) {
            printf(" SIGQUIT");
        }
        if (sigismember(&sigset, SIGUSR1)) {
            printf(" SIGUSR1");
        }
        if (sigismember(&sigset, SIGUSR2)) {
            printf(" SIGUSR2");
        }
        printf("\n");
    }

    errno = errno_save;
}

static void (* my_signal(int signo, void (*func)(int signo)))(int signo) {
    struct sigaction act, oact;

    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    if (sigaction(signo, &act, &oact) < 0) {
        return SIG_ERR;
    }
    return oact.sa_handler;
}
