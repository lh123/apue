#include "open.h"
#include <unistd.h>
#include <string.h>
#include <sys/uio.h> // struct iovec
#include <sys/socket.h>

int csopen(char *name, int oflag) {
    static int fd[2] = {-1, -1};
    if (fd[0] < 0) {
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) < 0) {
            err_ret("socketpair error");
            return -1;
        }
        pid_t pid = fork();
        if (pid < 0) {
            err_ret("fork error");
            return -1;
        } else if (pid == 0) {
            close(fd[0]);
            if (fd[1] != STDIN_FILENO && dup2(fd[1], STDIN_FILENO) != STDIN_FILENO) {
                err_sys("dup2 error to stdin");
            }
            if (fd[1] != STDOUT_FILENO && dup2(fd[1], STDOUT_FILENO) != STDOUT_FILENO) {
                err_sys("dup2 error to stdin");
            }
            if (execl("./opend.out", "opend", NULL) < 0) {
                err_sys("execl error");
            }
        }
        close(fd[1]);
    }
    char buf[10];
    snprintf(buf, sizeof(buf), " %d", oflag);
    struct iovec iov[3];
    iov[0].iov_base = CL_OPEN " ";
    iov[0].iov_len = strlen(CL_OPEN) + 1;
    iov[1].iov_base = name;
    iov[1].iov_len = strlen(name);
    iov[2].iov_base = buf;
    iov[2].iov_len = strlen(buf) + 1; // for null at end of buf
    int len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len;
    if (writev(fd[0], &iov[0], 3) != len) {
        err_ret("writev error");
        return -1;
    }
    return recv_fd(fd[0], write);
}
