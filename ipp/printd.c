/*
 * Print server daemon
 */
#include <apue.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <pthread.h>
#include <pwd.h>
#include <fcntl.h>
#include <dirent.h>

#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/syslog.h>

#include "print.h"
#include "ipp.h"

// These are for the HTTP response from the printer.
#define HTTP_INFO(x) ((x) >= 100 && (x) <= 199)
#define HTTP_SUCCESS(x) ((x) >= 200 && (x) <= 299)

// Describes a print job.
struct job {
    struct job *next;
    struct job *prev;
    long jobid;
    struct printreq req;
};

// Describes a thread processing a client request.
struct worker_thread {
    struct worker_thread *next;
    struct worker_thread *prev;
    pthread_t tid;
    int sockfd;
};

extern int log_to_stderr;

// Printer-related stuff.
struct addrinfo *printer;
char *printer_name;
pthread_mutex_t configlock = PTHREAD_MUTEX_INITIALIZER;
int reread;

// Thread-related stuff.
struct worker_thread *workers;
pthread_mutex_t workerlock = PTHREAD_MUTEX_INITIALIZER;
sigset_t mask;

// Job-related stuff.
struct job *jobhead, *jobtail;
int jobfd;
int32_t nextjob;
pthread_mutex_t joblock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t jobwait = PTHREAD_COND_INITIALIZER;

void daemonize(const char *cmd);
void init_request(void);
void init_printer(void);
void update_jobno(void);
int32_t get_newjobno(void);
void add_job(struct printreq *reqp, int32_t jobid);
void replace_job(struct job *jp);
void remove_job(struct job *jp);
void build_qonstart(void);
void *client_thread(void *arg);
void *printer_thread(void *arg);
void *signal_thread(void *arg);
ssize_t readmore(int sockfd, char **bpp, int off, int *bszp);
int printer_status(int, struct job *);
void add_worker(pthread_t tid, int sockfd);
void kill_workers(void);
void client_cleanup(void *);

int main(int argc, char *argv[]) {
    if (argc != 1) {
        err_quit("usage: printd");
    }
    daemonize("printd");

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) < 0) {
        log_sys("sigaction failed");
    }
    sigemptyset(&mask);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGTERM);
    int err = pthread_sigmask(SIG_BLOCK, &mask, NULL);
    if (err != 0) {
        log_sys("pthread_sigmask failed");
    }
    int n = sysconf(_SC_HOST_NAME_MAX);
    if (n < 0) {
        n = HOST_NAME_MAX;
    }
    char *host = malloc(n);
    if (host == NULL) {
        log_sys("malloc error");
    }
    if (gethostname(host, n) < 0) {
        log_sys("gethostname error");
    }
    struct addrinfo *ailist, *aip;
    if ((err = getaddrlist(host, "print", &ailist)) != 0) {
        log_quit("getaddrinfo error: %s", gai_strerror(err));
        exit(1);
    }

    fd_set rendezvous;
    FD_ZERO(&rendezvous);
    int maxfd = -1;
    for (aip = ailist; aip != NULL; aip = aip->ai_next) {
        int sockfd = initserver(SOCK_STREAM, aip->ai_addr, aip->ai_addrlen);
        if (sockfd >= 0) {
            FD_SET(sockfd, &rendezvous);
            if (sockfd > maxfd) {
                maxfd = sockfd;
            }
        }
    }
    if (maxfd == -1) {
        log_quit("service not enable");
    }
    struct passwd *pwdp = getpwnam(LPNAME);
    if (pwdp == NULL) {
        log_sys("can't find user %s", LPNAME);
    }
    if (pwdp->pw_uid == 0) {
        log_quit("user %s is privileged", LPNAME);
    }
    if (setgid(pwdp->pw_gid) < 0 || setuid(pwdp->pw_uid) < 0) { // 降权
        log_sys("can't change IDs to user %s", LPNAME);
    }
    init_request();
    init_printer();

    pthread_t tid;
    err = pthread_create(&tid, NULL, printer_thread, NULL);
    if (err == 0) {
        err = pthread_create(&tid, NULL, signal_thread, NULL);
    }
    if (err != 0) {
        log_exit(err, "can't create thread");
    }
    build_qonstart();
    log_msg("daemon initialized");

    for (;;) {
        fd_set rset = rendezvous;
        if (select(maxfd + 1, &rset, NULL, NULL, NULL) < 0) {
            log_sys("select failed");
        }
        int i;
        for (i = 0; i <= maxfd + 1; i++) {
            if (FD_ISSET(i, &rset)) {
                int sockfd = accept(i, NULL, NULL);
                if (sockfd < 0) {
                    log_ret("accept failed");
                }
                pthread_create(&tid, NULL, client_thread, (void *)((size_t)sockfd));
            }
        }
    }
    return 1;
}

