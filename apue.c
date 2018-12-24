#include "apue.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

static void err_doit(int errnoflag, int error, const char *fmt, va_list ap) {
    char buf[MAXLINE];

    vsnprintf(buf, MAXLINE - 1, fmt, ap);
    if (errnoflag) {
        snprintf(buf + strlen(buf), MAXLINE - strlen(buf) - 1,": %s", strerror(error));
    }
    strcat(buf, "\n");
    fflush(stdout);
    fputs(buf, stdout);
    fflush(stdout);
}

void err_ret(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    err_doit(1, errno, fmt, ap);
    va_end(ap);
}

void err_sys(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    err_doit(1, errno, fmt, ap);
    va_end(ap);
    exit(1);
}

void err_cont(int error, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    err_doit(1, error, fmt, ap);
    va_end(ap);
}

void err_exit(int error, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    err_doit(1, error, fmt, ap);
    va_end(ap);
    exit(1);
}

void err_dump(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    err_doit(1, errno, fmt, ap);
    va_end(ap);
    abort();
    exit(1);
}

void err_msg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    err_doit(0, 0, fmt, ap);
    va_end(ap);
}

void err_quit(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    err_doit(0, 0, fmt, ap);
    va_end(ap);
    exit(1);
}

ssize_t readn(int fd, void *ptr, size_t n) {
    size_t nleft = n;

    while (nleft > 0) {
        ssize_t nread;
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (nleft == n) {
                return -1;
            } else {
                break;
            }
        } else if (nread == 0) {
            break;
        }
        nleft -= nread;
        ptr += nread;
    }
    return n - nleft;
}

ssize_t writen(int fd, const void *ptr, size_t n) {
    size_t nleft = n;

    while (nleft > 0) {
        ssize_t nwrite;
        if ((nwrite = write(fd, ptr, nleft)) < 0) {
            if (nleft == n) {
                return -1;
            } else {
                break;
            }
        } else if (nwrite == 0) {
            break;
        }
        nleft -= nwrite;
        ptr += nwrite;
    }
    return n - nleft;
}
