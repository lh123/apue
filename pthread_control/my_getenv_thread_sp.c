#if defined(__INTELLISENSE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define MAXSTRINGSZ 4096

static pthread_key_t key;
static pthread_once_t init_done = PTHREAD_ONCE_INIT;
pthread_mutex_t env_mutex = PTHREAD_MUTEX_INITIALIZER;

extern char **environ;

static void thread_init(void) {
    pthread_key_create(&key, free); // 没必要使用递归锁来确保异步信号安全，因为调用了malloc，malloc本身就已经不是异步信号安全的了
}

// 异步信号不安全，不可重入
char *my_getenv(const char *name) {
    pthread_once(&init_done, thread_init);

    pthread_mutex_lock(&env_mutex);
    char *env_buf = pthread_getspecific(key);
    if (env_buf == NULL) {
        env_buf = malloc(MAXSTRINGSZ);
        if (env_buf == NULL) {
            pthread_mutex_unlock(&env_mutex);
            return NULL;
        }
        pthread_setspecific(key, env_buf);
    }
    int len = strlen(name);
    for (int i = 0; environ[i] != NULL; i++) {
        if ((strncmp(name, environ[i], len) == 0) && (environ[i][len] == '=')) {
            strncpy(env_buf, &environ[i][len + 1], MAXSTRINGSZ);
            pthread_mutex_unlock(&env_mutex);
            return env_buf;
        }
    }
    pthread_mutex_unlock(&env_mutex);
    return NULL;
}

void *thr_fn(void *arg) {
    printf("thread %lu -- path=%s\n", (unsigned long)pthread_self(), getenv("PATH"));
    return NULL;
}

#define TH_NUM 10

int main(void) {
    for (int i = 0; i < TH_NUM; i++) {
        pthread_t ptid;
        pthread_create(&ptid, NULL, thr_fn, NULL);
    }
    sleep(1);
}
