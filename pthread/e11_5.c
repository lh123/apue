#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define MY_BARRIER_INITIALIZER(X) {(X), PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER}

struct my_barrier {
    unsigned int count;
    pthread_mutex_t b_lock;
    pthread_cond_t b_cond;
};

typedef struct my_barrier my_barrier_t;

static int __attribute__((unused)) my_barrier_init(my_barrier_t *barrier, unsigned int count) {
    int err;
    barrier->count = count;
    err = pthread_mutex_init(&barrier->b_lock, NULL);
    if (err != 0) {
        return err;
    }
    err = pthread_cond_init(&barrier->b_cond, NULL);
    if (err != 0) {
        pthread_mutex_destroy(&barrier->b_lock);
        return err;
    }
    return err;
}

int __attribute__((unused)) my_barrier_destory(my_barrier_t *barrier) {
    pthread_mutex_destroy(&barrier->b_lock);
    int err = pthread_cond_destroy(&barrier->b_cond);
    return err;
}

int __attribute__((unused)) my_barrier_wait(my_barrier_t *barrier) {
    pthread_mutex_lock(&barrier->b_lock);
    if (barrier->count > 0) {
        barrier->count--;
    }
    if (barrier->count == 0) {
#if !defined(NDEBUG)
        printf("thread %lu send broadcast\n", (unsigned long)pthread_self());
#endif
        pthread_cond_broadcast(&barrier->b_cond);
    } else {
        while (barrier->count != 0) {
            pthread_cond_wait(&barrier->b_cond, &barrier->b_lock);
#if !defined(NDEBUG)
            printf("thread %lu wake\n", (unsigned long)pthread_self());
#endif
        }
    }
    pthread_mutex_unlock(&barrier->b_lock);
    return 0;
}

#define THR_CNT 5

my_barrier_t b;// = MY_BARRIER_INITIALIZER(THR_CNT + 1);

void *thr_fn(void *arg) {
    printf("thread %lu started\n", (unsigned long)pthread_self());
    sleep(1);
    printf("thread %lu finish\n", (unsigned long)pthread_self());
    my_barrier_wait(&b);
    return NULL;
}


int main(void) {
    my_barrier_init(&b, THR_CNT + 1);

    long i;
    for (i = 0; i < THR_CNT; i++) {
        pthread_t ptid;
        int err = pthread_create(&ptid, NULL, thr_fn, NULL);
        if (err != 0) {
            printf("create thread %ld error: %s\n", i + 1, strerror(err));
            exit(-1);
        }
    }
    sleep(1);
    printf("main thread %ld wait\n", (unsigned long)pthread_self());
    my_barrier_wait(&b);
    printf("main thread %ld end\n", (unsigned long)pthread_self());
}
