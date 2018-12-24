#include <apue.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>

#define NLOOPS 1000
#define SIZE sizeof(long)

static sig_atomic_t wait_flag;

void sig_usr1(int signo) {
    wait_flag = 1;
}

void TELL_WAIT(void) {
    struct sigaction act;
    act.sa_handler = sig_usr1;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        err_sys("sigaction SIGUSR1 error");
    }
    wait_flag = 0;

    sigset_t blockset;
    sigemptyset(&blockset);
    sigaddset(&blockset, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &blockset, NULL) < 0) {
        err_sys("sigprocmask SIG_BLOCK error");
    }
}

void WAIT(void) {
    sigset_t waitset;
    sigfillset(&waitset);
    sigdelset(&waitset, SIGUSR1);
    wait_flag = 0;
    while (wait_flag == 0) {
        sigsuspend(&waitset);
    }

    // sigset_t zeroset;
    // sigemptyset(&zeroset);
    // if (sigprocmask(SIG_SETMASK, &zeroset, NULL) < 0) {
    //     err_sys("sigprocmask SIG_SETMASK error");
    // }
}

void WAKE(pid_t pid) {
    kill(pid, SIGUSR1);
}


static int update(long *ptr) {
    return (*ptr)++;
}

int main(void) {
    int fd = open("/dev/zero", O_RDWR);
    if (fd < 0) {
        err_sys("open error");
    }
    void *area = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (area == MAP_FAILED) {
        err_sys("mmap error");
    }
    close(fd);

    TELL_WAIT();

    pid_t pid = fork();
    if (pid < 0) {
        err_sys("fork error");
    } else if (pid > 0) {
        int counter;
        for (int i = 0; i < NLOOPS; i += 2) {
            if ((counter = update((long *)area)) != i) {
                err_quit("parent: expected %d, got %d", i, counter);
            }
            WAKE(pid);
            WAIT();
        }
        printf("in parent: counter = %ld\n", *(long *)area);
    } else {
        int counter;
        for (int i = 1; i < NLOOPS; i += 2) {
            WAIT();
            if ((counter = update((long *)area)) != i) {
                err_quit("child: expected %d, got %d", i, counter);
            }
            WAKE(getppid());
        }
        printf("in child: counter = %ld\n", *(long *)area);
    }
}