/*
 * Initialize the job ID file. Use a record lock to prevent
 * more than one printer daemon from running at a time.
 * 
 * LOCKING: none, execpt for record-lock on job ID file.
 */
void init_request(void) {
    char name[FILENMSZ];
    sprintf(name, "%s/%s", SPOOLDIR, JOBFILE);
    jobfd = open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (write_lock(jobfd, 0, SEEK_SET, 0) < 0) {
        log_quit("daemon already running");
    }
    // Reuse the name buffer for the job counter.
    int n = read(jobfd, name, FILENMSZ);
    if (n < 0) {
        log_sys("can't read job file");
    }
    if (n == 0) {
        nextjob = 1;
    } else {
        nextjob = atol(name);
    }
}

/*
 * Initialize printer information from configuration file.
 * 
 * LOCKING: none.
 */
void init_printer(void) {
    printer = get_printaddr();
    if (printer == NULL) {
        exit(1);
    }
    printer_name = printer->ai_canonname;
    if (printer_name == NULL) {
        printer_name = "printer";
    }
    log_msg("printer is %s", printer_name);
}

/*
 * Update the job ID file with the next job number.
 * Doesn't handle wrap-around of job number.
 * 
 * LOCKING: none.
 */
void update_jobno(void) {
    if (lseek(jobfd, 0, SEEK_SET) < 0) {
        log_sys("can't seek in job file");
    }
    char buf[32];
    sprintf(buf, "%d", nextjob);
    if (write(jobfd, buf, strlen(buf)) < 0) {
        log_sys("can't update job file");
    }
}

/*
 * Get the next job number.
 * 
 * LOCKING: acquires and releases joblock.
 */
int32_t get_newjobno(void) {
    pthread_mutex_lock(&joblock);
    int32_t jobid = nextjob++;
    if (nextjob <= 0) {
        nextjob = 1;
    }
    pthread_mutex_unlock(&joblock);
    return jobid;
}

/*
 * Add a new job to the list of pending jobs. Then signal
 * the printer thread that a job is pending.
 * 
 * LOCKING: acquires and releases joblock.
 */
void add_job(struct printreq *reqp, int32_t jobid) {
    struct job *jp = malloc(sizeof(struct job));
    if (jp == NULL) {
        log_sys("malloc failed");
    }
    memcpy(&jp->req, reqp, sizeof(struct printreq));
    jp->jobid = jobid;
    jp->next = NULL;
    pthread_mutex_lock(&joblock);
    jp->prev = jobtail;
    if (jobtail == NULL) {
        jobhead = jp;
    } else {
        jobtail->next = jp;
    }
    jobtail = jp;
    pthread_cond_signal(&jobwait);
    pthread_mutex_unlock(&joblock);
}

/*
 * Replace a job back on the head of the list.
 * 
 * LOCKING: acquires and releases joblock.
 */
void replace_job(struct job *jp) {
    pthread_mutex_lock(&joblock);
    jp->prev = NULL;
    jp->next = jobhead;
    if (jobhead == NULL) {
        jobtail = jp;
    } else {
        jobhead->prev = jp;
    }
    jobhead = jp;
    pthread_mutex_unlock(&joblock);
}

