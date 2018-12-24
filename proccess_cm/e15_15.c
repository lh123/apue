#include <apue.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <errno.h>

#define NLOOPS 1000
#define SIZE sizeof(long)

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short  *array;
};

static int update(long *ptr) {
    return (*ptr)++;
}

int main(void) {
    int shmid = shmget(IPC_PRIVATE, SIZE, 0600);
    if (shmid < 0) {
        err_sys("shmget error");
    }
    printf("shmid = %d\n", shmid);
    void *area = shmat(shmid, NULL, 0);
    if (area < NULL) {
        err_sys("shmat error");
    }
    *(long *)area = 0;


    int semid = semget(IPC_PRIVATE, 2, 0600);
    if (semid < 0) {
        err_sys("semget error");
    }

    unsigned short array[2] = {0, 0};
    union semun args;
    args.array = array;
    if (semctl(semid, 0, SETALL, args) < 0) {
        err_sys("semctl error");
    }

    pid_t pid = fork();
    if (pid < 0) {
        err_sys("fork error");
    } else if (pid > 0) {
        int counter;
        for (int i = 0; i < NLOOPS; i += 2) {
            if ((counter = update((long *)area)) != i) {
                err_quit("parent: expected %d, got %d", i, counter);
            }
            struct sembuf buf;
            buf.sem_flg = SEM_UNDO;
            buf.sem_num = 0;
            buf.sem_op = 1;

            if (semop(semid, &buf, 1) < 0) { //wake child
                err_sys("semop error");
            }

            buf.sem_op = -1;
            buf.sem_num = 1;
            while (semop(semid, &buf, 1) < 0) { //wait child
                if (errno == EINTR) {
                    continue;
                } else {
                    err_sys("semop error");
                }
            }
        }
        printf("in parent: counter = %ld\n", *(long *)area);
        shmdt(area);

        while (waitpid(pid, NULL, 0) < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                err_sys("waitpid error");
            }
        }
        if (shmctl(shmid, IPC_RMID, NULL) < 0) {
            err_sys("shmctl IPC_RMID error");
        }
    } else {
        int counter;
        for (int i = 1; i < NLOOPS; i += 2) {
            struct sembuf buf;
            buf.sem_flg = SEM_UNDO;
            buf.sem_num = 0;
            buf.sem_op = -1;

            while (semop(semid, &buf, 1) < 0) { //wait parent
                if (errno == EINTR) {
                    continue;
                } else {
                    err_sys("semop error");
                }
            }
            if ((counter = update((long *)area)) != i) {
                err_quit("child: expected %d, got %d", i, counter);
            }

            buf.sem_op = 1;
            buf.sem_num = 1;
            buf.sem_flg = SEM_UNDO;

            if (semop(semid, &buf, 1) < 0) {
                err_sys("semop error");
            }
        }
        printf("in child: counter = %ld\n", *(long *)area);
        shmdt(area);
    }
}
