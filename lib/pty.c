#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif

#include "apue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

int ptym_open(char *pts_name, int pts_namesz) {
    int fdm = posix_openpt(O_RDWR);
    if (fdm < 0) {
        return -1;
    }
    
    int err;
    if (grantpt(fdm) < 0) {
        goto errout;
    }
    if (unlockpt(fdm) < 0) {
        goto errout;
    }

    char *ptr = ptsname(fdm);
    if (ptr == NULL) {
        goto errout;
    }
    strncpy(pts_name, ptr, pts_namesz - 1);
    pts_name[pts_namesz - 1] = 0;
    return fdm;

errout:
    err = errno;
    close(fdm);
    errno = err;
    return -1;
}

int ptys_open(char *pts_name) {
    int fds = open(pts_name, O_RDWR);
    if (fds < 0) {
        return -1;
    }
    return fds;
}

pid_t pty_fork(int *ptrfdm, char *slave_name, int slave_namesz,
                const struct termios *slave_termios, 
                const struct winsize *slave_winsize) {
    char pts_name[20];
    int fdm = ptym_open(pts_name, sizeof(pts_name));
    if (fdm < 0) {
        err_sys("can't open master pty: %s, error %d", pts_name, fdm);
    }
    if (slave_name != NULL) {
        strncpy(slave_name, pts_name, slave_namesz - 1);
        slave_name[slave_namesz - 1] = 0;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    } else if (pid == 0) {
        if (setsid() < 0) {
            err_sys("setsid error");
        }
        int fds = ptys_open(pts_name);
        if (fds < 0) {
            err_sys("can't open slave pty");
        }
        close(fdm);
        if (slave_termios != NULL) {
            if (tcsetattr(fds, TCSANOW, slave_termios) < 0) {
                err_sys("tcsetattr error on slave pty");
            }
        }
        if (slave_winsize != NULL) {
            if (ioctl(fds, TIOCSWINSZ, slave_winsize) < 0) {
                err_sys("TIOCSWINSZ error on slave pty");
            }
        }

        if (dup2(fds, STDIN_FILENO) != STDIN_FILENO) {
            err_sys("dup2 error to stdin");
        }
        if (dup2(fds, STDOUT_FILENO) != STDOUT_FILENO) {
            err_sys("dup2 error to stdout");
        }
        if (dup2(fds, STDERR_FILENO) != STDERR_FILENO) {
            err_sys("dup2 error to stderr");
        }
        if (fds != STDIN_FILENO && fds != STDOUT_FILENO && fds != STDERR_FILENO) {
            close(fds);
        }
        return 0;
    } else {
        *ptrfdm = fdm;
        return pid;
    }
}
