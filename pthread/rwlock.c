#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

struct job {
    struct job *j_next;
    struct job *j_pre;
    pthread_t j_id;
};

struct queue {
    struct job *q_head;
    struct job *q_tail;
    pthread_rwlock_t q_lock;
};

int queue_init(struct queue *qp) {
    int err;

    qp->q_head = NULL;
    qp->q_tail = NULL;
    err = pthread_rwlock_init(&qp->q_lock, NULL);
    if (err != 0) {
        return err;
    }
    return 0;
}

static void queue_destory(struct queue *qp) {
    pthread_rwlock_wrlock(&qp->q_lock);
    pthread_rwlock_unlock(&qp->q_lock);
    pthread_rwlock_destroy(&qp->q_lock);
}


void job_insert(struct queue *qp, struct job *jp) {
    pthread_rwlock_wrlock(&qp->q_lock);
    jp->j_next = qp->q_head;
    jp->j_pre = NULL;
    if (qp->q_head != NULL) {
        qp->q_head->j_pre = jp;
    } else {
        qp->q_tail = jp;
    }
    qp->q_head = jp;
    pthread_rwlock_unlock(&qp->q_lock);
}

void job_append(struct queue *qp, struct job *jp) {
    pthread_rwlock_wrlock(&qp->q_lock);
    jp->j_next = NULL;
    jp->j_pre = qp->q_tail;
    if (qp->q_tail != NULL) {
        qp->q_tail->j_next = jp;
    } else {
        qp->q_head = jp;
    }
    qp->q_tail = jp;
    pthread_rwlock_unlock(&qp->q_lock);
}

void job_remove(struct queue *qp, struct job *jp) {
    pthread_rwlock_wrlock(&qp->q_lock);
    if (jp == qp->q_head) {
        qp->q_head = jp->j_next;
        if (qp->q_tail == jp) {
            qp->q_tail = NULL;
        } else {
            jp->j_next->j_pre = jp->j_pre;
        }
    } else if (jp == qp->q_tail) {
        qp->q_tail = jp->j_pre;
        jp->j_pre->j_next = jp->j_next;
    } else {
        jp->j_pre->j_next = jp->j_next;
        jp->j_next->j_pre = jp->j_pre;
    }
    pthread_rwlock_unlock(&qp->q_lock);
}

struct job *job_find(struct queue *qp, pthread_t id) {
    struct job *jp;

    if (pthread_rwlock_rdlock(&qp->q_lock) != 0) {
        return NULL;
    }
    for (jp = qp->q_head; jp != NULL; jp = jp->j_next) {
        if (pthread_equal(jp->j_id, id)) {
            break;
        }
    }
    pthread_rwlock_unlock(&qp->q_lock);
    return jp;
}

static void *costumer_thread1(void *arg);
static void *costumer_thread2(void *arg);
static void sig_intr(int signo);

static struct queue que;
static volatile sig_atomic_t quit_flag;

int main(void) {
    int err;
    pthread_t tid1, tid2;
    struct job *jp;

    if (signal(SIGINT, sig_intr) == SIG_ERR) {
        perror("signal(SIGINT) error");
        exit(-1);
    }

    queue_init(&que);
    err = pthread_create(&tid1, NULL, costumer_thread1, (void *)1);
    if (err != 0) {
        printf("create thread 1 error\n");
        exit(-1);
    }
    err = pthread_create(&tid2, NULL, costumer_thread2, (void *)2);
    if (err != 0) {
        printf("create thread 1 error\n");
        exit(-1);
    }

    while (!quit_flag) {
        jp = malloc(sizeof(struct job));
        jp->j_id = tid1;
        job_append(&que, jp);

        sleep(1);

        jp = malloc(sizeof(struct job));
        jp->j_id = tid2;
        job_append(&que, jp);
    }

    err = pthread_join(tid1, NULL);
    if (err != 0) {
        printf("join with thread 1 error: %s\n", strerror(err));
    }
    err = pthread_join(tid2, NULL);
    if (err != 0) {
        printf("join with thread 2 error: %s\n", strerror(err));
    }
    queue_destory(&que);
    printf("main thread quit\n");

}


static void *costumer_thread1(void *arg) {
    while(!quit_flag) {
        struct job *jp = job_find(&que, pthread_self());
        if (jp != NULL) {
            job_remove(&que, jp);
            printf("thread 1 get job %lu\n", jp->j_id);
            free(jp);
        }
    }
    printf("thread 1 quit\n");
    pthread_exit(NULL);
}

static void *costumer_thread2(void *arg) {
    while(!quit_flag) {
        struct job *jp = job_find(&que, pthread_self());
        if (jp != NULL) {
            job_remove(&que, jp);
            printf("thread 2 get job %lu\n", jp->j_id);
            free(jp);
        }
    }
    printf("thread 2 quit\n");
    pthread_exit(NULL);
}

static void sig_intr(int signo) {
    if (signo == SIGINT) {
        printf("\ncaught SIGINT\n");
        quit_flag = 1;
    }
}
