#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXLINE 4096

int main(void) {
    char line[MAXLINE + 1];
    if (setvbuf(stdout, NULL, _IOLBF, 0) != 0) {

    }
    while (fgets(line, MAXLINE, stdin) != NULL) {
        int int1, int2;
        if (sscanf(line, "%d%d", &int1, &int2) == 2) {
            if (printf("%d\n", int1 + int2) == EOF) {
                perror("printf error");
                exit(1);
            }
        } else {
            if (printf("invalid args\n") == EOF) {
                perror("printf error");
                exit(1);
            }
        }
    }
}
