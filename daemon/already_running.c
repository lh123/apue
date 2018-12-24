#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>

#define LOCKFILE "/var/run/daemon.pid"
#define LOCKMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

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

int main(void) {
    already_running();
    sleep(5);
}
