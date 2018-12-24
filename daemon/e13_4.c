#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>

void __attribute__((noreturn, format(printf, 1, 2))) err_quit(const char *fmt, ...);

void daemonize(const char *cmd) {
    umask(0);
    struct rlimit rl;

    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        err_quit("getrlimit error");
    }

    pid_t pid = fork();
    if (pid < 0) {
        err_quit("fork error");
    } else if (pid != 0) {
        exit(0);
    }
    setsid();

    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        err_quit("%s: can't ignore SIGHUP", cmd);
    }

    // 这段fork是为了让该进程不成为会话首进程
    pid = fork();
    if (pid < 0) {
        err_quit("fork error");
    } else if (pid != 0) {
        exit(0);
    }

    if (chdir("/") < 0) {
        err_quit("%s: can't change directory to /", cmd);
    }

    if (rl.rlim_max == RLIM_INFINITY) {
        rl.rlim_max = 1024;
    }
    for (int i = 0; i < rl.rlim_max; i++) {
        close(i);
    }

    int fd0, fd1, fd2;
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);

    openlog(cmd, LOG_CONS, LOG_DAEMON);
    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        syslog(LOG_ERR, "unexpected file descriptors %d %d %d", fd0, fd1, fd2);
        exit(1);
    }
}

void err_quit(const char *fmt, ...) {
    va_list v;
    va_start(v, fmt);
    vprintf(fmt, v);
    va_end(v);
    printf("\n");
    exit(1);
}

int main(void) {
    daemonize("daemonize");

    char *login_name = getlogin();
    if (login_name != NULL) {
        syslog(LOG_INFO, "login: %s", login_name);
    } else {
        syslog(LOG_INFO, "login: NULL");
    }
    pause();
}
