#include <apue.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <errno.h>
#include <semaphore.h>

#define SEM_CLD_NAME "/sem.1"
#define SEM_PRA_NAME "/sem.2"
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
    sem_t *sem_cld = sem_open(SEM_CLD_NAME, O_CREAT | O_EXCL, 0600, 0);
    sem_t *sem_pare = sem_open(SEM_PRA_NAME, O_CREAT | O_EXCL, 0600, 1);

    sem_unlink(SEM_CLD_NAME);
    sem_unlink(SEM_PRA_NAME);

    if (sem_cld == SEM_FAILED || sem_pare == SEM_FAILED) {
        err_sys("sem_open error");
    }

    int shmid = shmget(IPC_PRIVATE, SIZE, 0600);
    printf("shmid = %d\n", shmid);
    
    void *area = shmat(shmid, NULL, 0);
    if (area < NULL) {
        err_sys("shmat error");
    }
    *(long *)area = 0;

    pid_t pid = fork();
    if (pid < 0) {
        err_sys("fork error");
    } else if (pid > 0) {
        int counter;
        for (int i = 0; i < NLOOPS; i += 2) {
            while (sem_wait(sem_pare) < 0) {
                if (errno != EINTR) {
                    err_sys("sem_wait error");
                }
            }
            if ((counter = update((long *)area)) != i) {
                err_quit("parent: expected %d, got %d", i, counter);
            }
            if (sem_post(sem_cld) < 0) {
                err_sys("sem_post error");
            }
        }

        while (waitpid(pid, NULL, 0) < 0) {
            if (errno != EINTR) {
                err_sys("waitpid error");
            }
        }
        
        printf("in parent: counter = %ld\n", *(long *)area);
        shmdt(area);

        if (shmctl(shmid, IPC_RMID, NULL) < 0) {
            err_sys("shmctl IPC_RMID error");
        }
    } else {
        int counter;
        for (int i = 1; i < NLOOPS; i += 2) {
            while (sem_wait(sem_cld) < 0) {
                if (errno != EINTR) {
                    err_sys("sem_wait error");
                }
            }
            if ((counter = update((long *)area)) != i) {
                err_quit("child: expected %d, got %d", i, counter);
            }
            if (sem_post(sem_pare) < 0) {
                err_sys("sem_post error");
            }
        }
        printf("in child: counter = %ld\n", *(long *)area);
        shmdt(area);
    }
}
