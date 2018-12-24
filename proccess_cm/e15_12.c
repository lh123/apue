#include <apue.h>
#include <sys/msg.h>
#include <sys/stat.h>

#define MSGQ_NUM 5

struct mymesg {
    long mtype;
    char mtext[15];
};

int main(void) {
    int qs[MSGQ_NUM];
    for (int i = 0; i < MSGQ_NUM; i++) {
        int id = msgget(IPC_PRIVATE, 0644);
        if (id < 0) {
            err_sys("msgget error");
        }
        printf("id = %d\n", id);
        if (msgctl(id, IPC_RMID, NULL) < 0) {
            err_sys("msgctl IPC_RMID error\n");
        }
    }

    for (int i = 0; i < MSGQ_NUM; i++) {
        qs[i] = msgget(IPC_PRIVATE, 0644);
        if (qs[i] < 0) {
            err_sys("msgget error");
        }
        struct mymesg msg = {i + 1, "Hello World"};
        if (msgsnd(qs[i], &msg, sizeof(struct mymesg) - sizeof(long), 0) < 0) {
            err_sys("msgsnd error");
        }
    }
    for (int i = 0; i < MSGQ_NUM; i++) {
        printf("qs[%d] = %d\n", i, qs[i]);
    }
}
