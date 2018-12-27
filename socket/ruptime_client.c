#include <apue.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAXSLEEP 0x80
#define BUFLEN 128

int connect_retry(int domain, int type, int protocol, const struct sockaddr *addr, socklen_t alen) {
    int numsec, fd;

    for (numsec = 1; numsec < MAXSLEEP; numsec <<= 2) {
        if ((fd = socket(domain, type, 0)) < 0) {
            return -1;
        }
        if (connect(fd, addr, alen) == 0) {
            return fd;
        }
        close(fd);

        if (numsec <= MAXSLEEP / 2) {
            sleep(numsec);
        }
    }
    return -1;
}

void print_uptime(int sockfd) {
    int n;
    char buf[BUFLEN];

    while ((n = recv(sockfd, buf, BUFLEN, 0)) > 0) {
        write(STDOUT_FILENO, buf, n);
    }
    if (n < 0) {
        err_sys("recv error");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        err_quit("usage: ruptime hostname port");
    }
    int err;
#if defined(USE_GETADDRINFO)
    struct addrinfo hint, *ailist, *aip;
    memset(&hint, 0, sizeof(hint));
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_canonname = NULL;
    hint.ai_addr = NULL;
    hint.ai_next = NULL;

    
    if ((err = getaddrinfo(argv[1], "ruptime", &hint, &ailist)) != 0) {
        err_quit("getaddrinfo error: %s", gai_strerror(err));
    }
    for (aip = ailist; aip != NULL; aip = aip->ai_next) {
        int sockfd;
        if ((sockfd = connect_retry(aip->ai_family, SOCK_STREAM, 0, aip->ai_addr, aip->ai_addrlen)) < 0) {
            err = errno;
        } else {
            print_uptime(sockfd);
            exit(0);
        }
    }
#else
    int saddr;
    if (inet_pton(AF_INET, argv[1], &saddr) != 1) {
        err_sys("inet_pton error");
    }
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_addr.s_addr = saddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(atoi(argv[2]));

    int sockfd = connect_retry(AF_INET, SOCK_STREAM, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if (sockfd < 0) {
        err = errno;
    } else {
        print_uptime(sockfd);
        exit(0);
    }
#endif
    err_exit(err, "can't connect to %s", argv[1]);
}
