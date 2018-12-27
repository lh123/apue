#include <apue.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/syslog.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#define QLEN 10
#define SYSLOG_ERR(fmt, ...) syslog(LOG_ERR, "rpcd: " fmt, ##__VA_ARGS__)
#define SYSLOG_INFO(fmt, ...) syslog(LOG_INFO, "rpcd: " fmt, ##__VA_ARGS__)
#define SYSLOG_ERR_C(fmt, ...) SYSLOG_ERR(fmt ": %s", ##__VA_ARGS__, strerror(errno))
#define SYSLOG_INFO_C(fmt, ...) SYSLOG_INFO(fmt ": %s", ##__VA_ARGS__, strerror(errno))

int initserver(int type, const struct sockaddr *addr, socklen_t alen, int qlen) {
    int err;
    int fd = socket(addr->sa_family, type, 0);
    if (fd < 0) {
        return -1;
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
    if (getrlimit(RLIMIT_OFILE, &rl) < 0) {
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
        SYSLOG_ERR_C("setsid error");
        exit(1);
    }
    if (chdir("/") < 0) {
        SYSLOG_ERR_C("chdir error");
    }

    for (int i = 0; i < rl.rlim_max; i++) {
        close(i);
    }
    int fd0 = open("/dev/null", O_RDWR);
    if (fd0 < 0) {
        SYSLOG_ERR_C("open /dev/null error");
        exit(1);
    }
    int fd1 = dup(fd0);
    int fd2 = dup(fd0);
    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        SYSLOG_ERR("unexpected dup fd %d %d %d", fd0, fd1, fd2);
        exit(1);
    }
}

void server(int sockfd) {
    if (fcntl(sockfd, F_SETFD, O_CLOEXEC) < 0) {
        SYSLOG_ERR_C("fcntl error");
        exit(1);
    }
    for (;;) {
        struct sockaddr addr;
        socklen_t alen;
        int clfd = accept(sockfd, &addr, &alen);
        if (clfd < 0) {
            if (errno != EINTR) {
                SYSLOG_ERR_C("accept error");
                exit(1);
            }
        } else {
            if (alen != sizeof(struct sockaddr)) {
                SYSLOG_INFO("invaild sockaddr received");
            } else {
                char c_ip_str[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &((struct sockaddr_in *)&addr)->sin_addr, c_ip_str, INET_ADDRSTRLEN) < 0) {
                    SYSLOG_ERR_C("inet_ntop error");
                } else {
                    SYSLOG_INFO("client: %s:%hu", c_ip_str, ntohs(((struct sockaddr_in *)&addr)->sin_port));
                }
            }

            pid_t pid = fork();
            if (pid < 0) {
                SYSLOG_ERR_C("fork error");
                exit(1);
            } else if (pid == 0) {
                if (dup2(clfd, STDOUT_FILENO) != STDOUT_FILENO || dup2(clfd, STDERR_FILENO) != STDERR_FILENO) {
                    SYSLOG_ERR_C("dup2 for child stdout and stderr error");
                    exit(1);
                }
                close(clfd);
                execl("/bin/sh", "sh", "-c", "ps -ef | wc -l", NULL);
                SYSLOG_ERR_C("unexpected return from exec");
                exit(1);
            } else {
                close(clfd);
                waitpid(pid, NULL, 0);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        err_quit("usage: rpcd <port>");
    }
    char *endptr;
    int port = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0') {
        err_quit("invalid port");
    }
    if (errno != 0) {
        err_sys("invalid port");
    }
    daemonize("rpcd");
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(port);

    int serfd = initserver(SOCK_STREAM, (struct sockaddr *)&saddr, sizeof(saddr), QLEN);
    if (serfd < 0) {
        SYSLOG_ERR_C("socket error");
        exit(1);
    }
    server(serfd);
}
