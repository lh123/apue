#include <apue.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int setasync(int sockfd) {
    if (fcntl(sockfd, F_SETOWN, getpid()) < 0) {
        return -1;
    }
    int flag = fcntl(sockfd,F_GETFL);
    if (flag < 0) {
        return -1;
    }
    if (fcntl(sockfd, F_SETFL, flag | O_ASYNC) < 0) {
        return -1;
    }
    return 0;
}

int clearasync(int sockfd) {
    int flag = fcntl(sockfd,F_GETFL);
    if (flag < 0) {
        return -1;
    }
    flag &= ~O_ASYNC;
    if (fcntl(sockfd, F_SETFL, flag) < 0) {
        return -1;
    }
    return 0;
}
