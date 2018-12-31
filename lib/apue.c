#include "apue.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <stddef.h>
#include <stdalign.h>

#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>




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

#define QLEN 10

int serv_listen(const char *name) {
    if (strnlen(name, sizeof((struct sockaddr_un *)0)->sun_path) >= sizeof((struct sockaddr_un *)0)->sun_path) {
        errno = ENAMETOOLONG;
        return -1;
    }

    int fd =  socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -2;
    }
    unlink(name);

    struct sockaddr_un un;
    memset(&un, 0, sizeof(un));
    un.sun_family = AF_UNIX;
    strncpy(un.sun_path, name, sizeof(un.sun_path) - 1);
    
    int rval;
    int err;
    if (bind(fd, (struct sockaddr *)&un, sizeof(un)) < 0) {
        rval = -3;
        goto errout;
    }

    if (listen(fd, QLEN) < 0) {
        rval = -4;
        goto errout;
    }
    return fd;
errout:
    err = errno;
    close(fd);
    errno = err;
    return rval;
}

#define STALE 30

int serv_accept(int listenfd, uid_t *uidptr) {
    char *name = malloc(sizeof(((struct sockaddr_un *)NULL)->sun_path) + 1);
    if (name == NULL) {
        return -1;
    }
    struct sockaddr_un un;
    int len = sizeof(un);
    int clifd = accept(listenfd, (struct sockaddr *)&un, &len);
    if (clifd < 0) {
        free(name);
        return -2;
    }

    strncpy(name, un.sun_path, sizeof(un.sun_path));
    name[sizeof(un.sun_path)] = 0;

    int rval;
    struct stat statbuf;
    if (stat(name, &statbuf) < 0) {
        rval = -3;
        goto errout;
    }

#if defined(S_ISSOCK)
    if (S_ISSOCK(statbuf.st_mode) == 0) {
        rval = -4;
        goto errout;
    }
#endif
    if ((statbuf.st_mode & (S_IRWXG | S_IRWXO) != 0) || (statbuf.st_mode & S_IRWXU) != S_IRWXU) { //rwx------
        rval = -5;
        goto errout;
    }

    time_t staletime = time(NULL) - STALE;
    if (statbuf.st_atime < staletime || statbuf.st_ctime < staletime || statbuf.st_mtime < staletime) {
        rval = -6;
        goto errout;
    }

    if (uidptr != NULL) {
        *uidptr = statbuf.st_uid;
    }
    unlink(name);
    free(name);
    return clifd;

    int err;
errout:
    err = errno;
    close(clifd);
    free(name);
    errno = err;
    return rval;
}

#define CLI_PATH "/tmp"
#define CLI_PERM S_IRWXU

int cli_conn(const char *name) {
    struct sockaddr_un un;
    if (strlen(name) >= sizeof(un.sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    memset(&un, 0, sizeof(un));
    un.sun_family = AF_UNIX;
    snprintf(un.sun_path, sizeof(un.sun_path), "%s/%05ld", CLI_PATH, (long)getpid());
    unlink(un.sun_path);

    int rval;
    if (bind(fd, (struct sockaddr *)&un, sizeof(un)) < 0) {
        rval = -2;
        goto errout;
    }
    
    int do_unlink = 0;
    if (chmod(un.sun_path, CLI_PERM) < 0) {
        rval = -3;
        do_unlink = 1;
        goto errout;
    }

    struct sockaddr_un sun;
    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    strncpy(sun.sun_path, name, sizeof(sun.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) < 0) {
        rval = -4;
        do_unlink = 1;
        goto errout;
    }
    return fd;
    int err;
errout:
    err = errno;
    close(fd);
    if (do_unlink) {
        unlink(un.sun_path);
    }
    errno = err;
    return rval;
}

int send_fd(int fd, int fd_to_send) {
    static struct cmsghdr *cmptr = NULL;
    struct msghdr msg;
    struct iovec iov[1];

    char buf[2];
    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);

    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    if (fd_to_send < 0) {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        buf[1] = -fd_to_send;
        if (buf[1] == 0) {
            buf[1] = 1;
        }
    } else {
        if (cmptr == NULL) {
            if ((cmptr = malloc(CMSG_SPACE(sizeof(int)))) == NULL) {
                return -1;
            }
        }
        cmptr->cmsg_level = SOL_SOCKET;
        cmptr->cmsg_type = SCM_RIGHTS;
        cmptr->cmsg_len = CMSG_LEN(sizeof(int));
        msg.msg_control = cmptr;
        msg.msg_controllen = CMSG_SPACE(sizeof(int));
        *(int *)CMSG_DATA(cmptr) = fd_to_send;
        buf[1] = 0;
    }
    buf[0] = 0;
    if (sendmsg(fd, &msg, 0) != 2) {
        return -1;
    }
    return 0;
}

int recv_fd(int fd, ssize_t (*userfunc)(int, const void *, size_t)) {
    static struct cmsghdr *cmptr = NULL;
    int status = -1;
    int newfd;
    char buf[MAXLINE];
    struct iovec iov[1];
    struct msghdr msg;
    for (;;) {
        iov[0].iov_base = buf;
        iov[0].iov_len = sizeof(buf);

        msg.msg_iov = iov;
        msg.msg_iovlen = 1;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        if (cmptr == NULL) {
            if ((cmptr = malloc(CMSG_SPACE(sizeof(int)))) == NULL) {
                return -1;
            }
        }
        msg.msg_control = cmptr;
        msg.msg_controllen = CMSG_SPACE(sizeof(int));
        
        int nr = recvmsg(fd, &msg, 0);
        if (nr < 0) {
            err_ret("recvmsg error");
            return -1;
        } else if (nr == 0) {
            err_ret("connection closed by server");
            return -1;
        }

        char *ptr;
        for (ptr = buf; ptr < &buf[nr]; ) {
            if (*ptr++ == 0) {
                if (ptr != &buf[nr - 1]) {
                    err_dump("message format error");
                }
                status = *ptr & 0xFF;
                if (status == 0) {
                    if (msg.msg_controllen < CMSG_SPACE(sizeof(int))) {
                        err_dump("status = 0 but no fd");
                    }
                    newfd = *(int *)CMSG_DATA(cmptr);
                } else {
                    newfd = -status;
                }
                nr -= 2;
            }
        }
        if (nr > 0 && userfunc(STDERR_FILENO, buf, nr) != nr) {
            return -1;
        }
        if (status >= 0) {
            return newfd;
        }
    }
}

int send_err(int fd, int errcode, const char *msg) {
    int n = strlen(msg);
    if (n > 0 && writen(fd, msg, n) != n) {
        return -1;
    }
    if (errcode >= 0) {
        errcode = -1;
    }
    if (send_fd(fd, errcode) < 0) {
        return -1;
    }
    return 0;
}
