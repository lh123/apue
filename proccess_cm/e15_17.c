#include <apue.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define LOCK_FILE "lock"
#define LOOP_CNT 10000

static int lockfileW(int fd) {
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;
    while (fcntl(fd, F_SETLKW, &fl) < 0) {
        if (errno == EINTR) {
            continue;
        } else {
            return -1;
        }
    }
    return 0;
}

static int unlockfile(int fd) {
    struct flock fl;
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;
    return fcntl(fd, F_SETLKW, &fl);
}

static long update(long *ptr) {
    return (*ptr)++;
}

int main(void) {
    int fd = open("/dev/zero", O_RDWR);
    int lockfd = open(LOCK_FILE, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0 || lockfd < 0) {
        err_sys("open failed");
    }
    unlink(LOCK_FILE);
    
    void *area = mmap(NULL, sizeof(long), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (area == MAP_FAILED) {
        err_sys("mmap error");
    }
    close(fd);
    lockfileW(lockfd);
    int num = 0;
    write(lockfd, &num, sizeof(int));

    pid_t pid = fork();
    if (pid < 0) {
        err_sys("fork error");
    } else if (pid > 0) {
        // parent
        int counter = 0;
        for (int i = 0; i < LOOP_CNT; i += 2) {
            printf("parent\n");
            if ((counter = update((long *)area)) != i) {
                err_quit("parent: expected %d but got %d", i, counter);
            }
            num = 1;
            lseek(lockfd, 0, SEEK_SET);
            if (write(lockfd, &num, sizeof(int)) != sizeof(int)) {
                err_sys("write error");
            }
            unlockfile(lockfd);
            while (1) {
                lockfileW(lockfd);
                lseek(lockfd, 0, SEEK_SET);
                if (read(lockfd, &num, sizeof(int)) != sizeof(int)) {
                    err_sys("read error");
                }
                if (num == 0) {
                    break;
                }
                unlockfile(lockfd);
            }
        }
        while (waitpid(pid, NULL, 0) < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                err_sys("waitpid error");
            }
        }
        printf("parent: counter = %d\n", counter);
    } else {
        int counter = 1;
        for (int i = 1; i < LOOP_CNT; i += 2) {
            while (1) {
                lockfileW(lockfd);
                lseek(lockfd, 0, SEEK_SET);
                if (read(lockfd, &num, sizeof(int)) != sizeof(int)) {
                    err_sys("read error");
                }
                if (num == 1) {
                    break;
                }
                unlockfile(lockfd);
            }
            printf("child\n");
            if ((counter = update((long *)area)) != i) {
                err_quit("child: expected %d but got %d", i, counter);
            }
            num = 0;
            lseek(lockfd, 0, SEEK_SET);
            if (write(lockfd, &num, sizeof(int)) != sizeof(int)) {
                err_sys("write error");
            }
            unlockfile(lockfd);
        }
        printf("child: counter = %d\n", counter);
    }
}
