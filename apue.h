#if !defined(_APUE_H)
#define _APUE_H

#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#define MAXLINE 4096

#include <stdio.h>


void __attribute__((format(printf, 1, 2))) err_ret(const char *fmt, ...);
void __attribute__((noreturn, format(printf, 1, 2))) err_sys(const char *fmt, ...);
void __attribute__((format(printf, 2, 3))) err_cont(int error, const char *fmt, ...);
void __attribute__((noreturn, format(printf, 2, 3))) err_exit(int error, const char *fmt, ...);
void __attribute__((noreturn, format(printf, 1, 2))) err_dump(const char *fmt, ...);
void __attribute__((format(printf, 1, 2))) err_msg(const char *fmt, ...);
void __attribute__((noreturn, format(printf, 1, 2))) err_quit(const char *fmt, ...);

ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, const void *ptr, size_t n);

#endif