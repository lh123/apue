#include <apue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <syslog.h>
#include <arpa/inet.h>

#define BUFLEN 128
#define HOST_NAME_MAX 256

int initserver(int type, const struct sockaddr *addr, socklen_t len, int qlen) {
    int err;
    int sockfd = socket(addr->sa_family, type, 0);
    if (sockfd < 0) {
        return -1;
    }
    if (bind(sockfd, addr, len) < 0) {
        goto errout;
    }
    if (type == SOCK_STREAM || type == SOCK_SEQPACKET) {
        if (listen(sockfd, qlen) < 0) {
            goto errout;
        }
    }
    return sockfd;
errout:
    err = errno;
    close(sockfd);
    errno = err;
    return -1;
}

void daemonize(const char *cmd) {
    umask(0);

    pid_t pid = fork();
    if (pid < 0) {
        err_sys("fork error");
    } else if (pid != 0) {
        exit(0);
    }

    openlog(cmd, LOG_CONS, LOG_DAEMON);
    if (setsid() < 0) {
        syslog(LOG_ERR, "%s: setsid error", cmd);
    }
    if (chdir("/") < 0) {
        syslog(LOG_ERR, "%s: chdir error", cmd);
    }

    int nullfd = open("/dev/null", O_RDWR);
    if (nullfd < 0) {
        syslog(LOG_ERR, "%s: open /dev/null error", cmd);
    }
    if (dup2(nullfd, STDIN_FILENO) != STDIN_FILENO) {
        syslog(LOG_ERR, "%s: dup2 error", cmd);
    }
    if (dup2(nullfd, STDOUT_FILENO) != STDOUT_FILENO) {
        syslog(LOG_ERR, "%s: dup2 error", cmd);
    }
    if (dup2(nullfd, STDERR_FILENO) != STDERR_FILENO) {
        syslog(LOG_ERR, "%s: dup2 error", cmd);
    }
}

void serve(int sockfd) {
    if (fcntl(sockfd, F_SETFD, O_CLOEXEC) < 0) {
        syslog(LOG_ERR, "ruptimed: fcntl error");
    }
    for (;;) {
        char buf[BUFLEN];
        struct sockaddr addr;
        socklen_t addrlen = sizeof(addr);
        int n = recvfrom(sockfd, buf, BUFLEN, 0, &addr, &addrlen);
        if (n < 0) {
            syslog(LOG_ERR, "ruptimed: recvfrom error: %s", strerror(errno));
            exit(1);
        }
        if (addrlen != sizeof(addr)) {
            syslog(LOG_INFO, "ruptimed: invalid sockaddr recv");
        } else {
            if (addr.sa_family == AF_INET) {
                char inetbuff[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &((struct sockaddr_in *)&addr)->sin_addr, inetbuff, INET_ADDRSTRLEN) == NULL) {
                    syslog(LOG_ERR, "ruptimed: inet_ntop error");
                } else {
                    syslog(LOG_INFO, "ruptimed: client: %s:%hu", inetbuff, ntohs(((struct sockaddr_in *)&addr)->sin_port));
                }
            } else if (addr.sa_family == AF_INET6) {
                char inetbuff[INET6_ADDRSTRLEN];
                if (inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&addr)->sin6_addr, inetbuff, INET6_ADDRSTRLEN) == NULL) {
                    syslog(LOG_ERR, "ruptimed: inet_ntop error");
                } else {
                    syslog(LOG_INFO, "ruptimed: client: %s:%hu", inetbuff, ntohs(((struct sockaddr_in *)&addr)->sin_port));
                }
            }
        }
        FILE *fp = popen("/usr/bin/uptime", "r");
        if (fp == NULL) {
            snprintf(buf, BUFLEN, "error: %s\n", strerror(errno));
            sendto(sockfd, buf, strnlen(buf, BUFLEN), 0, &addr, addrlen);
        } else {
            if (fgets(buf, BUFLEN, fp) != NULL) {
                sendto(sockfd, buf, strnlen(buf, BUFLEN), 0, &addr, addrlen);
            }
            pclose(fp);
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
    struct addrinfo hint;
    memset(&hint, 0, sizeof(hint));
    hint.ai_socktype = SOCK_DGRAM;
    hint.ai_flags = AI_CANONNAME;

    int err;
    struct addrinfo *ailist;
    if ((err = getaddrinfo(host, "ruptime", &hint, &ailist)) != 0) {
        syslog(LOG_ERR, "ruptimed: getaddrinfo error: %s", gai_strerror(err));
        exit(1);
    }
    struct addrinfo *aip;
    for (aip = ailist; aip != NULL; aip = aip->ai_next) {
        int sockfd = initserver(SOCK_DGRAM, aip->ai_addr, aip->ai_addrlen, 0);
        if (sockfd >= 0) {
            serve(sockfd);
            exit(0);
        }
    }
#else
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8888);
    
    int sockfd = initserver(SOCK_DGRAM, (struct sockaddr *)&addr, sizeof(addr), 0);
    if (sockfd >= 0) {
        serve(sockfd);
        exit(0);
    } else {
        syslog(LOG_ERR, "ruptimed: initserver error: %s", strerror(errno));
    }
#endif
    exit(1);
}