/*
 * Remove a job from the list of pending jobs.
 * 
 * LOCKING: caller must hold joblock.
 */
void remove_job(struct job *target) {
    if (target->next != NULL) {
        target->next->prev = target->prev;
    } else {
        jobtail = target->prev;
    }
    if (target->prev != NULL) {
        target->prev->next = target->next;
    } else {
        jobtail = target->next;
    }
}

/*
 * Check the spool directory for pending jobs on start-up.
 * 
 * LOCKING: none.
 */
void build_qonstart(void) {
    char dname[FILENMSZ], fname[FILENMSZ];
    sprintf(dname, "%s/%s", SPOOLDIR, REQDIR);
    DIR *dirp = opendir(dname);
    if (dirp == NULL) {
        return;
    }
    struct dirent *entp;
    int err;
    struct printreq req;
    while ((entp = readdir(dirp)) != NULL) {
        if (strcmp(entp->d_name, ".") == 0 || strcmp(entp->d_name, "..") == 0) {
            continue;
        }
        snprintf(fname, FILENMSZ, "%s/%s/%s", SPOOLDIR, REQDIR, entp->d_name);
        int fd = open(fname, O_RDONLY);
        if (fd < 0) {
            continue;
        }
        int nr = read(fd, &req, sizeof(struct printreq));
        if (nr != sizeof(struct printreq)) {
            if (nr < 0) {
                err = errno;
            } else {
                err = EIO;
            }
            close(fd);
            log_msg("build_qonstart: can't read %s: %s", fname, strerror(err));
            unlink(fname);
            sprintf(fname, "%s/%s/%s", SPOOLDIR, DATADIR, entp->d_name);
            unlink(fname);
            continue;
        }
        int32_t jobid = atol(entp->d_name);
        log_msg("adding job %d to queue", jobid);
        add_job(&req, jobid);
    }
    closedir(dirp);
}

/*
 * Accept a print job from a client.
 * 
 * LOCKING: none.
 */
