#include <apue.h>
#include <string.h>
#include "print.h"
#include <ctype.h>
#include <sys/select.h>

#define MAXCFGLINE 512
#define MAXKWLEN 16
#define MAXFMTLEN 16

/*
 * Get the address list for the given host and service and
 * return through ailistpp. Returns 0 on success or an error
 * code on failure. Note that we do not set errno if we
 * encounter an error.
 * 
 * LOCKING: none
 */
int getaddrlist(const char *host, const char *service, struct addrinfo **ailistpp) {
    struct addrinfo hint;
    hint.ai_flags = AI_CANONNAME;
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = 0;
    hint.ai_addrlen = 0;
    hint.ai_canonname = NULL;
    hint.ai_addr = NULL;
    hint.ai_next = NULL;
    return getaddrinfo(host, service, &hint, ailistpp);
}

/*
 * Given a keyword, scan the configuration file for a match
 * and return the string value corresponding to the keyword.
 * 
 * LOCKING: none
 */
static char *scan_configfile(char *keyword) {
    FILE *fp = fopen(CONFIG_FILE, "r");
    if (fp == NULL) {
        log_sys("can't open %s", CONFIG_FILE);
    }
    char keybuf[MAXKWLEN], pattern[MAXFMTLEN];
    char line[MAXCFGLINE];
    static char valbuf[MAXCFGLINE];

    sprintf(pattern, "%%%ds %%%ds", MAXKWLEN - 1, MAXCFGLINE - 1);
    int match = 0;
    while (fgets(line, MAXCFGLINE, fp) != NULL) {
        int n = sscanf(line, pattern, keybuf, valbuf);
        if (n == 2 && strcmp(keyword, keybuf) == 0) {
            match = 1;
            break;
        }
    }
    fclose(fp);
    if (match != 0) {
        return valbuf;
    } else {
        return NULL;
    }
}

/*
 * Return the host name running the print server or NULL on error.
 * 
 * LOCKING: none
 */
char *get_printserver(void) {
    return scan_configfile("printserver");
}

/*
 * Return the address of the network printer or NULL on error.
 * 
 * LOCKING: none
 */
struct addrinfo *get_printaddr(void) {
    char *p = scan_configfile("printer");
    if (p != NULL) {
        struct addrinfo *ailist;
        int err = getaddrlist(p, "ipp", &ailist);
        if (err != 0) {
            log_msg("no address information for %s", p);
        }
        return ailist;
    }
    log_msg("no printer address specified");
    return NULL;
}

/*
 * "Timed" read - timout specifies the # of seconds to wait before
 * giving up (5th argument to select controls how long to wait for
 * data to be readable). Return # of bytes read or -1 on error.
 * 
 * LOCKING: none
 */
ssize_t tread(int fd, void *buf, size_t nbyte, unsigned int timout) {
    struct timeval tv;
    tv.tv_sec = timout;
    tv.tv_usec = 0;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    int nfds = select(fd + 1, &readfds, NULL, NULL, &tv);

    if (nfds <= 0) {
        if (nfds == 0) {
            errno = ETIME;
        }
        return -1;
    }
    return read(fd, buf, nbyte);
}

/*
 * "Timed" read - timout specifies the number of seconds to wait
 * per read call before giving up, but read exactly nbytes bytes.
 * Return number of bytes read or -1 on error.
 * 
 * LOCKING: none
 */
ssize_t treadn(int fd, void *buf, size_t nbyte, unsigned int timout) {
    size_t nleft = nbyte;
    while (nleft > 0) {
        ssize_t nread = tread(fd, buf, nleft, timout);
        if (nread < 0) {
            if (nleft == nbyte) {
                return -1;
            } else {
                break;
            }
        } else if (nread == 0) {
            break;
        }
        nleft -= nread;
        buf += nread;
    }
    return nbyte - nleft;
}

int connect_retry(int domain, int type, int protocol, const struct sockaddr *addr, socklen_t alen) {
    int numsec;
    for (numsec = 1; numsec < MAXSLEEP; numsec <<= 1) {
        int fd = socket(domain, type, protocol);
        if (fd < 0) {
            return -1;
        }
        if (connect(fd, addr, alen) == 0) {
            return fd;
        }
        close(fd);

        if (numsec <= MAXSLEEP / 2) {
            sleep(numsec);
        }
    }
    return -1;
}

int initserver(int type, const struct sockaddr *addr, socklen_t len) {
    int sockfd = socket(addr->sa_family, type, 0);
    if (sockfd < 0) {
        return -1;
    }
    int err;
    if (bind(sockfd, addr, len) < 0) {
        goto errout;
    }
    if (type == SOCK_STREAM || type == SOCK_SEQPACKET) {
        if (listen(sockfd, QLEN) < 0) {
            goto errout;
        }
    }
    return sockfd;
errout:
    err = errno;
    close(sockfd);
    errno = err;
    return -1;
}