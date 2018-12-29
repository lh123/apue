#if !defined(_OPEND_H_)
#define _OPEND_H_
#include <apue.h>

#define CL_OPEN "open"

extern char errmsg[MAXLINE];
extern int oflag;
extern char *pathname;

/*
 * This function is called by buf_args(), which is called by
 * handle_request(). buf_args() has broken up the client's
 * buffer into an argv[]-style array, which we now process.
 */
int cli_args(int, char **);
void handle_request(char *buf, int nread, int fd);

#endif // _OPEND_H_