void *client_thread(void *arg) {
    pthread_t tid = pthread_self();
    pthread_cleanup_push(client_cleanup, (void *)((size_t)tid));
    int sockfd = (size_t)arg;
    add_worker(tid, sockfd);

    // Read the request header.
    struct printreq req;
    struct printresp res;
    int n = treadn(sockfd, &req, sizeof(struct printreq), 10);
    if (n != sizeof(struct printreq)) {
        res.jobid = 0;
        int retcode;
        if (n < 0) {
            retcode = errno;
        } else {
            retcode = EIO;
        }
        res.retcode = htonl(retcode);
        strncpy(res.msg, strerror(retcode), MSGLEN_MAX);
        writen(sockfd, &res, sizeof(struct printresp));
        pthread_exit((void *)1);
    }
    req.size = ntohl(req.size);
    req.flags = ntohl(req.flags);

    // Create the data file.
    int32_t jobid = get_newjobno();
    char name[FILENMSZ];
    sprintf(name, "%s/%s/%d", SPOOLDIR, DATADIR, jobid);
    int fd = creat(name, FILEPERM);
    if (fd < 0) {
        res.jobid = 0;
        res.retcode = htonl(errno);
        log_msg("client_thread: can't create %s: %s", name, strerror(errno));
        strncpy(res.msg, strerror(errno), MSGLEN_MAX);
        writen(sockfd, &res, sizeof(struct printresp));
        pthread_exit((void *)1);
    }
    
    /*
     * Read the file and store it in the spool directory.
     * Try to figure out if the file is a PostScript file
     * or a plain text file.
     */
    int first = 1;
    int nr, nw;
    char buf[IOBUFSZ];
    while ((nr = tread(sockfd, buf, IOBUFSZ, 20)) > 0) {
        if (first) {
            first = 0;
            if (strncmp(buf, "%!PS", 4) != 0) {
                req.flags |= PR_TEXT;
            }
        }
        nw = write(fd, buf, nr);
        if (nw != nr) {
            res.jobid = 0;
            int retcode;
            if (nw < 0) {
                retcode = errno;
            } else {
                retcode = EIO;
            }
            res.retcode = htonl(retcode);
            log_msg("client_thread: can't write %s: %s", name, strerror(retcode));
            close(fd);
            strncpy(res.msg, strerror(retcode), MSGLEN_MAX);
            writen(sockfd, &res, sizeof(struct printresp));
            unlink(name);
            pthread_exit((void *)1);
        }
    }
    close(fd);

    /*
     * Create the control file. Then write the
     * print request information to the control
     * file.
     */
    sprintf(name, "%s/%s/%d", SPOOLDIR, REQDIR, jobid);
    fd = creat(name, FILEPERM);
    if (fd < 0) {
        res.jobid = 0;
        res.retcode = htonl(errno);
        log_msg("client_thread: can't create %s: %s", name, strerror(errno));
        strncpy(res.msg, strerror(errno), MSGLEN_MAX);
        writen(sockfd, &res, sizeof(struct printresp));
        sprintf(name, "%s/%s/%d", SPOOLDIR, DATADIR, jobid);
        unlink(name);
        pthread_exit((void *)1);
    }
    nw = write(fd, &req, sizeof(struct printreq));
    if (nw != sizeof(struct printreq)) {
        res.jobid = 0;
        int retcode;
        if (nw < 0) {
            retcode = errno;
        } else {
            retcode = EIO;
        }
        res.retcode = htonl(retcode);
        log_msg("client_thread: can't write %s: %s", name, strerror(retcode));
        close(fd);
        strncpy(res.msg, strerror(retcode), MSGLEN_MAX);
        writen(sockfd, &res, sizeof(struct printresp));
        unlink(name);
        sprintf(name, "%s/%s/%d", SPOOLDIR, DATADIR, jobid);
        unlink(name);
        pthread_exit((void *)1);
    }
    close(fd);

    // Send response to client.
    res.retcode = 0;
    res.jobid = htonl(jobid);
    sprintf(res.msg, "request ID %d", jobid);
    writen(sockfd, &res, sizeof(struct printresp));

    // Notify the printer thread, clean up, and exit.
    log_msg("adding job %d to queue", jobid);
    add_job(&req, jobid);
    pthread_cleanup_pop(1);
    return NULL;
}

/*
 * Add a worker to the list of worker threads.
 * 
 * LOCKING: acquires and releases workerlock.
 */
void add_worker(pthread_t tid, int sockfd) {
    struct worker_thread *wtp = malloc(sizeof(struct worker_thread));
    if (wtp == NULL) {
        log_ret("add_worker: can't malloc");
        pthread_exit((void *)1);
    }
    wtp->tid = tid;
    wtp->sockfd = sockfd;

    pthread_mutex_lock(&workerlock);
    wtp->prev = NULL;
    wtp->next = workers;
    if (workers != NULL) {
        workers->next = wtp;
    }
    workers = wtp;
    pthread_mutex_unlock(&workerlock);
}

/*
 * Cancel (kill) all outstanding workers.
 * 
 * LOCKING: acquires and releases workerlock.
 */
void kill_workers(void) {
    pthread_mutex_lock(&workerlock);
    struct worker_thread *wtp;
    for (wtp = workers; wtp != NULL; wtp = wtp->next) {
        pthread_cancel(wtp->tid);
    }
    pthread_mutex_unlock(&workerlock);
}

/*
 * Cancellation routine for the worker thread.
 * 
 * LOCKING: acquires and releases workerlock.
 */
void client_cleanup(void *arg) {
    pthread_t tid = (size_t)arg;
    struct worker_thread *wtp;
    pthread_mutex_lock(&workerlock);
    for (wtp = workers; wtp != NULL; wtp = wtp->next) {
        if (wtp->tid == tid) {
            if (wtp->next != NULL) {
                wtp->next->prev = wtp->prev;
            }
            if (wtp->prev != NULL) {
                wtp->prev->next = wtp->next;
            } else {
                workers = wtp->next;
            }
            break;
        }
    }
    pthread_mutex_unlock(&workerlock);
    if (wtp != NULL) {
        close(wtp->sockfd);
        free(wtp);
    }
}

