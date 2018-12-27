#include <apue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXSLEEP 0x80
#define BUFLEN 128

int connect_retry(int type, const struct sockaddr *addr, socklen_t alen) {
    int numsec, fd;
    for (numsec = 1; numsec < MAXSLEEP; numsec <<= 1) {
        if ((fd = socket(addr->sa_family, type, 0)) < 0) {
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

void print_pc(int fd) {
    char buf[BUFLEN];
    int n;
    printf("process count: ");
    fflush(stdout);
    while ((n = recv(fd, buf, BUFLEN, 0)) > 0) {
        write(STDOUT_FILENO, buf, n);
    }
    if (n < 0) {
        err_sys("recv error");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        err_quit("usage: rpc <ip> <port>");
    }
    char *endptr;
    int port = strtol(argv[2], &endptr, 10);
    if (*endptr != 0) {
        err_quit("invaild port");
    }
    if (errno != 0) {
        err_sys("invaild port");
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    if (inet_pton(AF_INET, argv[1], &saddr.sin_addr) < 0) {
        err_sys("inet_pton error");
    }
    saddr.sin_port = htons(port);

    int fd = connect_retry(SOCK_STREAM, (struct sockaddr *)&saddr, sizeof(saddr));
    if (fd < 0) {
        err_sys("connect error");
    }
    print_pc(fd);
}
