#include <apue.h>
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>

int my_isatty(int fd) {
    struct termios ts;
    return tcgetattr(fd, &ts) != -1;
}

int main(void) {
    int fd1 = open("/dev/null", O_RDWR);
    int fd2 = open("/dev/tty", O_RDWR);
    if (fd1 < 0 || fd2 < 0) {
        err_sys("open error");
    }
    printf("%-12s %15s %15s\n", "name", "my_isatty", "isatty");
    printf("%-12s %15s %15s\n", "/dev/null", my_isatty(fd1) ? "true" : "false", isatty(fd1) ? "true" : "false");
    printf("%-12s %15s %15s\n", "/dev/tty", my_isatty(fd2) ? "true" : "false", isatty(fd2) ? "true" : "false");
}
