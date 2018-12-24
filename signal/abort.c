#if defined(__INTELLISENSE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

typedef void Sigfunc(int signo);

__attribute__((noreturn)) static void my_abort(void);
__attribute__((noreturn)) static void err_sys(const char *str);
static void sig_abrt(int signo);
static Sigfunc *my_signal(int sig, Sigfunc *func);

int main(void) {
    if (my_signal(SIGABRT, sig_abrt) == SIG_ERR) {
        err_sys("signal(SIGABRT) error");
    }

    printf("call abort\n");
    my_abort();
    printf("should not be here\n");
}

static void my_abort(void) {
    sigset_t mask;
    struct sigaction action;
    

    sigaction(SIGABRT, NULL, &action);
    if (action.sa_handler == SIG_IGN) {
        // SIGABRT 不能被忽略， 如果被忽略就重设为 Default.
        action.sa_handler = SIG_DFL;
        sigaction(SIGABRT, &action, NULL);
    }
    if (action.sa_handler == SIG_DFL) {
        // 如果SIGABRT是默认处理，那么下面的kill就会结束程序，因此在结束前，我们需要冲洗所有的流.
        fflush(NULL);
    }

    sigfillset(&mask);
    sigdelset(&mask, SIGABRT);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    kill(getpid(), SIGABRT);

    // If we're here, process caught SIGABRT and returned.
    fflush(NULL);
    action.sa_handler = SIG_DFL;
    sigaction(SIGABRT, &action, NULL);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    kill(getpid(), SIGABRT);

    // Should never be executed ...
    exit(1);
}

static void sig_abrt(int signo) {
    printf("\ncaughted SIGABRT\n");
}

static Sigfunc *my_signal(int sig, Sigfunc *func) {
    struct sigaction act, oact;

    act.sa_handler = func;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    if (sigaction(sig, &act, &oact) < 0) {
        return SIG_ERR;
    }
    return oact.sa_handler;
}

static void err_sys(const char *str) {
    perror(str);
    exit(-1);
}
