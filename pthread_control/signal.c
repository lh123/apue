#if defined(__INTELLISENSE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

int quitflag;
sigset_t mask;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t waitloc = PTHREAD_COND_INITIALIZER;

void *thr_fn(void *arg) {
    for(;;) {
        int signo;
        int err = sigwait(&mask, &signo);
        if (err != 0) {
            printf("sigwait failed: %s\n", strerror(err));
            exit(-1);
        }
        switch (signo) {
        case SIGINT:
            printf("\ninterrupt\n");
            break;
        case SIGQUIT:
            pthread_mutex_lock(&lock);
            quitflag = 1;
            pthread_cond_signal(&waitloc);
            pthread_mutex_unlock(&lock);
            return NULL;
        default:
            printf("unexpected signal %d\n", signo);
            exit(-1);
        }
    }
}

int main(void) {
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    
    sigset_t oldmask;
    int err = pthread_sigmask(SIG_BLOCK, &mask, &oldmask);
    if (err != 0) {
        printf("SIG_BLOCK error: %s\n", strerror(err));
        exit(-1);
    }

    pthread_t tid;
    err = pthread_create(&tid, NULL, thr_fn, 0);
    if (err != 0) {
        printf("can't create thread: %s\n", strerror(err));
        exit(-1);
    }

    pthread_mutex_lock(&lock);
    while (quitflag == 0) {
        pthread_cond_wait(&waitloc, &lock);
    }
    pthread_mutex_unlock(&lock);

    quitflag = 0;

    if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {
        perror("SIG_SETMASK error");
    }
} 
