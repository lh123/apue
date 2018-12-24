#include "apue.h"
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define BSZ 4096

unsigned char buf[BSZ];

unsigned char translate(unsigned char c) {
    if (isalpha(c)) {
        if (c + 13 > 'z') {
            c = c + 13 - 'z' - 1 + 'a';
        } else if (c >= 'a') {
            c += 13;
        } else if (c + 13 > 'Z') {
            c = c + 13 - 'Z' - 1 + 'A';
        } else {
            c += 13;
        }
    }
    return c;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        err_quit("usage: rot13 infile outfile");
    }

    int ifd, ofd;
    if ((ifd = open(argv[1], O_RDONLY)) < 0) {
        err_sys("can't open %s", argv[1]);
    }
    if ((ofd = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
        err_sys("can't create %s", argv[2]);
    }

    int n;
    while ((n = read(ifd, buf, BSZ)) > 0) {
        for (int i = 0; i < n; i++) {
            buf[i] = translate(buf[i]);
        }
        int nw;
        if ((nw = write(ofd, buf, n)) != n) {
            if (nw < 0) {
                err_sys("write failed");
            } else {
                err_quit("short write (%d/%d)", nw, n);
            }
        }
    }

    fsync(ofd);
}
