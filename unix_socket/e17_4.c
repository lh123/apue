#include <apue.h>
#include <stdlib.h>
#include <string.h>

#define WHITE " \t\n"

int buf_args(char *buf, int (*optfunc)(int, char **)) {
    int argc_len = 10;
    int argc = 0;
    char **argv = malloc(argc_len * sizeof(char *));
    if (argv == NULL) {
        err_sys("malloc error");
    }

    if (strtok(buf, WHITE) == NULL) {
        return -1;
    }
    argv[argc] = buf;
    char *ptr;
    while ((ptr = strtok(NULL, WHITE)) != NULL) {
        if (++argc >= argc_len) {
            argc_len *= 2;
            if ((argv = realloc(argv, argc_len * sizeof(char *))) == NULL) {
                err_sys("realloc error");
            }
        }
        argv[argc] = ptr;
    }
    argv[++argc] = NULL;
    int ret = optfunc(argc, argv);
    free(argv);
    return ret;
}
