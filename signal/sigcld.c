#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static void sig_cld(int signo);

int main(void) {
    pid_t pid;

    if (signal(SIGCLD, sig_cld) == SIG_ERR) {
        perror("signal error");
        _exit(-1);
    }
    if ((pid = fork()) < 0) {
        perror("fork error");
        _exit(-1);
    } else if (pid == 0) {
        sleep(2);
        _exit(0);
    }

    pause();
    return 0;
}

static void sig_cld(int signo) {
    pid_t pid;
    int status;

    printf("SIGCLD received\n");

    if (signal(SIGCLD, sig_cld) == SIG_ERR) {
        perror("signal error");
        _exit(-1);
    }

    if ((pid = wait(&status)) < 0) {
        perror("wait error");
    }

    printf("pid = %d\n", pid);
}
