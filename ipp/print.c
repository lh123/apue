/*
 * The client command for printing documents. Opens the file
 * and sends it to the printer spooling daemon. Usage:
 *      print [-t] filename
 */
#include <apue.h>
#include <print.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>

extern int log_to_stderr;

void submit_file(int fd, int sockfd, const char * fname, size_t nbytes, int text);

int main(int argc, char *argv[]) {
    int text = 0, err = 0, c;
    log_to_stderr = 1;
    while ((c = getopt(argc, argv, "t")) != -1) {
        switch (c) {
        case 't':
            text = 1;
            break;
        default:
            err = 1;
        }
    }

    if (err || optind != argc - 1) {
        err_quit("usage: print [-t] filename");
    }
    int fd = open(argv[optind], O_RDONLY);
    if (fd < 0) {
        err_sys("print: can't open %s", argv[optind]);
    }
    struct stat sbuf;
    if (fstat(fd, &sbuf) < 0) {
        err_sys("print: can't stat %s", argv[optind]);
    }

    if (!S_ISREG(sbuf.st_mode)) {
        err_quit("print: %s must be a regular file\n", argv[optind]);
    }

    // Get the hostname of the host acting as the print server.
    char *host = get_printserver();
    if (host == NULL) {
        err_quit("print: no print server defined");
    }
    struct addrinfo *ailist, *aip;
    if ((err = getaddrlist(host, "print", &ailist)) != 0) {
        err_quit("print: getaddrinfo error: %s", gai_strerror(err));
    }
    for (aip = ailist; aip != NULL; aip = aip->ai_next) {
        int sfd = connect_retry(AF_INET, SOCK_STREAM, 0, aip->ai_addr, aip->ai_addrlen);
        if (sfd < 0) {
            err = errno;
        } else {
            submit_file(fd, sfd, argv[optind], sbuf.st_size, text);
            exit(0);
        }
    }
}

void submit_file(int fd, int sockfd, const char * fname, size_t nbytes, int text) {
    struct printreq req;
    struct passwd *pwd = getpwuid(geteuid());
    if (pwd == NULL) {
        strcpy(req.usernm, "unknown");
    } else {
        strncpy(req.usernm, pwd->pw_name, USERNM_MAX - 1);
        req.usernm[USERNM_MAX - 1] = '\0';
    }
    req.size = htonl(nbytes);

    if (text) {
        req.flags = htonl(PR_TEXT);
    } else {
        req.flags = 0;
    }

    int len = strlen(fname);
    if (len >= JOBNM_MAX) {
        /*
         * Truncate the filename (+-5 accounts for the leading
         * four characters and the terminating null).
         */
        strcpy(req.jobnm, "... ");
        strncat(req.jobnm, &fname[len - JOBNM_MAX + 5], JOBNM_MAX - 5);
    } else {
        strcpy(req.jobnm, fname);
    }

    int nw = writen(sockfd, &req, sizeof(struct printreq));
    if (nw != sizeof(struct printreq)) {
        if (nw < 0) {
            err_sys("can't write to print server");
        } else {
            err_quit("short write (%d/%d) to print server", nw, (int)sizeof(struct printreq));
        }
    }

    // Now send the file
    int nr;
    char buf[IOBUFSZ];
    while ((nr = read(fd, buf, IOBUFSZ)) != 0) {
        nw = writen(sockfd, buf, nr);
        if (nw != nr) {
            if (nw < 0) {
                err_sys("can't write to print server");
            } else {
                err_quit("short write (%d/%d) to print server", nw, nr);
            }
        }
    }
    struct printresp res;
    if ((nr = readn(sockfd, &res, sizeof(struct printresp))) != sizeof(struct printresp)) {
        err_sys("can't read response from server");
    }
    if (res.retcode != 0) {
        printf("rejected: %s\n", res.msg);
        exit(1);
    } else {
        printf("job ID %ld\n", (long)ntohl(res.jobid));
    }
}
