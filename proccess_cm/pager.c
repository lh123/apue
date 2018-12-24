#include <apue.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define USE_FCNTL_DUP
#define USE_POPEN

#define DEF_PAGER "/bin/more"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        err_quit("usage: a.out <pathname>");
    }

    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        err_sys("can't open %s", argv[1]);
    }
#if defined(USE_POPEN)
    FILE *fpout = popen(DEF_PAGER, "w");
    if (fpout == NULL) {
        err_sys("popen error");
    }
    char line[MAXLINE];
    while (fgets(line, MAXLINE, fp) != NULL) {
        if (fputs(line, fpout) == EOF) {
            err_sys("fputs error to pipe");
        }
    }

    if (ferror(fp)) {
        err_sys("fgets error");
    }
    if (pclose(fpout) == -1) {
        err_sys("pclose error");
    }
#else
    int fd[2];
    if (pipe(fd) < 0) {
        err_sys("pipe error");
    }

    pid_t pid = fork();
    if (pid < 0) {
        err_sys("fork error");
    } else if (pid > 0) {
        // parent
        close(fd[0]);
        char line[MAXLINE];
        while (fgets(line, MAXLINE, fp) != NULL) {
            int n = strlen(line);
            if (write(fd[1], line, n) != n) {
                err_sys("write error to pipe");
            }
        }
        if (ferror(fp)) {
            err_sys("fgets error");
        }
        close(fd[1]);

        if (waitpid(pid, NULL, 0) < 0) {
            err_sys("waitpid error");
        }
        exit(0);
    } else {
        // child
        close(fd[1]);
        if (fd[0] != STDIN_FILENO) {
#if defined(USE_FCNTL_DUP)
            close(STDIN_FILENO);
            if (fcntl(fd[0], F_DUPFD, STDIN_FILENO) != STDIN_FILENO) {
                err_sys("fcntl F_DUPFD error to stdin");
            }
#else
            if (dup2(fd[0], STDIN_FILENO) != STDIN_FILENO) {
                err_sys("dup2 error to stdin");
            }
#endif
        }
        close(fd[0]);

        char *pager = getenv("PAGER");
        if (pager == NULL) {
            pager = DEF_PAGER;
        }

        char *argv0 = strrchr(pager, '/');
        if (argv0 != NULL) {
            argv0++;
        } else {
            argv0 = pager;
        }

        if (execl(pager, argv0, NULL) < 0) {
            err_sys("execl error for %s", pager);
        }
        exit(0);
    }
#endif
}
