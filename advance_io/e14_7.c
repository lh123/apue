#include <apue.h>
#include <fcntl.h>
#include <unistd.h>

int main(void) {
    int fd[2];
    if (pipe(fd) < 0) {
        err_sys("pipe error");
    }

    int flag = fcntl(fd[1], F_GETFL);
    if (flag < 0) {
        err_sys("fcntl F_GETFL error");
    }

    flag |= O_NONBLOCK;
    if (fcntl(fd[1], F_SETFL, flag) < 0) {
        err_sys("fcntl F_SETFL error");
    }
    int n;
    for (n = 0;; n++) {
        int i = write(fd[1], "a", 1);
        if (i != 1) {
            printf("write ret %d, ", i);
            break;
        }
    }
    printf("pipe capacity = %d, PIPE_BUF = %d\n", n, (int)pathconf("/", _PC_PIPE_BUF));
}
