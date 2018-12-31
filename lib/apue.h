#if !defined(_APUE_H)
#define _APUE_H

#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#define MAXLINE 4096

#include <stdio.h>
#include <unistd.h>

void __attribute__((format(printf, 1, 2))) err_ret(const char *fmt, ...);
void __attribute__((noreturn, format(printf, 1, 2))) err_sys(const char *fmt, ...);
void __attribute__((format(printf, 2, 3))) err_cont(int error, const char *fmt, ...);
void __attribute__((noreturn, format(printf, 2, 3))) err_exit(int error, const char *fmt, ...);
void __attribute__((noreturn, format(printf, 1, 2))) err_dump(const char *fmt, ...);
void __attribute__((format(printf, 1, 2))) err_msg(const char *fmt, ...);
void __attribute__((noreturn, format(printf, 1, 2))) err_quit(const char *fmt, ...);

extern int log_to_stderr;

void log_open(const char *ident, int option, int facility);
void __attribute__((format(printf, 1, 2))) log_ret(const char *fmt, ...);
void __attribute__((noreturn, format(printf, 1, 2))) log_sys(const char *fmt, ...);
void __attribute__((format(printf, 2, 3))) log_cont(int error, const char *fmt, ...);
void __attribute__((noreturn, format(printf, 2, 3))) log_exit(int error, const char *fmt, ...);
void __attribute__((format(printf, 1, 2))) log_msg(const char *fmt, ...);
void __attribute__((noreturn, format(printf, 1, 2))) log_quit(const char *fmt, ...);

ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, const void *ptr, size_t n);

int serv_listen(const char *name);
int serv_accept(int listenfd, uid_t *uidptr);
int cli_conn(const char *name);

/*
 * Pass a file descriptor to another process.
 * If fd_to_send < 0, then -fd_to_send is sent back instead as the error status.
 */
int send_fd(int fd, int fd_to_send);

/*
 * Receive a file descriptor from a server process. Also, any data
 * received is passed to (*userfunc)(STDERR_FILENO, buf, nbytes).
 * We have a 2-byte protocol for receiving the fd from send_fd().
 */
int recv_fd(int fd, ssize_t (*userfunc)(int, const void *, size_t));

/*
 * Used when we had planned to send an fd using send_fd(),
 * but encountered an error instead. We send the error back
 * using the send_fd()/recv_fd() protocol.
 */
int send_err(int fd, int errcode, const char *msg);



#endif