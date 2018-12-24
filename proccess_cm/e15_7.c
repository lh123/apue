#include <apue.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#define USE_POLL

#if !defined(USE_POLL)
#include <sys/select.h>
#else
#include <sys/poll.h>

#define POLL_IS(fd, msg, event) \
            do { \
                if (fd.revents & event) { \
                    printf(msg " " #event "\n");\
                } \
            } while(0) \

#endif

int main(void) {
    int fd1[2], fd2[2];

    if (pipe(fd1) < 0 || pipe(fd2) < 0) {
        err_sys("pipe error");
    }

    pid_t pid;
    if ((pid = fork()) < 0) {
        err_sys("fork error");
    } else if (pid == 0) {
        // child
        close(fd1[1]);
        close(fd2[0]);

        printf("child close read port\n");
        close(fd1[0]);
        printf("child close write port\n");
        close(fd2[1]);
        exit(0);
    } else {
        close(fd1[0]);
        close(fd2[1]);
        sleep(2);

#if !defined(USE_POLL)
        fd_set rset, wset, eset;
        int maxfd = fd1[1] > fd2[0] ? fd1[1] : fd2[0];
        
        FD_ZERO(&rset);
        FD_ZERO(&wset);
        FD_ZERO(&eset);
        FD_SET(fd1[1], &wset);
        FD_SET(fd2[0], &rset);
        FD_SET(fd1[1], &eset);
        FD_SET(fd2[0], &eset);
        int n;
        while((n = select(maxfd + 1, &rset, &wset, &eset, NULL)) <= 0) {
            if (errno == EINTR) {
                continue;
            } else {
                err_sys("select error");
            }
        }
        if (FD_ISSET(fd1[1], &wset)) {
            printf("write port writable\n");
        }
        if (FD_ISSET(fd2[0], &rset)) {
            printf("read port readable\n");
        }
        if (FD_ISSET(fd1[1], &eset)) {
            printf("write port error\n");
        }
        if (FD_ISSET(fd2[0], &eset)) {
            printf("read port error\n");
        }
#else
        struct pollfd fds[2];
        fds[0].fd = fd1[1];
        fds[0].events = POLLIN | POLLOUT | POLLERR | POLL_HUP;
        fds[1].fd = fd2[0];
        fds[0].events = POLLIN | POLLOUT | POLLERR | POLL_HUP;

        int n;
        while ((n = poll(fds, 2, -1)) < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                err_sys("poll error");
            }
        }

        POLL_IS(fds[0], "write port", POLLIN);
        POLL_IS(fds[0], "write port", POLLOUT);
        POLL_IS(fds[0], "write port", POLLERR);
        POLL_IS(fds[0], "write port", POLLHUP);

        POLL_IS(fds[1], "read port", POLLIN);
        POLL_IS(fds[1], "read port", POLLOUT);
        POLL_IS(fds[1], "read port", POLLERR);
        POLL_IS(fds[1], "read port", POLLHUP);
#endif
        while (waitpid(pid, NULL, 0) < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                err_sys("waitpid error");
            }
        }
        exit(0);
    }
}
