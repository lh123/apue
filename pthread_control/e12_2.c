#if defined(__INTELLISENSE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

extern char **environ;

static pthread_mutex_t env_lock;
static pthread_once_t init_done = PTHREAD_ONCE_INIT;

static void thread_init(void) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); //保证重入时不会死锁

    pthread_mutex_init(&env_lock, &attr);
    pthread_mutexattr_destroy(&attr);
}

int my_putenv_r(char *string) {
    pthread_once(&init_done, thread_init);

    sigset_t mask, oldmask;
    sigfillset(&mask);

    // 阻塞所有信号，因为后面要调用relloc
    int err = pthread_sigmask(SIG_BLOCK, &mask, &oldmask);
    if (err != 0) {
        errno = err;
        return -1;
    }
    
    char *key_ptr = strchr(string, '=');
    if (key_ptr == NULL) {
        errno = EINVAL;
        return -1;
    }
    int key_len = key_ptr - string;

    pthread_mutex_lock(&env_lock); //需要查询和修改全局变量
    int i;
    for (i = 0; environ[i] != NULL; i++) {
        if ((strncmp(environ[i], string, key_len) == 0) && environ[i][key_len] == '=') {
            environ[i] = string;
            err = pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
            if (err != 0) {
                errno = err;
                exit(-1);
            }
            pthread_mutex_unlock(&env_lock);
            return 0;
        }
    }
    //update env
    char **new_environ = malloc((i + 2) * sizeof(char *));
    memcpy(new_environ, environ, (i + 1) * sizeof(char *));
    environ = new_environ;
    environ[i] = string;
    environ[i + 1] = NULL;
    err = pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
    if (err != 0) {
        errno = err;
        exit(-1);
    }
    pthread_mutex_unlock(&env_lock);
    return 0;
}

void *thr_fn(void *arg) {
    char *env = malloc(4096);
    snprintf(env, 4095, "TEST_%d=TEST", (int)((unsigned long)(arg)));
    if (my_putenv_r(env) < 0) {
        perror("putenv error");
    }
    return NULL;
}

#define THR_NUM 50

int main(void) {
    int i;
    
    for (i = 0; i < THR_NUM; i++) {
        pthread_t tid;
        int err = pthread_create(&tid, NULL, thr_fn, (void *)((unsigned long)i));
        if (err != 0) {
            printf("can't create thread: %s\n", strerror(err));
            exit(-1);
        }
    }

    sleep(5);

    for (i = 0; environ[i] != NULL; i++) {
        printf("%s\n", environ[i]);
    }
}
