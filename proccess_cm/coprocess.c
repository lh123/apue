#include <apue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static void sig_pipe(int);

int main(void) {
    if (signal(SIGPIPE, sig_pipe) == SIG_ERR) {
        err_sys("signal error");
    }    

    int fd1[2], fd2[2];
    if (pipe(fd1) < 0 || pipe(fd2) < 0) {
        err_sys("pipe error");
    }

    /* parent           child
     * fd1[0]           fd1[1]
     * fd1[1] --------> fd1[0]
     * fd2[0] <-------- fd2[1]
     * fd2[1]           fd2[0]
     */
    pid_t pid = fork();
    if (pid < 0) {
        err_sys("fork error");
    } else if (pid > 0) {
        // parent
        close(fd1[0]);
        close(fd2[1]);
        char line[MAXLINE];
        while (fgets(line, MAXLINE, stdin) != NULL) {
            int n = strlen(line);
            if (write(fd1[1], line, n) != n) {
                err_sys("write error to pipe");
            }
            if ((n = read(fd2[0], line, MAXLINE - 1)) < 0) {
                err_sys("read error from pipe");
            }
            if (n == 0) {
                err_msg("child closed pipe");
                break;
            }
            line[n] = 0;
            if (fputs(line, stdout) == EOF) {
                err_sys("fputs error");
            }
        }

        if (ferror(stdin)) {
            err_sys("fgets error on stdin");
        }
        exit(0);
    } else {
        close(fd1[1]);
        close(fd2[0]);

        if (fd1[0] != STDIN_FILENO) {
            if (dup2(fd1[0], STDIN_FILENO) != STDIN_FILENO) {
                err_sys("dup2 error to stdin");
            }
            close(fd1[0]);
        }
        if (fd2[1] != STDOUT_FILENO) {
            if (dup2(fd2[1], STDOUT_FILENO) != STDOUT_FILENO) {
                err_sys("dup2 error to stdout");
            }
            close(fd2[1]);
        }
        if (execl("./add_filter", "add_filter", NULL) < 0) {
            err_sys("execl error");
        }
        exit(0);
    }
}

static void sig_pipe(int signo) {
    printf("SIGPIE caught\n");
    exit(1);
}
