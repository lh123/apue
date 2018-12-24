#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>

#define LOCKFILE "/var/run/daemon.pid"
#define LOCKMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

sigset_t mask;

void __attribute__((noreturn, format(printf, 1, 2))) err_quit(const char *fmt, ...);
void __attribute__((noreturn, format(printf, 2, 3))) err_exit(int err, const char *fmt, ...);

void reread(void) {
    //read configure
}

void daemonize(const char *cmd) {
    umask(0);

    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        err_quit("%s: can't get file limit", cmd);
    }

    pid_t pid = fork();
    if (pid < 0) {
        err_quit("%s: can't fork", cmd);
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

    pid = fork();
    if (pid < 0) {
        err_quit("%s: can't fork", cmd);
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

int lockfile(int fd) {
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    return fcntl(fd, F_SETLK, &lock);
}

int already_running(void) {
    int fd = open(LOCKFILE, O_RDWR | O_CREAT, LOCKMODE);
    if (fd < 0) {
        syslog(LOG_ERR, "can't open %s: %m", LOCKFILE);
        exit(1);
    }
    
    if (lockfile(fd) < 0) {
        if (errno == EACCES || errno == EAGAIN) {
            syslog(LOG_ERR, "file already lock: %m");
            close(fd);
            return 1;
        }
        syslog(LOG_ERR, "can't lock %s: %m", LOCKFILE);
        exit(1);
    }
    
    ftruncate(fd, 0);
    char buf[16];
    sprintf(buf, "%ld", (long)getpid());
    write(fd, buf, strlen(buf));
    return 0;
}

void sigterm(int signo) {
    syslog(LOG_INFO, "got SIGTERM; exiting");
    exit(0);
}

void sighup(int signo) {
    syslog(LOG_INFO, "Re-reading configuration file");
    reread();
}

int main(int argc, char *argv[]) {
    char *cmd = strrchr(argv[0], '/');
    if (cmd == NULL) {
        cmd = argv[0];
    } else {
        cmd++;
    }
    daemonize(cmd);

    if (already_running()) {
        syslog(LOG_ERR, "daemon already running");
        exit(1);
    }

    struct sigaction sa;
    sa.sa_handler = sigterm;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGHUP);
    sa.sa_flags = 0;
    if (sigaction(SIGTERM, &sa, NULL) < 0) {
        err_quit("%s: can't catch SIGTERM: %s", cmd, strerror(errno));
    }

    sa.sa_handler = sighup;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGTERM);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        err_quit("%s: can't catch SIGHUP: %s", cmd, strerror(errno));
    }
    while (1) {
        pause();
    }
}

void err_quit(const char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    vprintf(fmt, list);
    va_end(list);
    printf("\n");
    exit(1);
}

void err_exit(int err, const char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    vprintf(fmt, list);
    va_end(list);
    printf(": %s\n", strerror(err));
    exit(1);
}
