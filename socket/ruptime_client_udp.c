#include <apue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFLEN 128
#define TIMEOUT 20

void sigalrm(int signo) {

}

void print_uptime(int sockfd, struct sockaddr *addr, socklen_t len) {
    char buf[BUFLEN];
    buf[0] = 0;
    if (sendto(sockfd, buf, 1, 0, addr, len) < 0) {
        err_sys("sendto error");
    }
    alarm(TIMEOUT);
    struct sockaddr_in remote;
    socklen_t remote_len = sizeof(remote);
    int n = recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr *)&remote, &remote_len);
    if (n < 0) {
        if (errno != EINTR) {
            alarm(0);
        }
        err_sys("recv error");
    }
    alarm(0);
    char remoteaddrstr[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &remote.sin_addr.s_addr, remoteaddrstr, INET_ADDRSTRLEN) == NULL) {
        err_sys("inet_ntop error");
    }
    printf("remote: %s:%hu\n", remoteaddrstr, htons(remote.sin_port));
    write(STDOUT_FILENO, buf, n);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        err_quit("usage: ruptime hostname");
    }
    struct sigaction act;
    act.sa_handler = sigalrm;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    
    if (sigaction(SIGALRM, &act, NULL) < 0) {
        err_sys("sigaction SIGALRM error");
    }
    int err;
#if defined(USE_GETADDRINFO)
    struct addrinfo hint, *addlist;
    memset(&hint, 0, sizeof(hint));
    hint.ai_socktype = SOCK_DGRAM;
    if ((err = getaddrinfo(argv[1], "ruptime", &hint, &addlist)) != 0) {
        err_quit("getaddrinfo error: %s", gai_strerror(err));
    }
    struct addrinfo *aip;
    for (aip = addlist; aip != NULL; aip = aip->ai_next) {
        int sockfd = socket(aip->ai_family, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            err = errno;
        } else {
            print_uptime(sockfd, aip->ai_addr, aip->ai_addrlen);
            exit(0);
        }
    }
#else
    uint32_t baddr;
    if ((err = inet_pton(AF_INET, argv[1], &baddr)) != 1) {
        if (err == 0) {
            err_quit("invalid network address");
        }
        err_sys("inet_pton error");
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = baddr;
    addr.sin_port = htons(8888);
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        err = errno;
    } else {
        print_uptime(sockfd, (struct sockaddr *)&addr, sizeof(addr));
        exit(0);
    }
#endif
    fprintf(stderr, "can't contact %s: %s\n", argv[1], strerror(err));
}
