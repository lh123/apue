#if defined(__INTELLISENSE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;

void prepare(void) {
    printf("preparing locks...\n");
    int err = pthread_mutex_lock(&lock1);
    if (err != 0) {
        printf("can't lock lock1 in prepare handler: %s\n", strerror(err));
    }
    err = pthread_mutex_lock(&lock2);
    if (err != 0) {
        printf("can't lock lock2 in prepare handler: %s\n", strerror(err));
    }
}

void parent(void) {
    printf("parent unlocking locks...\n");
    int err = pthread_mutex_unlock(&lock1);
    if (err != 0) {
        printf("can't unlock lock1 in parent handler: %s\n", strerror(err));
    }
    err = pthread_mutex_unlock(&lock2);
    if (err != 0) {
        printf("can't unlock lock2 in parent handler: %s\n", strerror(err));
    }
}

void child(void) {
    printf("child unlocking locks...\n");
    int err = pthread_mutex_unlock(&lock1);
    if (err != 0) {
        printf("can't unlock lock1 in child handler: %s\n", strerror(err));
    }
    err = pthread_mutex_unlock(&lock2);
    if (err != 0) {
        printf("can't unlock lock2 in child handler: %s\n", strerror(err));
    }
}

void *thr_fn(void *arg) {
    printf("thread started...\n");
    pause();
    return NULL;
}

int main(void) {
    int err = pthread_atfork(prepare, parent, child);
    if (err != 0) {
        printf("can't install fork handlers: %s\n", strerror(err));
        exit(1);
    }
    pthread_t tid;
    err = pthread_create(&tid, NULL, thr_fn, 0);
    if (err != 0) {
        printf("can't create thread: %s\n", strerror(err));
        exit(1);
    }

    sleep(2);
    printf("parent about to fork...\n");

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(1);
    } else if (pid == 0) {
        printf("child returned from fork\n");
    } else {
        printf("parent returned from fork\n");
    }
}
