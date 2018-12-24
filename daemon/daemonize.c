#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/resource.h>

void daemonize(const char *cmd) {
    umask(0);

    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        printf("%s: can't get file limit: %s\n", cmd, strerror(errno));
        exit(1);
    }

    pid_t pid = fork();
    if (pid < 0) {
        printf("%s: can't fork: %s\n", cmd, strerror(errno));
        exit(1);
    } else if (pid != 0) {
        exit(0);
    }

    setsid();

    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        printf("%s: can't ignore SIGHUP: %s\n", cmd, strerror(errno));
        exit(1);
    }

    // 让该进程不是会话首进程，避免获得终端
    pid = fork();
    if (pid < 0) {
        printf("%s: can't fork: %s\n", cmd, strerror(errno));
        exit(1);
    } else if (pid != 0) {
        exit(0);
    }

    if (chdir("/") < 0) {
        printf("%s: can't change directory to /: %s\n", cmd, strerror(errno));
        exit(1);
    }
    
    if (rl.rlim_max == RLIM_INFINITY) {
        rl.rlim_max = 1024;
    }
    for (int i = 0; i < rl.rlim_max; i++) {
        close(i);
    }

    int fd0 = open("/dev/null", O_RDWR);
    int fd1 = dup(0);
    int fd2 = dup(0);

    openlog(cmd, LOG_CONS, LOG_DAEMON);
    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        syslog(LOG_ERR, "unexpected file dsecriptors %d %d %d", fd0, fd1, fd2);
        exit(1);
    }
}

int main(void) {
    daemonize("my_daemon");
    syslog(LOG_INFO, "hello world");
    sleep(10);
}
