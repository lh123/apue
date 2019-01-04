#include <apue.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <poll.h>
#include <sys/socket.h>

#define OPTSTR "d:einv"

static void set_noecho(int);
void do_driver(char *);
void loop(int, int);

int main(int argc, char *argv[]) {
    int interactive = isatty(STDIN_FILENO);
    int ignoreeof = 0;
    int noecho = 0;
    int verbose = 0;
    char *driver = NULL;

    opterr = 0;

    int c;
    while ((c = getopt(argc, argv, OPTSTR)) != -1) {
        switch (c) {
        case 'd':
            driver = optarg;
            break;
        case 'e':
            noecho = 1;
            break;
        case 'i':
            ignoreeof = 1;
            break;
        case 'n':
            interactive = 0;
            break;
        case 'v':
            verbose = 1;
            break;
        default:
            err_quit("unrecognized option: -%c", optopt);
        }
    }

    if (optind >= argc) {
        err_quit("usage: pty [-d driver -einv] program [ arg ...]");
    }

    pid_t pid;
    int fdm;
    char slave_name[20];
    if (interactive) {
        struct termios orig_termios;
        if (tcgetattr(STDIN_FILENO, &orig_termios) < 0) {
            err_sys("tcgetattr error on stdin");
        }
        struct winsize size;
        if (ioctl(STDIN_FILENO, TIOCGWINSZ, &size) < 0) {
            err_sys("TIOCGWINSZ error");
        }
        pid = pty_fork(&fdm, slave_name, sizeof(slave_name), &orig_termios, &size);
    } else {
        pid = pty_fork(&fdm, slave_name, sizeof(slave_name), NULL, NULL);
    }

    if (pid < 0) {
        err_sys("fork error");
    } else if (pid == 0) {
        if (noecho) {
            set_noecho(STDIN_FILENO);
        }
        if (execvp(argv[optind], &argv[optind]) < 0) {
            err_sys("can't execute: %s", argv[optind]);
        }
    }

    if (verbose) {
        fprintf(stderr, "slave name = %s\n", slave_name);
        if (driver != NULL) {
            fprintf(stderr, "driver = %s\n", driver);
        }
    }

    if (interactive && driver != NULL) {
        if (tty_raw(STDIN_FILENO) < 0) {
            err_sys("tty_raw error");
        }
        if (atexit(tty_atexit) < 0) {
            err_sys("atexit error");
        }
    }

    if (driver) {
        do_driver(driver);
    }
    loop(fdm, ignoreeof);
}

static void set_noecho(int fd) {
    struct termios stermios;

    if (tcgetattr(fd, &stermios) < 0) {
        err_sys("tcgetattr error");
    }
    stermios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    stermios.c_oflag &= ~(ONLCR);

    if (tcsetattr(fd, TCSANOW, &stermios) < 0) {
        err_sys("tcsetattr error");
    }
}

#define BUFFSIZE 512

void loop(int ptym, int ignoreeof) {
    struct pollfd pollfds[2];
    char buf[BUFFSIZE];
    for (;;) {
        pollfds[0].fd = STDIN_FILENO;
        pollfds[0].events = POLL_IN;

        pollfds[1].fd = ptym;
        pollfds[1].events = POLL_IN;

        int npoll = poll(pollfds, 2, -1);
        if (npoll < 0) {
            err_sys("poll error");
        }

        if (pollfds[0].revents & POLL_IN) {
            int nread = read(pollfds[0].fd, buf, BUFFSIZE);
            if (nread < 0) {
                err_sys("read error on fd %d", pollfds[0].fd);
            } else if (nread == 0) {
                pollfds[0].fd = -1;
                break;
            } else {
                if (writen(ptym, buf, nread) < 0) {
                    err_sys("write error to master pty");
                }
            }
        }

        if (pollfds[1].revents & POLL_IN) {
            int nread = read(pollfds[1].fd, buf, BUFFSIZE);
            if (nread < 0) {
                err_sys("read error on fd %d", pollfds[0].fd);
            } else if (nread == 0) {
                pollfds[1].fd = -1;
                close(ptym);
                break;
            } else {
                if (writen(STDOUT_FILENO, buf, nread) < 0) {
                    err_sys("write error to stdout");
                }
            }
        }

        if (pollfds[0].revents & POLLHUP || pollfds[1].revents & POLLHUP) {
            close(ptym);
            pollfds[0].fd = -1;
            pollfds[1].fd = -1;
            break;
        }
    }
}

void do_driver(char *driver) {
    int pipe[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipe) < 0) {
        err_sys("can't create stream pipe");
    }
    pid_t child = fork();
    if (child < 0) {
        err_sys("fork error");
    } else if (child == 0) {
        close(pipe[1]);

        if (dup2(pipe[0], STDIN_FILENO) != STDIN_FILENO) {
            err_sys("dup2 error to stdin");
        }
        if (dup2(pipe[0], STDOUT_FILENO) != STDOUT_FILENO) {
            err_sys("dup2 error to stdout");
        }
        if (pipe[0] != STDIN_FILENO && pipe[0] != STDOUT_FILENO) {
            close(pipe[0]);
        }
        execlp(driver, driver, NULL);
        err_sys("execlp error for: %s", driver);
    }

    close(pipe[0]);
    if (dup2(pipe[1], STDIN_FILENO) != STDIN_FILENO) {
        err_sys("dup2 error to stdin");
    }
    if (dup2(pipe[1], STDOUT_FILENO) != STDOUT_FILENO) {
        err_sys("dup2 error to stdout");
    }
    if (pipe[1] != STDIN_FILENO && pipe[1] != STDOUT_FILENO) {
        close(pipe[1]);
    }
}
