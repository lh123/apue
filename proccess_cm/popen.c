#include <apue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char line[4096];
    FILE *fpin = popen("./filter", "r");
    if (fpin == NULL) {
        err_sys("popen error");
    }

    for (;;) {
        fputs("prompt> ", stdout);
        fflush(stdout);
        if (fgets(line, 4096, fpin) == NULL) {
            break;
        }
        if (fputs(line, stdout) == EOF) {
            err_sys("fputs error to pipe");
        }
    }
    if (ferror(fpin)) {
        err_sys("fgets error from pipe");
    }
    if (pclose(fpin) == -1) {
        err_sys("pclose error");
    }
    putchar('\n');
}
