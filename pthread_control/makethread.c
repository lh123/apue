#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

int makethread(pthread_t *tpid, void *(*fn)(void *), void *arg) {
    pthread_attr_t attr;
    int err = pthread_attr_init(&attr);
    if (err != 0) {
        return err;
    }
    err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (err == 0) {
        err = pthread_create(tpid, &attr, fn, arg);
    }
    pthread_attr_destroy(&attr);
    return err;
}

static void *thr_fn(void *arg) {
    printf("thread 0x%lx started\n", (unsigned long)pthread_self());
    sleep(1);
    printf("thread 0x%lx finished\n", (unsigned long)pthread_self());
    return NULL;
}

int main(void) {
    pthread_t ptid;
    int err = makethread(&ptid, thr_fn, NULL);
    if (err != 0) {
        printf("makethread error: %s\n", strerror(err));
        exit(-1);
    }
    err = pthread_join(ptid, NULL);
    if (err != 0) {
        printf("join with thread 0x%lx fail: %s\n", (unsigned long)ptid, strerror(err));
    } else {
        printf("join with thread 0x%lx success\n", (unsigned long)ptid);
    }
    sleep(2);
}
