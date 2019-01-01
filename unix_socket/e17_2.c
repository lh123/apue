#include <apue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/socket.h>

#define FILE_OPEN "./tt.out"

static volatile sig_atomic_t flag;
static sigset_t oldset;

static void sig_usr(int signo) {
    flag = 1;
}

static void sync_init(void) {
    struct sigaction act;
    act.sa_handler = sig_usr;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        err_sys("sigaction for SIGUSR1 error");
    }

    // 防止第一次有可能会丢失sigusr1
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &set, &oldset) < 0) {
        err_sys("sigprocmask error");
    }
}

static void sync_wait(void) {
    sigset_t zero;
    sigemptyset(&zero);
    while (flag == 0) {
        sigsuspend(&zero);
    }
    flag = 0;

    if (sigprocmask(SIG_SETMASK, &oldset, NULL) < 0) {
        err_sys("sigprocmask error");
    }
}

static void sync_wake(pid_t pid) {
    kill(pid, SIGUSR1);
}

static int my_send_fd(int fd, int fd_to_send) {
    static struct cmsghdr *cmptr = NULL;
    struct msghdr msg;

    char data = 'f';
    struct iovec iov;
    iov.iov_base = &data;
    iov.iov_len = sizeof(char);

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    
    if (cmptr == NULL) {
        if ((cmptr = malloc(CMSG_SPACE(sizeof(int)))) == NULL) {
            err_sys("malloc error");
        }
    }
    cmptr->cmsg_len = CMSG_LEN(sizeof(int));
    cmptr->cmsg_level = SOL_SOCKET;
    cmptr->cmsg_type = SCM_RIGHTS;
    *CMSG_DATA(cmptr) = fd_to_send;

    msg.msg_control = cmptr;
    msg.msg_controllen = CMSG_SPACE(sizeof(int));

    if (sendmsg(fd, &msg, 0) != sizeof(char)) {
        return -1;
    }
    return 0;
}

static int my_recv_fd(int fd) {
    static struct cmsghdr *cmptr = NULL;
    struct msghdr msg;

    char buf[MAXLINE];
    struct iovec iov;
    

    if (cmptr == NULL) {
        if ((cmptr = malloc(CMSG_SPACE(sizeof(int)))) == NULL) {
            err_sys("malloc error");
        }
    }
    
    for (;;) {
        iov.iov_base = buf;
        iov.iov_len = MAXLINE;

        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_control = cmptr;
        msg.msg_controllen = CMSG_SPACE(sizeof(int));

        int n = recvmsg(fd, &msg, 0);
        if (n < 0) {
            err_sys("recvmsg error");
        } else if (n == 0) {
            err_ret("connection closed by server");
            return -1;
        } else {
            if (n == sizeof(char) && buf[0] == 'f') {
                if (msg.msg_controllen != CMSG_SPACE(sizeof(int))) {
                    err_quit("message format error");
                }
                int fd = *(int *)CMSG_DATA(cmptr);
                return fd;
            }
        }
    }
    return -1;
}

int main(void) {
    sync_init();
    int fd[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fd) < 0) {
        err_sys("socketpair error");
    }
    pid_t pid = fork();
    if (pid < 0) {
        err_sys("fork error");
    } else if (pid == 0) {
        // child
        close(fd[0]);
        int filefd = open(FILE_OPEN, O_RDWR);
        if (filefd < 0) {
            err_sys("open '%s' error", FILE_OPEN);
        }
        printf("child send fd %d over %d\n", filefd, fd[1]);
        if (my_send_fd(fd[1], filefd) < 0) {
            err_sys("send_fd error");
        }
        printf("child lseek to 10 byte offset BEGIN\n");
        if (lseek(filefd, 10, SEEK_SET) < 0) {
            err_sys("lseek error");
        }
        sync_wake(getppid());
        sync_wait();

        printf("child lseek to BEGIN\n");
        if (lseek(filefd, 0, SEEK_SET) < 0) {
            err_sys("lseek error");
        }
        sync_wake(getppid());
        close(filefd);
    } else {
        close(fd[1]);
        int filefd = my_recv_fd(fd[0]);
        if (filefd < 0) {
            err_sys("recv_fd error");
        }
        printf("parent recv fd %d from %d\n", filefd, fd[0]);
        sync_wait();
        int offset = lseek(filefd, 0, SEEK_CUR);
        if (offset < 0) {
            err_sys("lseek error");
        }
        printf("parent offset %d byte\n", offset);
        
        sync_wake(pid);
        sync_wait();
        
        offset = lseek(filefd, 0, SEEK_CUR);
        if (offset < 0) {
            err_sys("lseek error");
        }
        printf("parent offset %d byte\n", offset);

        printf("\nfile content:\n");
        char buf[MAXLINE];
        // FILE *file = fdopen(filefd, "r");
        // if (file == NULL) {
        //     err_sys("fdopen error");
        // }
        // while (fgets(buf, MAXLINE, file) != NULL) {
        //     if (fputs(buf, stdout) == EOF) {
        //         err_sys("fputs error");
        //     }
        // }
        // if (ferror(file)) {
        //     err_sys("fgets error");
        // }
        // fclose(file);
        int n;
        while ((n = read(filefd, buf, MAXLINE)) > 0) {
            if (write(STDOUT_FILENO, buf, n) != n) {
                err_sys("write error");
            }
        }
        if (n < 0) {
            err_sys("read error");
        }
        waitpid(pid, NULL, 0);
    }
}
