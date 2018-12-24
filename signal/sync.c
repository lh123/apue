#if defined(__INTELLISENSE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

typedef void Sigfunc(int signo);

static volatile sig_atomic_t sigflag;
static sigset_t newmask, oldmask, zeromask;

static void sig_usr(int signo);
Sigfunc *my_signal(int sig, Sigfunc *func);
__attribute__((noreturn)) static void err_sys(const char *str);
static void charatatime(char *str);

static void sync_init();
static void wake_parent();
static void wake_child();
static void wait_parent();
static void wait_child();

int main(void) {
    sync_init();
    pid_t pid = fork();
    if (pid < 0) {
        err_sys("fork error");
    } else if (pid == 0) {
        wait_parent();
        charatatime("output from child\n");
        charatatime("child exit\n");
        wake_parent(getppid());
    } else {
        charatatime("output from parent\n");
        wake_child(pid);
        wait_child();
        charatatime("parent exit\n");
    }
}

static void sig_usr(int signo) {
    sigflag = 1;
}

Sigfunc *my_signal(int sig, Sigfunc *func) {
    struct sigaction act, oact;

    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    if (sigaction(sig, &act, &oact) < 0) {
        return SIG_ERR;
    }
    return oact.sa_handler;
}

static void err_sys(const char *str) {
    perror(str);
    exit(-1);
}

static void charatatime(char *str) {
    setbuf(stdout, NULL);
    char *ptr;
    int c;
    for (ptr = str; (c = *ptr) != 0; ++ptr) {
        putc(c, stdout);
    }
}

static void sync_init() {
    if (my_signal(SIGUSR1, sig_usr) == SIG_ERR) {
        err_sys("signal(SIGUSR1) error");
    }
    sigemptyset(&zeromask);
    sigemptyset(&newmask);
    sigaddset(&newmask, SIGUSR1);

    if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0) {
        err_sys("SIG_BLOCK error");
    }
}

static void wake_parent(pid_t pid) {
    kill(pid, SIGUSR1);
}

static void wake_child(pid_t pid) {
    kill(pid, SIGUSR1);
}

static void wait_parent() {
    while (sigflag == 0) {
        sigsuspend(&zeromask);
    }
    sigflag = 0;

    if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {
        err_sys("SIG_SETMASK error");
    }
}

static void wait_child() {
    while (sigflag == 0) {
        sigsuspend(&zeromask);
    }
    sigflag = 0;

    if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {
        err_sys("SIG_SETMASK error");
    }
}

