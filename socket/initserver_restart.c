#include <apue.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>

int initserver(int type, const struct sockaddr *addr, socklen_t alen, int qlen) {
    int err;
    int fd = socket(addr->sa_family, type, 0);
    if (fd < 0) {
        err_sys("socket error");
    }
    int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0) {
        goto errout;
    }
    if (type == SOCK_STREAM || type == SOCK_SEQPACKET) {
        if (listen(fd, qlen) < 0) {
            goto errout;
        }
    }
    return fd;
errout:
    err = errno;
    close(fd);
    errno = err;
    return -1;
}
