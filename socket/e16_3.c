#include <apue.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/resource.h>

#define SYSLOG_ERR(fmt, ...) syslog(LOG_ERR, "ruptimed: " fmt ": %s", ##__VA_ARGS__, strerror(errno))
#define SYSLOG_INFO(fmt, ...) syslog(LOG_INFO, "ruptimed: " fmt, ##__VA_ARGS__)

#define PORT_NUMS 10
#define QLEN 10

int initserver(int type, const struct sockaddr *addr, socklen_t alen, int qlen) {
    int err;
    int fd = socket(addr->sa_family, type, 0);
    if (fd < 0) {
        goto errout;
    }
    int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0) {
        goto errout;
    }
    if (bind(fd, addr, alen) < 0) {
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

void daemonize(const char *cmd) {
    umask(0);
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        err_sys("getrlimit error");
    }
    if (rl.rlim_max == RLIM_INFINITY) {
        rl.rlim_max = 1024;
    }

    pid_t pid = fork();
    if (pid < 0) {
        err_sys("fork error");
    } else if (pid != 0) {
        exit(0);
    }

    openlog(cmd, LOG_CONS, LOG_DAEMON);
    if (setsid() < 0) {
        SYSLOG_ERR("setsid error");
    }
    if (chdir("/") < 0) {
        SYSLOG_ERR("chdir error");
    }
    for (int i = 0; i < rl.rlim_max; i++) {
        close(i);
    }
    int fd0 = open("/dev/null", O_RDWR);
    int fd1 = dup(0);
    int fd2 = dup(0);
    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        syslog(LOG_ERR, "unexpectd dup result %d %d %d", fd0, fd1, fd2);
        exit(1);
    }
}

void serve(int *sockfds, int len) {
    int maxfd = -1;
    for (int i = 0; i < len; i++) {
        if (fcntl(sockfds[i], F_SETFD, O_CLOEXEC) < 0) {
            SYSLOG_ERR("fcntl F_SETFD error");
            exit(1);
        }
        if (sockfds[i] > maxfd) {
            maxfd = sockfds[i];
        }
    }

    for (;;) {
        int maxfd = -1;
        fd_set aset;
        FD_ZERO(&aset);
        for (int i = 0; i < len; i++) {
            FD_SET(sockfds[i], &aset);
            maxfd = sockfds[i] > maxfd ? sockfds[i] : maxfd;
        }
        int n = select(maxfd + 1, &aset, NULL, NULL, NULL);
        if (n < 0) {
            if (errno != EINTR) {
                SYSLOG_ERR("select error");
                exit(1);
            }
        } else if (n > 0) {
            for (int i = 0; i < len; i++) {
                if (FD_ISSET(sockfds[i], &aset)) {
                    int clfd = accept(sockfds[i], NULL, NULL);
                    if (clfd < 0) {
                        SYSLOG_ERR("accept error");
                        exit(1);
                    }
                    pid_t pid = fork();
                    if (pid < 0) {
                        SYSLOG_ERR("fork error");
                        exit(1);
                    } else if (pid == 0) {
                        if (dup2(clfd, STDOUT_FILENO) != STDOUT_FILENO || dup2(clfd, STDERR_FILENO) != STDERR_FILENO) {
                            SYSLOG_ERR("dup2 error");
                            exit(1);
                        }
                        close(clfd);
                        execl("/usr/bin/uptime", "uptime", NULL);
                        SYSLOG_ERR("unexpected return from exec");
                        exit(1);
                    } else {
                        close(clfd);
                        waitpid(pid, NULL, 0);
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 1) {
        err_quit("usage: ruptimed");
    }
    daemonize("ruptimed");
    struct sockaddr_in seraddr[PORT_NUMS];
    for (int i = 0; i < PORT_NUMS; i++) {
        memset(&seraddr[i], 0, sizeof(struct sockaddr_in));
        seraddr[i].sin_family = AF_INET;
        seraddr[i].sin_addr.s_addr = htonl(INADDR_ANY);
        seraddr[i].sin_port = htons(8888 + i);
    }
    int sockfds[PORT_NUMS];
    for (int i = 0; i < PORT_NUMS; i++) {
        sockfds[i] = initserver(SOCK_STREAM, (struct sockaddr *)&seraddr[i], sizeof(struct sockaddr_in), QLEN);
        if (sockfds[i] < 0) {
            SYSLOG_ERR("initserver error");
        } else {
            SYSLOG_INFO("server start at %hu", ntohs(seraddr[i].sin_port));
        }
    }
    serve(sockfds, PORT_NUMS);
}
