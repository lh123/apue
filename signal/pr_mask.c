#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

void pr_mask(const char *str);

int main(void) {
    pr_mask("main");
}

// 打印当前进程的信号屏蔽字中的信号名
void pr_mask(const char *str) {
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