/*
 * Deal with signals.
 * 
 * LOCKING: acquires and releases configlock.
 */
void *signal_thread(void *arg) {
    for (;;) {
        int signo;
        int err = sigwait(&mask, &signo);
        if (err != 0) {
            log_quit("sigwait failed: %s", strerror(err));
        }
        switch (signo) {
        case SIGHUP:
            pthread_mutex_lock(&configlock);
            reread = 1;
            pthread_mutex_unlock(&configlock);
            break;
        case SIGTERM:
            kill_workers();
            log_msg("terminate with signal %s", strsignal(signo));
            exit(0);
        default:
            kill_workers();
            log_quit("unexpected signal %d", signo);
        }
    }
}

/*
 * Add an option to the IPP header.
 * 
 * LOCKINGN: none.
 */
char *add_option(char *cp, int tag, const char *optname, const char *optval) {
    union {
        int16_t s;
        char c[2];
    } u;
    *cp++ = tag;
    int n = strlen(optname);
    u.s = htons(n);
    *cp++ = u.c[0];
    *cp++ = u.c[1];
    strcpy(cp, optname);
    cp += n;
    n = strlen(optval);
    u.s = htons(n);
    *cp++ = u.c[0];
    *cp++ = u.c[1];
    strcpy(cp, optval);
    return cp + n;
}

/*
 * Single thread to communicate with the printer.
 * 
 * LOCKING: acquires and releases joblock and configlock.
 */
void *printer_thread(void *arg) {
    char name[FILENMSZ];
    char hbuf[HBUFSZ];
    char ibuf[IBUFSZ];
    char buf[IOBUFSZ];
    struct timespec ts = {60, 0};
    for (;;) {
        pthread_mutex_lock(&joblock);
        while (jobhead == NULL) {
            log_msg("printer_thread: waiting...");
            pthread_cond_wait(&jobwait, &joblock);
        }
        struct job *jp = jobhead;
        remove_job(jp);
        log_msg("printer_thread: picked up job %ld", jp->jobid);
        pthread_mutex_unlock(&joblock);
        update_jobno();

        // Check for a change in the config file.
        pthread_mutex_lock(&configlock);
        if (reread) {
            freeaddrinfo(printer);
            printer = NULL;
            printer_name = NULL;
            reread = 0;
            pthread_mutex_unlock(&configlock);
            init_printer();
        } else {
            pthread_mutex_unlock(&configlock);
        }

        // Send job to printer.
        sprintf(name, "%s/%s/%ld", SPOOLDIR, DATADIR, jp->jobid);
        int fd = open(name, O_RDONLY);
        if (fd < 0) {
            log_msg("job %ld canceled - can't fstat %s: %s", jp->jobid, name, strerror(errno));
            free(jp);
            continue;
        }
        struct stat sbuf;
        if (fstat(fd, &sbuf) < 0) {
            log_msg("job %ld canceled - can't fstat %s: %s", jp->jobid, name, strerror(errno));
            free(jp);
            close(fd);
            continue;
        }
        int sockfd = connect_retry(AF_INET, SOCK_STREAM, 0, printer->ai_addr, printer->ai_addrlen);
        if (sockfd < 0) {
            log_msg("job %ld deferred - can't contact printer: %s", jp->jobid, strerror(errno));
            goto defer;
        }


        //Set up the IPP header.
        char *icp = ibuf;
        struct ipp_hdr *hp = (struct ipp_hdr *)icp;
        hp->major_version = 1;
        hp->minor_version = 1;
        hp->operation = htons(OP_PRINT_JOB);
        hp->request_id = htonl(jp->jobid);
        icp += offsetof(struct ipp_hdr, attr_group);
        *icp++ = TAG_OPERATION_ATTR;
        icp = add_option(icp, TAG_CHARSET, "attributes-charset", "utf-8");
        icp = add_option(icp, TAG_NAMEWOLANG, "requesting-user-name", jp->req.usernm);
        icp = add_option(icp, TAG_NAMEWOLANG, "job-name", jp->req.jobnm);
        const char *p;
        int extra;
        if (jp->req.flags & PR_TEXT) {
            p = "text/plain";
            extra = 1;
        } else {
            p = "application/postscript";
            extra = 0;
        }
        icp = add_option(icp, TAG_MIMETYPE, "document-format", p);
        *icp++ = TAG_END_OF_ATTR;
        int ilen = icp - ibuf;

        char *hcp = hbuf;
        sprintf(hcp, "POST /ipp HTTP/1.1\r\n");
        hcp += strlen(hcp);
        sprintf(hcp, "Content-Length: %ld\r\n", (long)sbuf.st_size + ilen + extra);
        hcp += strlen(hcp);
        sprintf(hcp, "Host: %s:%d\r\n", printer_name, IPP_PORT);
        hcp += strlen(hcp);
        *hcp++ = '\r';
        *hcp++ = '\n';
        int hlen = hcp - hbuf;

        struct iovec iov[2];
        iov[0].iov_base = hbuf;
        iov[0].iov_len = hlen;
        iov[1].iov_base = ibuf;
        iov[1].iov_len = ilen;
        if (writev(sockfd, iov, 2) != hlen + ilen) {
            log_ret("can't write to printer");
            goto defer;
        }

        if (jp->req.flags & PR_TEXT) {
            // Hack: allow PostScript to be printed as plain text.
            if (write(sockfd, "\b", 1) != 1) {
                log_ret("can't write to printer");
                goto defer;
            }
        }

        int nr, nw;
        while ((nr = read(fd, buf, IOBUFSZ)) > 0) {
            if ((nw = writen(sockfd, buf, nr)) != nr) {
                if (nw < 0) {
                    log_ret("can't write to printer");
                } else {
                    log_msg("short write (%d/%d) to printer", nw, nr);
                }
                goto defer;
            }
        }
        if (nr < 0) {
            log_ret("can't read %s", name);
            goto defer;
        }

        // Read the response from the printer.
        if (printer_status(sockfd, jp)) {
            unlink(name);
            sprintf(name, "%s/%s/%ld", SPOOLDIR, REQDIR, jp->jobid);
            unlink(name);
            free(jp);
            jp = NULL;
        }

defer:
        close(fd);
        if (sockfd >= 0) {
            close(sockfd);
        }
        if (jp != NULL) {
            replace_job(jp);
            nanosleep(&ts, NULL);
        }
    }
}

