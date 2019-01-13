#if !defined(_PRINT_H)
#define _PRINT_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define CONFIG_FILE "/tmp/printer.conf"
#define SPOOLDIR "/tmp/printer"
#define JOBFILE "jobno"
#define DATADIR "data"
#define REQDIR "reqs"
#define LPNAME "lp"

#define FILENMSZ 64
#define FILEPERM (S_IRUSR | S_IWUSR)

#define USERNM_MAX 64
#define JOBNM_MAX 256
#define MSGLEN_MAX 512

#if !defined(HOST_NAME_MAX)
#define HOST_NAME_MAX 256
#endif

#define IPP_PORT 631
#define QLEN 10

#define IBUFSZ 512 // IPP header buffer size
#define HBUFSZ 512 // http header buffer size
#define IOBUFSZ 8192 // data buffer size

#if !defined(ETIME)
#define ETIME ETIMEDOUT
#endif

#define MAXSLEEP 0x80

int getaddrlist(const char *host, const char *service, struct addrinfo **ailistpp);
char *get_printserver(void);
struct addrinfo *get_printaddr(void);
ssize_t tread(int fd, void *buf, size_t nbyte, unsigned int timout);
ssize_t treadn(int fd, void *buf, size_t nbyte, unsigned int timout);
int connect_retry(int domain, int type, int protocol, const struct sockaddr *addr, socklen_t alen);
int initserver(int type, const struct sockaddr *addr, socklen_t len);

// Structure describing a print request
struct printreq {
    uint32_t size;
    uint32_t flags;
    char usernm[USERNM_MAX];
    char jobnm[JOBNM_MAX];
};

// Request flags
#define PR_TEXT 0x01

struct printresp {
    uint32_t retcode;
    uint32_t jobid;
    char msg[MSGLEN_MAX];
};

#endif // _PRINT_H
