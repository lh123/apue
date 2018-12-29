#include "open.h"
#include <string.h>
#include <fcntl.h>

#define BUFFSIZE 8192

int main(int argc, char *argv[]) {
    char buf[BUFFSIZE];
    char line[MAXLINE];

    while (fgets(line, MAXLINE, stdin) != NULL) {
        if (line[strlen(line) - 1] == '\n') {
            line[strlen(line) - 1] = 0;
        }
        int fd = csopen(line, O_RDONLY);
        if (fd < 0) {
            continue;
        }

        int n;
        while ((n = read(fd, buf, BUFFSIZE)) > 0) {
            if (write(STDOUT_FILENO, buf, n) != n) {
                err_sys("write error");
            }
        }
        if (n < 0) {
            err_sys("read error");
        }
        close(fd);
    }
}
