#include "slock.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

struct slock *s_alloc(void) {
    struct slock *sp;
    static int cnt;
    if ((sp = malloc(sizeof(struct slock))) == NULL) {
        return NULL;
    }
    do {
        snprintf(sp->name, sizeof(sp->name) - 1, "/%ld.%d", (long)getpid(), cnt++);
        sp->semp = sem_open(sp->name, O_CREAT | O_EXCL, S_IRWXU, 1); // O_EXCL 如果文件存则errno = EEXIST
    } while((sp->semp == SEM_FAILED) && (errno == EEXIST));

    if (sp->semp == SEM_FAILED) {
        free(sp);
        return NULL;
    }
    sem_unlink(sp->name); // 会延迟销毁，前面调用sem_open打开了信号量
    return sp;
}

void s_free(struct slock *sp) {
    sem_close(sp->semp);
    free(sp);
}

int s_lock(struct slock *sp) {
    return sem_wait(sp->semp);
}

int s_trylock(struct slock *sp) {
    return sem_trywait(sp->semp);
}

int s_unlock(struct slock *sp) {
    return sem_post(sp->semp);
}
