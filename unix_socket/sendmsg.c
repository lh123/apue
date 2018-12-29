#include <apue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>

#define MAXMSZ 512

struct mymesg {
    long mtype;
    char mtext[MAXMSZ];
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: sendmsg KEY message\n");
        exit(1);
    }
    key_t key = strtol(argv[1], NULL, 0);
    int qid = msgget(key, 0);
    if (qid < 0) {
        err_sys("can't open queue key %s", argv[1]);
    }
    struct mymesg m;
    memset(&m, 0, sizeof(m));
    strncpy(m.mtext, argv[2], MAXMSZ - 1);
    size_t nbytes = strnlen(m.mtext, MAXMSZ - 1);
    m.mtype = 1;
    if (msgsnd(qid, &m, nbytes, 0) < 0) {
        err_sys("can't send message");
    }
}
