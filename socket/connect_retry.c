#include <apue.h>
#include <unistd.h>
#include <sys/socket.h>

#define MAXSLEEP 128 // 1000 0000(b)

int connect_retry(int sockfd, const struct sockaddr *addr, socklen_t alen) {
    int numesc;
    for (numesc = 1; numesc <= MAXSLEEP; numesc <<= 1) {
        if (connect(sockfd, addr, alen) == 0) {
            return 0;
        }
        // 每次失败都会休眠，但时常翻倍
        if (numesc <= MAXSLEEP / 2) {
            sleep(numesc);
        }
    }
    return -1;
}
