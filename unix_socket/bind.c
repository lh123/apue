#include <apue.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(void) {
    struct sockaddr_un un;
    memset(&un, 0, sizeof(struct sockaddr_un));
    un.sun_family = AF_UNIX;
    strncpy(un.sun_path, "foo.socket", sizeof(un.sun_path) - 1);
    int size = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);
    int fd = socket(un.sun_family, SOCK_STREAM, 0);
    if (fd < 0) {
        err_sys("socket failed");
    }
    if (bind(fd, (struct sockaddr *)&un, size) < 0) {
        err_sys("bind failed");
    }
    printf("UNIX domain socket bound\n");
}
