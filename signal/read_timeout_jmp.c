#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>

static jmp_buf env_alrm;
static void sig_alrm(int signal);
static long get_maxline(void);

int main(void) {
    int n;
    char *line;

    line = malloc(get_maxline());
    printf("maxline: %ld\n", get_maxline());
    if (signal(SIGALRM, sig_alrm) == SIG_ERR) {
        perror("signal error");
        exit(-1);
    }

    if (setjmp(env_alrm) != 0) {
        printf("read timeout\n");
        exit(-1);
    }
    alarm(10);
    if ((n = read(STDIN_FILENO, line, get_maxline())) < 0) {
        perror("read error");
        exit(-1);
    }
    alarm(0);
    
    write(STDOUT_FILENO, line, n);
    return 0;
}

static long get_maxline(void) {
    static long max_line = -1;
    if (max_line == -1) {
        if ((max_line = sysconf(_SC_LINE_MAX)) < 0) {
            perror("sysconf error");
            max_line = 4096;
        }
    }
    return max_line;
}

static void sig_alrm(int signal) {
    longjmp(env_alrm, 1);
}
