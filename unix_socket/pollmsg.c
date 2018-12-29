#include <apue.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <sys/msg.h>
#include <sys/socket.h>

#define NQ 3 // number of queues
#define MAXMSZ 512 //maximum message size
#define KEY 0x123

struct threadinfo {
    int qid;
    int fd;
};

struct mymesg {
    long mtype;
    char mtext[MAXMSZ];
};

void *helpter(void *arg) {
    struct threadinfo *tip = arg;
    for (;;) {
        struct mymesg m;
        memset(&m, 0, sizeof(m));
        // MSG_NOERROR: To truncate the message text if longer than msgsz bytes.
        int n = msgrcv(tip->qid, &m, MAXMSZ, 0, MSG_NOERROR);
        if (n < 0) {
            err_sys("msgrcv error");
        }
        if (write(tip->fd, m.mtext, n) < 0) {
            err_sys("write error");
        }
    }
}

int main(void) {
    int i;
    int qid[NQ];

    struct pollfd pfd[NQ];
    struct threadinfo ti[NQ];
    pthread_t tid[NQ];

    for (i = 0; i < NQ; i++) {
        if ((qid[i] = msgget((KEY + i), IPC_CREAT | 0666)) < 0) {
            err_sys("msgget error");
        }
        printf("queue ID %d is %d\n", i, qid[i]);
        int fd[2];
        if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fd) < 0) {
            err_sys("socketpair error");
        }
        pfd[i].fd = fd[0];
        pfd[i].events = POLLIN;
        ti[i].fd = fd[1];
        ti[i].qid = qid[i];
        int err = pthread_create(&tid[i], NULL, helpter, &ti[i]);
        if (err != 0) {
            err_exit(err, "pthread_create error");
        }
    }
    
    char buf[MAXMSZ];
    for (;;) {
        if (poll(pfd, NQ, -1) < 0) {
            err_sys("poll error");
        }
        for (i = 0; i < NQ; i++) {
            if (pfd[i].revents & POLLIN) {
                int n = read(pfd[i].fd, buf, MAXMSZ - 1);
                if (n < 0) {
                    err_sys("read error");
                }
                buf[n] = 0;
                printf("queue id %d, message %s\n", qid[i], buf);
            }
        }
    }
}