/*
 * Read data from printer, possibly increasing the buffer.
 * Returns offset of end of data in buffer or -1 on failure.
 * 
 * LOCKING: none.
 */
ssize_t readmore(int sockfd, char **bpp, int off, int *bszp) {
    char *bp = *bpp;
    int bsz = *bszp;
    if (off >= bsz) {
        bsz += IOBUFSZ;
        if ((bp = realloc(*bpp, bsz)) == NULL) {
            log_sys("readmore: can't allocate bigger read buffer");
        }
        *bszp = bsz;
        *bpp = bp;
    }
    ssize_t nr = tread(sockfd, &bp[off], bsz - off, 1);
    if (nr > 0) {
        return nr;
    } else {
        return -1;
    }
}

/*
 * Read and parse the response from the printer. Return 1
 * if the request was successful, and 0 otherwise.
 * 
 * LOCKING: none.
 */
int printer_status(int sfd, struct job *jp) {
    int bufsz = IOBUFSZ;
    char *bp = malloc(IOBUFSZ);
    if (bp == NULL) {
        log_sys("printer_status: can't allocate read buffer");
    }
    ssize_t nr;
    int len;
    struct ipp_hdr h;
    int32_t jobid;
    int success = 0;
    while ((nr = tread(sfd, bp, bufsz, 5)) > 0) {
        /*
         * Find the status. Response starts with "HTTP/x.y"
         * so we can skip the first 8 characters.
         */
        char *cp = bp + 8;
        int datsz = nr;
        while (isspace((int)*cp)) {
            cp++;
        }
        char *statcode = cp;
        while (isdigit((int)*cp)) {
            cp++;
        }
        if (cp == statcode) { // Bad format; log it and move on
            log_msg(bp);
        } else {
            *cp++ = '\0';
            char *reason = cp;
            while (*cp != '\r' && *cp != '\n') {
                cp++;
            }
            *cp = '\0';
            int code = atoi(statcode);
            if (HTTP_INFO(code)) {
                continue;
            }
            if (!HTTP_SUCCESS(code)) {
                bp[datsz] = '\0';
                log_msg("error: %s", reason);
                break;
            }
            /*
             * Http request was okey, but still need to check
             * IPP status. Search for the Content-Length.
             */
            int i = cp - bp;
            for (;;) {
                while (*cp != 'C' && *cp != 'c' && i < datsz) {
                    cp++;
                    i++;
                }
                if (i >= datsz) { // get more header
                    if ((nr = readmore(sfd, &bp, i, &bufsz)) < 0) {
                        goto out;
                    } else {
                        cp = &bp[i];
                        datsz += nr;
                    }
                }
                if (strncasecmp(cp, "Content-Length:", 15) == 0) {
                    cp += 15;
                    while (isspace(*cp)) {
                        cp++;
                    }
                    char *contentlen = cp;
                    while (isdigit(cp)) {
                        cp++;
                    }
                    *cp++ = '\0';
                    i = cp - bp;
                    len = atoi(contentlen);
                    break;
                } else {
                    cp++;
                    i++;
                }
            }
            if (i >= datsz) {
                if ((nr = readmore(sfd, &bp, i, &bufsz)) < 0) {
                    goto out;
                } else {
                    cp = &bp[i];
                    datsz += nr;
                }
            }

            int found = 0;
            while (!found) { // look for end of HTTP header.
                while (i < datsz - 2) {
                    if (*cp == '\n' && *(cp + 1) == '\r' && *(cp + 2) == '\n') {
                        found = 1;
                        cp += 3;
                        i += 3;
                        break;
                    }
                    cp++;
                    i++;
                }
                if (i >= datsz - 2) {
                    if ((nr = readmore(sfd, &bp, datsz, &bufsz)) < 0) {
                        goto out;
                    } else {
                        cp = &bp[i];
                        datsz += nr;
                    }
                }
            }

            if (datsz - i < len) {
                if ((nr = readmore(sfd, &bp, datsz, &bufsz)) < 0) {
                    goto out;
                } else {
                    cp = &bp[i];
                    datsz += nr;
                }
            }
            memcpy(&h, cp, sizeof(struct ipp_hdr));
            i = ntohs(h.status);
            jobid = ntohl(h.request_id);
            if (jobid != jp->jobid) {
                log_msg("jobid %d status code %d", jobid, i);
                break;
            }

            if (STATCLASS_OK(i)) {
                success = 1;
            }
            break;
        }
    }
out:
    free(bp);
    if (nr < 0) {
        log_msg("jobid %d: error reading printer response: %s", jobid, strerror(errno));
    }
    return success;
}

void daemonize(const char *cmd) {
    umask(0);
    pid_t pid = fork();
    if (pid < 0) {
        err_sys("fork error");
    } else if (pid != 0) {
        exit(0);
    }
    log_open(cmd, LOG_CONS, LOG_DAEMON);
    if (setsid() < 0) {
        log_sys("setsid error");
    }
    if (chdir("/") < 0) {
        log_sys("chdir error");
    }
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        log_sys("open /dev/null error");
    }
    if (dup2(fd, STDIN_FILENO) != STDIN_FILENO) {
        log_sys("dup2 for stdin error");
    }
    if (dup2(fd, STDOUT_FILENO) != STDOUT_FILENO) {
        log_sys("dup2 for stout error");
    }
    if (dup2(fd, STDERR_FILENO) != STDERR_FILENO) {
        log_sys("dup2 for stderr error");
    }
    if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO) {
        close(fd);
    }
    log_msg("daemon started");
}
