#if defined(__INTELLISENSE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

extern char **environ;

pthread_mutex_t env_mutex;

static pthread_once_t init_done = PTHREAD_ONCE_INIT;

static void thread_init(void) {
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); // 设置成递归锁，因为有可能在信号处理函数中调用
    pthread_mutex_init(&env_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

int getenv_r(const char *name, char *buf, int size) {
    pthread_once(&init_done, thread_init);

    int len = strlen(name);
    pthread_mutex_lock(&env_mutex);
    for (int i = 0; environ[i] != NULL; i++) {
        if ((strncmp(name, environ[i], len) == 0) && (environ[i][len] == '=')) {
            int olen = strlen(&environ[i][len + 1]);
            if (olen >= size) {
                pthread_mutex_unlock(&env_mutex);
                return ENOSPC;
            }
            strcpy(buf, &environ[i][len + 1]);
            pthread_mutex_unlock(&env_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&env_mutex);
    return ENOENT;
}
