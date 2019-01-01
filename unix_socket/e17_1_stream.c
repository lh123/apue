#include <apue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <pthread.h>

#include <sys/un.h>
#include <sys/msg.h>
#include <sys/socket.h>

#define NQ 3
#define MAXMSZ 512
#define KEY 0x123

struct mymesg {
    long mtype;
    char mtext[MAXMSZ];
};

struct threadinfo {
    int fd;
    int qid;
};

void *helper(void *arg) {
    struct threadinfo *info = arg;
    struct mymesg msg;
    for (;;) {
        memset(&msg, 0, sizeof(msg));
        int n = msgrcv(info->qid, &msg, MAXMSZ, 0, MSG_NOERROR);
        if (n < 0) {
            err_sys("msgrcv error");
        }
        if (writen(info->fd, &n, sizeof(n)) != sizeof(n)) {
            err_sys("write error");
        }
        if (writen(info->fd, msg.mtext, n) != n) {
            err_sys("write error");
        }
    }
}

int main(void) {
    int qid[NQ];
    struct threadinfo ti[NQ];
    int fd[2];
    struct pollfd pfd[NQ];
    pthread_t tid[NQ];

    int i;
    int err;
    for (i = 0; i < NQ; i++) {
        if ((qid[i] = msgget(KEY + i, IPC_CREAT | 0666)) < 0) {
            err_sys("msgget error");
        }
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) < 0) {
            err_sys("socketpair error");
        }
        ti[i].fd = fd[0];
        ti[i].qid = qid[i];
        pfd[i].fd = fd[1];
        pfd[i].events = POLLIN;

        if ((err = pthread_create(&tid[i], NULL, helper, &ti[i])) != 0) {
            err_exit(err, "pthread_create error");
        }
    }

    char buf[MAXLINE];
    for (;;) {
        int n = poll(pfd, NQ, -1);
        if (n < 0) {
            err_sys("poll error");
        }

        for (i = 0; i < NQ; i++) {
            if (pfd[i].revents & POLLIN) {
                int n;
                if (readn(pfd[i].fd, &n, sizeof(n)) != sizeof(n)) {
                    err_sys("read error");
                }
                if (readn(pfd[i].fd, buf, n) != n) {
                    err_sys("read error");
                }
                buf[n] = 0;
                printf("queue id %d, message %s\n", qid[i], buf);
            }
        }
    }
}
