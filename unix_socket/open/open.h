#if !defined(_OPEN_H_)
#define _OPEN_H_

#include <apue.h>
#include <errno.h>

#define CL_OPEN "open"

/*
 * Open the file by sending the "name" and "oflag" to the
 * connection server and reading a file descriptor back.
 */
int csopen(char *name, int oflag);

#endif