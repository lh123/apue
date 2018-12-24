#include <apue.h>
#include <sys/shm.h>

#define SHM_SIZE 100000
#define SHM_MODE 0600

struct list {
    char data;
    off_t nextoffset;
};

int main(void) {
    int shmid;

    if ((shmid = shmget(IPC_PRIVATE, SHM_SIZE, SHM_MODE)) < 0) {
        err_sys("shmget error");
    }
    printf("shmid = %d\n", shmid);
    
    void *shmptr;
    if ((shmptr = shmat(shmid, NULL, 0)) < NULL) {
        err_sys("shmat error");
    }

    printf("attach address: %p\n", shmptr);

    struct list *ptr = (struct list *)shmptr;
    ptr[0].data = 'a';
    ptr[0].nextoffset = sizeof(struct list);
    ptr[1].data = 'b';
    ptr[1].nextoffset = 0;

    if (shmdt(shmptr) < 0) {
        err_sys("shmdt error");
    }

    if ((shmptr = shmat(shmid, NULL, 0)) < NULL) {
        err_sys("shmat error");
    }

    if ((shmptr = shmat(shmid, NULL, 0)) < NULL) {
        err_sys("shmat error");
    }
    printf("attach address: %p\n", shmptr);

    ptr = (struct list *)shmptr;
    printf("1: data: %c\n", ptr[0].data);
    printf("2: data: %c\n", ptr[1].data);

    if (shmdt(shmptr) < 0) {
        err_sys("shmdt error");
    }
    if (shmctl(shmid, IPC_RMID, NULL) < 0) {
        err_sys("shmctl IPC_RMID error");
    }
}
