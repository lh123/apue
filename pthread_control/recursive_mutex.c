#if defined(__INTELLISENSE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

int makethread(void *(*fn)(void *), void *arg) {
    pthread_attr_t attr;
    int err = pthread_attr_init(&attr);
    if (err != 0) {
        return err;
    }
    err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (err != 0) {
        pthread_attr_destroy(&attr);
        return err;
    }
    pthread_t ptid;
    err = pthread_create(&ptid, &attr, fn, arg);
    pthread_attr_destroy(&attr);
    return err;
}

struct to_info {
    void (*to_fn)(void *);      // function
    void *to_arg;               // argument
    struct timespec to_wait;    // time to wait
};

#define SECTONSEC 1000000000

void *timeout_helper(void *arg) {
    struct to_info *tip = (struct to_info *)arg;
    clock_nanosleep(CLOCK_REALTIME, 0, &tip->to_wait, NULL);
    tip->to_fn(tip->to_arg);
    free(tip);
    return NULL;
}

void timeout(const struct timespec *when, void (*func)(void *), void *arg) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    if ((when->tv_sec > now.tv_sec) || (when->tv_sec == now.tv_sec && when->tv_nsec > now.tv_nsec)) {
        struct to_info *tip = malloc(sizeof(struct to_info));
        if (tip != NULL) {
            tip->to_fn = func;
            tip->to_arg = arg;
            tip->to_wait.tv_sec = when->tv_sec - now.tv_sec;
            if (when->tv_nsec >= now.tv_nsec) {
                tip->to_wait.tv_nsec = when->tv_nsec - now.tv_nsec;
            } else {
                tip->to_wait.tv_sec--;
                tip->to_wait.tv_nsec = SECTONSEC + when->tv_nsec - now.tv_nsec;
            }
            int err = makethread(timeout_helper, (void *)tip);
            if (err != 0) {
                free(tip);
            } else {
                return;
            }
        }
    }
    func(arg);
}

pthread_mutexattr_t attr;
pthread_mutex_t mutex;

void retry(void *arg) {
    pthread_mutex_lock(&mutex);
    // perform retry steps ...
    printf("retry\n");
    pthread_mutex_unlock(&mutex);
}

int main(void) {
    int condition = 1, arg = 0;

    int err = pthread_mutexattr_init(&attr);
    if (err != 0) {
        printf("pthread_mutexattr_init failed: %s\n", strerror(err));
        exit(-1);
    }
    err = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (err != 0) {
        printf("can't set recursive type: %s\n", strerror(err));
        exit(-1);
    }
    err = pthread_mutex_init(&mutex, &attr);
    if (err != 0) {
        printf("can't create recursive mutex: %s\n", strerror(err));
        exit(-1);
    }

    pthread_mutex_lock(&mutex);
    if (condition) {
        struct timespec when;
        clock_gettime(CLOCK_REALTIME, &when);
        when.tv_sec += 10;
        timeout(&when, retry, (void *)((unsigned long)arg));
    }
    pthread_mutex_unlock(&mutex);

    sleep(16);
}
