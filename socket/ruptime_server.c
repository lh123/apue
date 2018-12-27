#include <apue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/resource.h>

#define QLEN 10
#define HOST_NAME_MAX 256

int initserver(int type, const struct sockaddr *addr, socklen_t alen, int qlen) {
    int fd, err;
    if ((fd = socket(addr->sa_family, type, 0)) < 0) {
        return -1;
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

void daemonize(const char *cmdstring) {
    umask(0);

    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        err_sys("getrlimit error");
    }
    pid_t pid;

    // fork 是为了让该进程不是组长进程
    if ((pid = fork()) < 0) {
        err_sys("fork error");
    } else if (pid != 0) {
        exit(0);
    }

    setsid(); //该进程变成会话首进程、组长进程，ppid = 1
    struct sigaction act;
    act.sa_handler = SIG_IGN;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    if (sigaction(SIGHUP, &act, NULL) < 0) {
        err_sys("sigaction error");
    }

    if (chdir("/") < 0) {
        err_sys("chdir error");
    }

    if (rl.rlim_max == RLIM_INFINITY) {
        rl.rlim_max = 1024;
    }
    int i;
    for (i = 0; i < rl.rlim_max; i++) {
        close(i);
    }

    int fd0, fd1, fd2;
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);
    openlog(cmdstring, LOG_CONS, LOG_DAEMON);
    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        syslog(LOG_ERR, "unexpected file dsecriptors %d %d %d", fd0, fd1, fd2);
        exit(1);
    }
}

void serve(int sockfd) {
    if (fcntl(sockfd, F_SETFD, FD_CLOEXEC) < 0) {
        err_sys("fcntl F_SETFD FD_CLOEXEC error");
    }
    for (;;) {
        int clfd;
        if ((clfd = accept(sockfd, NULL, NULL)) < 0) {
            syslog(LOG_ERR, "ruptimed: accept error: %s", strerror(errno));
            exit(1);
        }
        pid_t pid;
        if ((pid = fork()) < 0) {
            syslog(LOG_ERR, "ruptimed: fork error: %s", strerror(errno));
            exit(1);
        } else if (pid == 0) {
            if (dup2(clfd, STDOUT_FILENO) != STDOUT_FILENO || dup2(clfd, STDERR_FILENO) != STDERR_FILENO) {
                syslog(LOG_ERR, "ruptimed: unexpected error");
                exit(1);
            }
            close(clfd);
            execl("/usr/bin/uptime", "uptime", NULL);
            syslog(LOG_ERR, "ruptimed: unexpected return from exec: %s", strerror(errno));
        } else {
            close(clfd);
            waitpid(pid, NULL, 0);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 1) {
        err_quit("usage: ruptimed");
    }
    int n = sysconf(_SC_HOST_NAME_MAX);
    if (n < 0) {
        n = HOST_NAME_MAX;
    }
    char *host = malloc(n);
    if (host == NULL) {
        err_sys("malloc error");
    }
    if (gethostname(host, n) < 0) {
        err_sys("gethostname error");
    }
    daemonize("ruptimed");
#if defined(USE_GETADDRINFO)
    struct addrinfo hint, *ailist, *aip;
    memset(&hint, 0, sizeof(hint));
    hint.ai_flags = AI_CANONNAME;
    hint.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(host, "ruptime", &hint, &ailist);
    if (err != 0) {
        syslog(LOG_ERR, "ruptimed: getaddrinfo error: %s", gai_strerror(err));
        exit(1);
    }
    for (aip = ailist; aip != NULL; aip = aip->ai_next) {
        int sockfd = initserver(SOCK_STREAM, aip->ai_addr, aip->ai_addrlen, QLEN);
        if(sockfd >= 0) {
            serve(sockfd);
            exit(0);
        }
    }
#else
    struct sockaddr_in bindaddr;
    memset(&bindaddr, 0, sizeof(bindaddr));
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_port = htons(8888);

    int addrlen = sizeof(bindaddr);

    int sockfd = initserver(SOCK_STREAM, (struct sockaddr *)&bindaddr, addrlen, QLEN);
    if(sockfd >= 0) {
        serve(sockfd);
        exit(0);
    }
#endif
}
