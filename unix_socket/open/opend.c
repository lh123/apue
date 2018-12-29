#include "opend.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

char errmsg[MAXLINE];
int oflag;
char *pathname;

int main(void) {
    char buf[MAXLINE];
    for (;;) {
        int nread = read(STDIN_FILENO, buf, MAXLINE);
        if (nread < 0) {
            err_sys("read error on stream pipe");
        } else if (nread == 0) {
            break;
        }
        handle_request(buf, nread, STDOUT_FILENO);
    }
}

int buf_args(char *buf, int (*optfunc)(int, char **));

void handle_request(char *buf, int nread, int fd) {
    if(buf[nread - 1] != 0) {
        snprintf(errmsg, MAXLINE, "request not null terminated: %*.*s\n", nread, nread, buf);
        send_err(fd, -1, errmsg);
        return;
    }
    if (buf_args(buf, cli_args) < 0) {
        send_err(fd, -1, errmsg);
        return;
    }
    int newfd = open(pathname, oflag);
    if (newfd < 0) {
        snprintf(errmsg, MAXLINE, "can't open %s: %s\n", pathname, strerror(errno));
        send_err(fd, -1, errmsg);
        return;
    }
    if (send_fd(fd, newfd) < 0) {
        err_sys("send_fd error");
    }
    close(newfd);
}

#define MAXARGC 50
#define WHITE " \t\n"

/*
 * buf[] contains white-space-separated arguments. We convert it to an
 * argv-style array of pointers, and call the user's function (optfunc)
 * to process the array. We return -1 if there's a problem parsing buf,
 * else we return whatever optfunc() returns. Note that user's buf[]
 * array is modified (nulls placed after each token).
 */
int buf_args(char *buf, int (*optfunc)(int, char **)) {
    if (strtok(buf, WHITE) == NULL) {
        return -1;
    }
    int argc = 0;
    char *argv[MAXARGC];
    argv[0] = buf;
    char *ptr;
    while ((ptr = strtok(NULL, WHITE)) != NULL) {
        if (++argc >= MAXARGC - 1) {
            return -1;
        }
        argv[argc] = ptr;
    }
    argv[++argc] = NULL;
    return optfunc(argc, argv);
}

int cli_args(int argc, char **argv) {
    if (argc != 3 || strcmp(argv[0], CL_OPEN) != 0) {
        strncpy(errmsg, "usage: <pathname> <oflag>\n", sizeof(errmsg) - 1);
        errmsg[sizeof(errmsg) - 1] = 0;
        return -1;
    }
    pathname = argv[1];
    errno = 0;
    oflag = strtol(argv[2], NULL, 10);
    if (errno != 0) {
        strncpy(errmsg, "invalid oflag\n", sizeof(errmsg) - 1);
        errmsg[sizeof(errmsg) - 1] = 0;
        return -1;
    }
    return 0;
}
