#if defined(__INTELLISENSE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

typedef void Sigfunc(int signo);

static int my_system(const char *cmdstring);
static Sigfunc *my_signal(int sig, Sigfunc *func);
static void sig_intr(int signo);
static void sig_quit(int signo);

int main(void) {
    if (my_signal(SIGINT, sig_intr) == SIG_ERR) {
        perror("signal(SIGINT) error");
        exit(-1);
    }
    if (my_signal(SIGQUIT, sig_quit) == SIG_ERR) {
        perror("signal(SIGQUIT) error");
        exit(-1);
    }
    int status;
    if ((status =  my_system("cat")) < 0) {
        perror("system(\"cat\") error");
        exit(-1); 
    }
    printf("status = %d\n", status);
}

static int my_system(const char *cmdstring) {
    pid_t pid;
    int status;
    struct sigaction ignore, saveintr, savequit;
    sigset_t chldmask, savemask;

    if (cmdstring == NULL) {
        return 1;
    }

    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    ignore.sa_flags = 0;
    if (sigaction(SIGINT, &ignore, &saveintr) < 0) {
        return -1;
    }
    if (sigaction(SIGQUIT, &ignore, &savequit) < 0) {
        return -1;
    }
    sigemptyset(&chldmask);
    sigaddset(&chldmask, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &chldmask, &savemask) < 0) {
        return -1;
    }

    if ((pid = fork()) < 0) {
        status = -1;
    } else if (pid == 0) {
        sigaction(SIGINT, &saveintr, NULL);
        sigaction(SIGQUIT, &savequit, NULL);
        sigprocmask(SIG_SETMASK, &savemask, NULL);
        execl("/bin/sh", "sh", "-c", cmdstring, NULL);
        exit(127); //execl error
    } else {
        while (waitpid(pid, &status, 0) < 0) {
            if (errno == EINTR) {
                status = -1;
                break;
            }
        }
    }

    if (sigaction(SIGINT, &saveintr, NULL) < 0) {
        return -1;
    }
    if (sigaction(SIGQUIT, &savequit, NULL) < 0) {
        return -1;
    }
    if (sigprocmask(SIG_SETMASK, &savemask, NULL) < 0) {
        return -1;
    }
    return status;
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

static void sig_intr(int signo) {
    printf("\ncaught SIGINT\n");
    fflush(stdout);
}

static void sig_quit(int signo) {
    printf("\ncaught SIGQUIT\n");
    fflush(stdout);
}
