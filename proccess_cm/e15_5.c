#include <apue.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static void sig_pipe(int signo);

int main(void) {
    if (signal(SIGPIPE, sig_pipe) == SIG_ERR) {
        err_sys("signal error");
    }
    
    int fd1[2], fd2[2];
    if (pipe(fd1) < 0 || pipe(fd2) < 0) {
        err_sys("pipe error");
    }

    pid_t pid;
    if ((pid = fork()) < 0) {
        err_sys("fork error");
    } else if (pid == 0) {
        // child
        close(fd2[0]);
        close(fd1[1]);

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
    } else {
        close(fd1[0]);
        close(fd2[1]);

        FILE *c_out, *c_in;
        if ((c_in = fdopen(fd1[1], "w")) == NULL) {
            err_sys("fdopen error for child stdin");
        }
        if ((c_out = fdopen(fd2[0], "r")) == NULL) {
            err_sys("fdopen error for child stdout");
        }
        setvbuf(c_in, NULL, _IOLBF, 0);
        char line[MAXLINE];
        while (fgets(line, MAXLINE, stdin) != NULL) {
            if (fputs(line, c_in) == EOF) {
                err_sys("fputs error on child stdin");
            }
            if (fgets(line, MAXLINE, c_out) == NULL) {
                err_sys("child close pipe");
            }
            if (fputs(line, stdout) == EOF) {
                err_sys("fputs error on stdout");
            }
        }
        if (ferror(stdin)) {
            err_sys("fgets error on stdin");
        }
        exit(0);
    }
}

static void sig_pipe(int signo) {
    printf("SIGPIPE caughted\n");
    exit(1);
}
