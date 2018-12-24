#include <apue.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define FIFO "temp.fifo"

int main(void) {
    unlink(FIFO);

    if (mkfifo(FIFO, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) < 0) {
        err_sys("mkfifo error");
    }

    int fdread, fdwrite;
    if ((fdread = open(FIFO, O_RDONLY | O_NONBLOCK)) < 0) {
        err_sys("open error for reading");
    }
    if ((fdwrite = open(FIFO, O_WRONLY)) < 0) {
        err_sys("open error for writing");
    }
    int flag;
    if ((flag = fcntl(fdread, F_GETFL)) < 0) {
        err_sys("fcntl F_GETFL error");
    }
    if (flag & O_NONBLOCK) {
        printf("nonblock\n");
    }
    flag &= ~O_NONBLOCK;
    if (fcntl(fdread, F_SETFL, flag) < 0) {
        err_sys("fcntl F_SETFL error");
    }
}
