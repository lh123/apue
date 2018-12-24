#include <apue.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_SIZE 40000
#define MALLOC_SIZE 100000
#define SHM_SIZE 100000
#define SHM_MODE 0600

char array[ARRAY_SIZE];

int main(void) {
    int shmid;

    printf("array[] from %p to %p\n", (void *)&array[0], (void *)&array[ARRAY_SIZE]);
    printf("stack around %p\n", (void *)&shmid);

    char *ptr = malloc(MALLOC_SIZE);
    if (ptr == NULL) {
        err_sys("malloc error");
    }
    printf("malloced from %p to %p\n", (void *)ptr, (void *)ptr + MALLOC_SIZE);

    if ((shmid = shmget(IPC_PRIVATE, SHM_SIZE, SHM_MODE)) < 0) {
        err_sys("shmget error");
    }
    char *shmptr = shmat(shmid, 0, 0);
    if (shmptr == (void *)-1) {
        err_sys("shmat error");
    }
    printf("shared memory attached from %p to %p\n", (void *)shmptr, (void *)shmptr + SHM_SIZE);

    if (shmctl(shmid, IPC_RMID, NULL) < 0) {
        err_sys("shmctll error");
    }
}
