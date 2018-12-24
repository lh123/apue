#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

static volatile sig_atomic_t sync_flag;
static sigset_t oldmask;

static void sync_init(void);
static void wait(void);
static void wake(pid_t pid);
static void sig_usr1(int signo);

int main(void) {
    umask(0);
    int fd = open("a", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    int count = 0;
    if (fd < 0) {
        perror("open error\n");
        exit(-1);
    }

    sync_init();

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork error\n");
        exit(-1);
    } else if (pid == 0) {
        while (1) {
            wait();
            sleep(1);
            if (lseek(fd, SEEK_SET, 0) < 0) {
                perror("lseek error");
                exit(-1);
            }
            if (read(fd, &count, sizeof(count)) != sizeof(count)) {
                perror("read error");
                exit(-1);
            }
            count++;
            if (lseek(fd, SEEK_SET, 0) < 0) {
                perror("lseek error");
                exit(-1);
            }
            if (write(fd, &count, sizeof(count)) != sizeof(count)) {
                perror("write error");
                exit(-1);
            }
            printf("child write : %d\n", count);
            wake(getppid());
        }
        printf("child received quit\n");
        printf("child quit\n");
        wake(getppid());
        exit(0);
    }

    while (1) {
        if (lseek(fd, SEEK_SET, 0) < 0) {
            perror("lseek error");
            exit(-1);
        }
        if (write(fd, &count, sizeof(count)) != sizeof(count)) {
            perror("write error");
            exit(-1);
        }
        printf("parent write : %d\n", count);
        wake(pid);
        wait();
        if (lseek(fd, SEEK_SET, 0) < 0) {
            perror("lseek error");
            exit(-1);
        }
        if (read(fd, &count, sizeof(count)) != sizeof(count)) {
            perror("read error");
            exit(-1);
        }
        count++;
        sleep(1);
    }
    printf("parent received quit\n");
    wait();
    printf("parent quit\n");
}

static void sync_init(void) {
    struct sigaction act;
    act.sa_handler = sig_usr1;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror("sigaction(SIGUSR1) error");
    }

    sigset_t blockmask;
    sigemptyset(&blockmask);
    sigaddset(&blockmask, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &blockmask, &oldmask) < 0) {
        perror("sigprocmask(SIG_BLOCK) error");
    }
}

static void wait(void) {
    sigset_t zeromask;
    sigemptyset(&zeromask);

    sync_flag = 0;
    while (sync_flag == 0) {
        sigsuspend(&zeromask);
    }
}

static void wake(pid_t pid) {
    kill(pid, SIGUSR1);
}

static void sig_usr1(int signo) {
    sync_flag = 1;
}