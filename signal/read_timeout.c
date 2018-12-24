#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>

static void sig_alrm(int signo);

int main(void) {
    int n;
    long max_line;
    if ((max_line = sysconf(_SC_LINE_MAX)) < 0) {
        perror("sysconf error\n");
        exit(-1);
    }
    printf("maxline = %ld\n", max_line);
    char *line = malloc(max_line);

    if (signal(SIGALRM, sig_alrm) == SIG_ERR) {
        perror("signal error");
        exit(-1);
    }

    alarm(10);
    if ((n = read(STDIN_FILENO, line, max_line)) < 0) { // linux 中read是自动重启的，因此SIGALRM无法打断read
        perror("read error\n");
        exit(-1);
    }
    alarm(0);

    write(STDOUT_FILENO, line, n);
    return 0;
}

static void sig_alrm(int signo) {
    // printf("received sig_alrm\n");
    // nothing to do, just return to interrupt the read
}
