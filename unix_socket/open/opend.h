#if !defined(_OPEND_H_)
#define _OPEND_H_
#include <apue.h>

#define CL_OPEN "open"
#define CS_OPEN "/tmp/opend.socket"

extern int debug; // nonzero if interactive (not daemon)
extern char errmsg[MAXLINE]; //error message string to return to client
extern int oflag; // open flag: O_XXX
extern char *pathname; // of file to open for client

typedef struct {
    int fd;
    uid_t uid;
} Client;

extern Client *client; // ptr to malloc'ed array
extern int client_size; // # entries in client[] array

/*
 * This function is called by buf_args(), which is called by
 * handle_request(). buf_args() has broken up the client's
 * buffer into an argv[]-style array, which we now process.
 */
int cli_args(int, char **);
int client_add(int, uid_t);
void client_del(int);
void __attribute__((noreturn)) loop(void);
void handle_request(char *buf, int nread, int fd, uid_t uid);

#endif // _OPEND_H_
