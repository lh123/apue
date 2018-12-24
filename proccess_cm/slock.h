#ifndef SLOCK_H
#define SLOCK_H

#include <apue.h>
#include <stdlib.h>
#include <limits.h>
#include <semaphore.h>

struct slock {
    sem_t *semp;
    char name[_POSIX_NAME_MAX];
};

struct slock * __attribute__((weak)) s_alloc(void);
void s_free(struct slock *sp);
int s_lock(struct slock *sp);
int s_trylock(struct slock *sp);
int s_unlock(struct slock *sp);

#endif