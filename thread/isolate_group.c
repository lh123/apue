#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

static void sig_hub(int signo) {
    printf("SIGHUB received, pid = %ld\n", (long)getpid());
}

static void pr_ids(const char *name) {
    printf("%s: pid = %ld, ppid = %ld, pgrp = %ld, tpgrp = %ld\n", name, (long)getpid(), (long)getppid(), (long)getpgrp(), (long)tcgetpgrp(STDOUT_FILENO));
    fflush(stdout);
}

int main(void) {
    pid_t pid;

    pr_ids("parent");
    if ((pid = fork()) < 0) {
        printf("fork error\n");
        exit(-1);
    } else if (pid == 0) {
        pr_ids("child");
        signal(SIGHUP, sig_hub);
        kill(getpid(), SIGTSTP);
        pr_ids("child");
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) {
            printf("read error %d on controlling TTY\n", errno);
        }
        exit(0);
    } else {
        sleep(5);
    }
}
