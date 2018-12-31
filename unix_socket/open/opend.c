#include "opend.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/syslog.h>

#define NALLOC 10

int debug;
extern int log_to_stderr;

Client *client = NULL;
int client_size;

char errmsg[MAXLINE];
int oflag;
char *pathname;

void daemonize(const char *cmd);

int main(int argc, char *argv[]) {
    log_open("open.serv", LOG_PID, LOG_USER);
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "d")) != -1) {
        switch (c) {
        case 'd':
            debug = log_to_stderr = 1;
            break;
        default: // '?'
            err_quit("unrecognized option: -%c", optopt);
        }
    }

    if (debug == 0) {
        daemonize("opend");
    }
    loop();
}

int buf_args(char *buf, int (*optfunc)(int, char **));

void handle_request(char *buf, int nread, int fd, uid_t uid) {
    if(buf[nread - 1] != 0) {
        snprintf(errmsg, MAXLINE, "request not null terminated: %*.*s\n", nread, nread, buf);
        send_err(fd, -1, errmsg);
        return;
    }
    log_msg("request: %s, from uid %d", buf, uid);
    if (buf_args(buf, cli_args) < 0) {
        send_err(fd, -1, errmsg);
        log_msg(errmsg);
        return;
    }
    int newfd = open(pathname, oflag);
    if (newfd < 0) {
        snprintf(errmsg, MAXLINE, "can't open %s: %s\n", pathname, strerror(errno));
        send_err(fd, -1, errmsg);
        log_msg(errmsg);
        return;
    }
    if (send_fd(fd, newfd) < 0) {
        log_sys("send_fd error");
    }
    log_msg("sent fd %d over fd %d for %s", newfd, fd, pathname);
    close(newfd);
}

#define MAXARGC 50
#define WHITE " \t\n"

/*
 * buf[] contains white-space-separated arguments. We convert it to an
 * argv-style array of pointers, and call the user's function (optfunc)
 * to process the array. We return -1 if there's a problem parsing buf,
 * else we return whatever optfunc() returns. Note that user's buf[]
 * array is modified (nulls placed after each token).
 */
int buf_args(char *buf, int (*optfunc)(int, char **)) {
    if (strtok(buf, WHITE) == NULL) {
        return -1;
    }
    int argc = 0;
    char *argv[MAXARGC];
    argv[0] = buf;
    char *ptr;
    while ((ptr = strtok(NULL, WHITE)) != NULL) {
        if (++argc >= MAXARGC - 1) {
            return -1;
        }
        argv[argc] = ptr;
    }
    argv[++argc] = NULL;
    return optfunc(argc, argv);
}

int cli_args(int argc, char **argv) {
    if (argc != 3 || strcmp(argv[0], CL_OPEN) != 0) {
        strncpy(errmsg, "usage: <pathname> <oflag>\n", sizeof(errmsg) - 1);
        errmsg[sizeof(errmsg) - 1] = 0;
        return -1;
    }
    pathname = argv[1];
    errno = 0;
    oflag = strtol(argv[2], NULL, 10);
    if (errno != 0) {
        strncpy(errmsg, "invalid oflag\n", sizeof(errmsg) - 1);
        errmsg[sizeof(errmsg) - 1] = 0;
        return -1;
    }
    return 0;
}

static void client_alloc(void) {
    if (client == NULL) {
        client = malloc(NALLOC * sizeof(Client));
    } else {
        client = realloc(client, (client_size + NALLOC) * sizeof(Client));
    }
    if (client == NULL) {
        err_sys("can't alloc for client array");
    }

    int i;
    for (i = client_size; i < client_size + NALLOC; i++) {
        client[i].fd = -1;
    }
    client_size += NALLOC;
}

int client_add(int fd, uid_t uid) {
    int i;

    if (client == NULL) {
        client_alloc();
    }
    for (;;) {
        for (i = 0; i < client_size; i++) {
            if (client[i].fd == -1) {
                client[i].fd = fd;
                client[i].uid = uid;
                return i;
            }
        }

        client_alloc();
    }
}

void client_del(int fd) {
    int i;
    for (i =  0; i < client_size; i++) {
        if (client[i].fd == fd) {
            client[i].fd = -1;
            return;
        }
    }
    log_quit("can't find client entry for fd %d", fd);
}

void daemonize(const char *cmd) {
    umask(0);
    
    pid_t pid = fork();
    if (pid < 0) {
        err_sys("fork error");
    } else if (pid != 0) {
        exit(0);
    }

    if (setsid() < 0) {
        log_sys("setsid error");
    }
    if (chdir("/") < 0) {
        log_sys("chdir error");
    }
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    int fd0 = open("/dev/null", O_RDWR);
    int fd1 = dup(0);
    int fd2 = dup(0);

    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        log_quit("unexpected fd %d %d %d", fd0, fd1, fd2);
    }
}

void loop(void) {
    fd_set rset, allset;
    FD_ZERO(&allset);
    
    int listenfd = serv_listen(CS_OPEN);
    if (listenfd < 0) {
        log_sys("serv_listen error");
    }
    FD_SET(listenfd, &allset);
    int maxfd = listenfd;
    int maxi = -1;

    uid_t uid;
    int clifd;
    int i;
    char buf[MAXLINE];
    log_msg("server start");
    for (;;) {
        rset = allset;
        int n = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (n < 0) {
            log_sys("select error");
        }
        
        if (FD_ISSET(listenfd, &rset)) {
            if ((clifd = serv_accept(listenfd, &uid)) < 0) {
                log_sys("serv_accept error: %d", clifd);
            }
            i = client_add(clifd, uid);
            FD_SET(clifd, &allset);
            if (clifd > maxfd) {
                maxfd = clifd;
            }
            if (i > maxi) {
                maxi = i;
            }
            log_msg("new connection: uid %d, fd %d", uid, clifd);
            continue;
        }
        
        for (i = 0; i <= maxi; i++) {
            if ((clifd = client[i].fd) < 0) {
                continue;
            }
            if (FD_ISSET(clifd, &rset)) {
                int nread = read(clifd, buf, MAXLINE);
                if (nread < 0) {
                    log_sys("read error on fd %d", clifd);
                } else if (nread == 0) {
                    log_msg("closed: uid %d, fd %d", client[i].uid, clifd);
                    client_del(clifd);
                    FD_CLR(clifd, &allset);
                    close(clifd);
                } else {
                    handle_request(buf, nread, clifd, client[i].fd);
                }
            }
        }
    }
}
