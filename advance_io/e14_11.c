#include <apue.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define COPYINCR (1024 * 1024 * 1024)

int main(int argc, char *argv[]) {
    if (argc != 3) {
        err_quit("usage: %s <from> <to>", argv[0]);
    }

    int fdin, fdout;
    if ((fdin = open(argv[1], O_RDONLY)) < 0) {
        err_sys("can't open %s for reading", argv[1]);
    }
    if ((fdout = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
        err_sys("can't create %s for writing", argv[2]);
    }

    struct stat sbuf;
    if (fstat(fdin, &sbuf) < 0) {
        err_sys("fstat error");
    }

    if (ftruncate(fdout, sbuf.st_size) < 0) {
        err_sys("fruncate error");
    }

    off_t fsz = 0;
    size_t copysz;
    void *src, *dst;
    while (fsz < sbuf.st_size) {
        if ((sbuf.st_size - fsz) > COPYINCR) {
            copysz = COPYINCR;
        } else {
            copysz = sbuf.st_size - fsz;
        }

        if ((src = mmap(0, copysz, PROT_READ, MAP_SHARED, fdin, fsz)) == MAP_FAILED) {
            err_sys("mmap error for input");
        }
        if ((dst = mmap(0, copysz, PROT_WRITE | PROT_READ, MAP_SHARED, fdout, fsz)) == MAP_FAILED) {
            err_sys("mmap error for outpur");
        }

        //close fd to test
        close(fdin);
        close(fdout);

        memcpy(dst, src, copysz);
        munmap(src, copysz);
        munmap(dst, copysz);
        fsz += copysz;
    }
}
