#include <apue.h>
#include <fcntl.h>
#include <unistd.h>

static int lockfd;

void TELL_WAIT(int isparent) {
    if (lockfd == 0) {
        lockfd = open("waitlock", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (lockfd < 0) {
            err_sys("open waitlock failed");
        }
    }
    char byte[2] = {0, 1};
    write(lockfd, byte, 2);

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_len = 1;
    if (isparent) {
        lock.l_start = 0;
    } else {
        lock.l_start = 1;
    }
    fcntl(lockfd, F_SETLK, &lock);
}

void TELL_PARENT(void) {
    struct flock cldl;
    cldl.l_type = F_WRLCK;
    cldl.l_whence = SEEK_SET;
    cldl.l_start = 1;
    cldl.l_len = 1;

    fcntl(lockfd, F_UNLCK, &cldl);
}

void TELL_CHILD(void) {
    struct flock cldl;
    cldl.l_type = F_WRLCK;
    cldl.l_whence = SEEK_SET;
    cldl.l_start = 0;
    cldl.l_len = 1;

    fcntl(lockfd, F_UNLCK, &cldl);
}

void WAIT_PARENT(void) {
    struct flock cldl;
    cldl.l_type = F_WRLCK;
    cldl.l_whence = SEEK_SET;
    cldl.l_start = 0;
    cldl.l_len = 1;

    fcntl(lockfd, F_SETLKW, &cldl);

    fcntl(lockfd, F_UNLCK, &cldl);
}

void WAIT_CHLID(void) {
    struct flock pral;
    pral.l_type = F_WRLCK;
    pral.l_whence = SEEK_SET;
    pral.l_start = 1;
    pral.l_len = 1;

    fcntl(lockfd, F_SETLKW, &pral);

    fcntl(lockfd, F_UNLCK, &pral);
}

int main(void) {
    pid_t pid = fork();
    if (pid < 0) {
        err_sys("fork error");
    } else if (pid == 0) {
        TELL_WAIT(0);
        sleep(1);

        printf("child init\n");
        printf("child sleep 5s\n");
        sleep(5);
        TELL_PARENT();
        printf("wake parent\n");
        printf("child exit\n");;
    } else {
        sleep(1);
        TELL_WAIT(1);

        printf("parent init\n");
        printf("waiting child\n");
        WAIT_CHLID();
        printf("parent exit\n");;
    }
}
