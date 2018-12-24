#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXLINE 4096

int main(void) {
    int n;
    char line[MAXLINE + 1];
    while ((n = read(STDIN_FILENO, line, MAXLINE)) > 0) {
        line[n] = 0;
        int int1, int2;
        if (sscanf(line, "%d%d", &int1, &int2) == 2) {
            sprintf(line, "%d\n", int1 + int2);
            n = strnlen(line, MAXLINE);
            if (write(STDOUT_FILENO, line, n) != n) {
                perror("write error");
                exit(1);
            }
        } else {
            if (write(STDOUT_FILENO, "invalid args\n", 13) != 13) {
                perror("write error");
                exit(1);
            }
        }
    }
}
