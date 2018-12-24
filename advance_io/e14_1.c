#include <apue.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>

int lockfile(int fd, int type, off_t start, off_t len) {
    struct flock lock;
    lock.l_type = type;
    lock.l_whence = SEEK_SET;
    lock.l_start = start;
    lock.l_len = len;

    return fcntl(fd, F_SETLK, &lock);
}

int lockfilew(int fd, int type, off_t start, off_t len) {
    struct flock lock;
    lock.l_type = type;
    lock.l_whence = SEEK_SET;
    lock.l_start = start;
    lock.l_len = len;

    return fcntl(fd, F_SETLKW, &lock);
}

void (*signal_intr(int sig, void (*func)(int signo)))(int signo) {
    struct sigaction act, oact;

    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
#if defined(SA_INTERRUPT)
    act.sa_flags |= SA_INTERRUPT;
#endif
    if (sigaction(sig, &act, &oact) < 0) {
        return SIG_ERR;
    }
    return oact.sa_handler;
}

void sigint(int signo) {

}

int main(void) {
    setbuf(stdout, NULL);
    signal_intr(SIGINT, sigint);

    int fd;
    if ((fd = open("lockfile", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
        err_sys("can't open/create lockfile");
    }

    pid_t pid1, pid2, pid3;
    if ((pid1 = fork()) < 0) {
        err_sys("fork failed");
    } else if (pid1 == 0) {
        if (lockfile(fd, F_RDLCK, 0, 0) < 0) {
            err_sys("child 1: can't read-lock file");
        }
        printf("child 1: obtained read lock on file\n");
        pause();
        printf("child 1: exit after pause\n");
        exit(0);
    } else {
        sleep(2);
    }

    if ((pid2 = fork()) < 0) {
        err_sys("fork failed");
    } else if (pid2 == 0) {
        if (lockfile(fd, F_RDLCK, 0, 0) < 0) {
            err_sys("child 2: can't read-lock file");
        }
        printf("child 2: obtained read lock on file\n");
        pause();
        printf("child 2: exit after pause\n");
        exit(0);
    } else {
        sleep(2);
    }

    if ((pid3 = fork()) < 0) {
        err_sys("fork failed");
    } else if (pid3 == 0) {
        if (lockfile(fd, F_WRLCK, 0, 0) < 0) {
            err_ret("child 3: can't write-lock file");
        }
        printf("child 3: about to block write-lock...\n");
        if (lockfilew(fd, F_WRLCK, 0, 0) < 0) {
            err_sys("child 3: can't write-lock file");
        }
        printf("child 3 returned and got write lock????\n");
        pause();
        printf("child 3: exit after pause\n");
        exit(0);
    } else {
        sleep(2);
    }

    if (lockfile(fd, F_RDLCK, 0, 0) < 0) {
        printf("parent: can't set read lock: %s\n", strerror(errno));
    } else {
        printf("parent: obtained additional read lock while write lock is pending\n");
    }
    printf("killing child 1...\n");
    kill(pid1, SIGINT);
    printf("killing child 2...\n");
    kill(pid2, SIGINT);
    printf("killing child 3...\n");
    kill(pid3, SIGINT);
}
