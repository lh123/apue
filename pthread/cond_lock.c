#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

struct msg {
    struct msg *m_next;
    char *str;
};

struct msg *workq;

static pthread_cond_t qready = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;
static sig_atomic_t quit_flag;

void process_msg(void) {
    struct msg *mp;

    while (!quit_flag) {
        pthread_mutex_lock(&qlock);
        while (workq == NULL) {
            pthread_cond_wait(&qready, &qlock); // 调用时对互斥量解锁，被条件唤醒时对互斥量重新加锁
            if (quit_flag) {
                pthread_mutex_unlock(&qlock);
                return;
            }
        }
        mp = workq;
        workq = workq->m_next;
        pthread_mutex_unlock(&qlock);

        printf("proccess msg: %s\n", mp->str);
        free(mp->str);
        free(mp);
    }
    
}

void enqueue_msg(const char *msg) {
    struct msg *mp;
    
    pthread_mutex_lock(&qlock);
    mp = malloc(sizeof(struct msg));
    if (mp == NULL) {
        pthread_mutex_unlock(&qlock);
        return;
    }
    mp->str = malloc(strlen(msg) + 1);
    if (mp->str == NULL) {
        free(mp);
        pthread_mutex_unlock(&qlock);
        return;
    }
    strcpy(mp->str, msg);
    mp->m_next = workq;
    workq = mp;
    pthread_cond_signal(&qready);
    pthread_mutex_unlock(&qlock);
}

static void sig_intr(int signo) {
    if (signo == SIGINT) {
        quit_flag = 1;
    }
}

static void *proccess_thread(void *arg) {
    process_msg();
    printf("thread \"proccess_thread\" quit\n");
    pthread_exit(NULL);
}

int main(void) {
    int err;
    pthread_t tid;

    if (signal(SIGINT, sig_intr) == SIG_ERR) {
        perror("signal(SIGINT) error");
        exit(-1);
    }

    err = pthread_create(&tid, NULL, proccess_thread, NULL);
    if (err != 0) {
        printf("create thread \"process_msg\" error\n");
        exit(-1);
    }

    while (!quit_flag) {
        printf("enqueue_msg\n");
        enqueue_msg("hello world");
        sleep(1);
    }
    pthread_cond_broadcast(&qready);
    err = pthread_join(tid, NULL);
    if (err != 0) {
        printf("join with thread %lu error: %s\n", (unsigned long)tid, strerror(err));
    }
    printf("main thread quit\n");
}
