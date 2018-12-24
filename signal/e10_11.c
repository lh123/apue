#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>

#define BUFFSIZE 100

static void sig_intr(int signo);

int main(int argc, char *argv[]) {
    int n;
    int w;
    char buf[BUFFSIZE];
    int infd, outfd;
    struct sigaction act;
    struct rlimit fsize;

    if (argc != 3) {
        printf("usage: %s infile outfile\n", argv[0]);
        return -1;
    }

    if (getrlimit(RLIMIT_FSIZE, &fsize) < 0) {
        perror("getrlimit(RLIMIT_FSIZE) error");
        exit(-1);
    }
    fsize.rlim_cur = 1024;
    if (setrlimit(RLIMIT_FSIZE, &fsize) < 0) {
        perror("setrlimit(RLIMIT_FSIZE) error");
        exit(-1);
    }
    printf("RLIMIT_FSIZE: %ld\n", (long)fsize.rlim_cur);

    act.sa_handler = sig_intr;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    if (sigaction(SIGXFSZ, &act, NULL) < 0) {
        perror("sigaction(SIGXFSZ) error");
        exit(-1);
    }

    if ((infd = open(argv[1], O_RDONLY)) < 0) {
        printf("open '%s' error: %s\n", argv[1], strerror(errno));
        exit(-1);
    }

    if ((outfd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH)) < 0) {
        printf("open '%s' error: %s\n", argv[2], strerror(errno));
        exit(-1);
    }

    while ((n = read(infd, buf, BUFFSIZE)) > 0) {
        if ((w = write(outfd, buf, n)) != n) {
            printf("write error '%d'\n", w);
            break;
        }
    }

    if (w != n) {
        write(outfd, buf + w, n - w);
    }
}

static void sig_intr(int signo) {
    printf("\ncaught SIGXFSZ\n");
}
