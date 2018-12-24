#include <apue.h>
#include <ctype.h>
#include <fcntl.h>
#include <aio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define BSZ 4096
#define NTASK 8

enum rwop {
    UNUSED = 0,
    READ_PENDING = 1,
    WRITE_PENDING = 2
};

struct aio_task {
    enum rwop op;
    int is_last;
    struct aiocb aiocb;
    unsigned char data[BSZ];
};

struct aio_task tasks[NTASK];

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

int main(void) {
    int infd = STDIN_FILENO;
    int outfd = STDOUT_FILENO;

    struct stat sta;
    if (fstat(infd, &sta) < 0) {
        err_sys("fstat failed\n");
    }

    int is_tty_or_pipe;

    if (!S_ISREG(sta.st_mode)) {
        is_tty_or_pipe = 1;
    } else {
        is_tty_or_pipe = 0;
    }

    const struct aiocb *aiolist[NTASK];

    for (int i = 0; i < NTASK; i++) {
        tasks[i].op = UNUSED;
        tasks[i].aiocb.aio_buf = tasks[i].data;
        tasks[i].aiocb.aio_sigevent.sigev_notify = SIGEV_NONE;
        aiolist[i] = NULL;
    }

    int numtask = 0;
    off_t cur_off = 0;
    int err;
    int num;
    for (;;) {
        for (int i = 0; i < NTASK; i++) {
            switch (tasks[i].op) {
            case UNUSED:
                if (cur_off < sta.st_size || is_tty_or_pipe) {
                    tasks[i].op = READ_PENDING;
                    tasks[i].aiocb.aio_fildes = infd;
                    tasks[i].aiocb.aio_nbytes = BSZ;
                    tasks[i].aiocb.aio_offset = cur_off;
                    
                    cur_off += BSZ;
                    if (cur_off >= sta.st_size) {
                        tasks[i].is_last = 1;
                    }
                    if (aio_read(&tasks[i].aiocb) < 0) {
                        err_sys("aio_read failed");
                    }
                    aiolist[i] = &tasks[i].aiocb;
                    numtask++;
                }
                break;
            case READ_PENDING:
                err = aio_error(&tasks[i].aiocb);
                if (err == EINPROGRESS) {
                    continue;
                }
                if (err != 0) {
                    if (err == -1) {
                        err_sys("aio_error failed");
                    } else {
                        err_exit(err, "aio_read failed");
                    }
                }
                num = aio_return(&tasks[i].aiocb);
                if (num < 0) {
                    err_sys("aio_return failed");
                }
                if (num != BSZ && !is_tty_or_pipe && !tasks[i].is_last) {
                    err_quit("short read (%d/%d)", num, BSZ);
                }
                for (int j = 0; j < num; j++) {
                    tasks[i].data[j] = translate(tasks[i].data[j]);
                }

                tasks[i].op = WRITE_PENDING;
                tasks[i].aiocb.aio_fildes = outfd;
                tasks[i].aiocb.aio_nbytes = num;

                if (aio_write(&tasks[i].aiocb) < 0) {
                    err_sys("aio_write failed");
                }
                break;
            case WRITE_PENDING:
                err = aio_error(&tasks[i].aiocb);
                if (err == EINPROGRESS) {
                    continue;
                }
                if (err != 0) {
                    if (err == -1) {
                        err_sys("aio_error failed");
                    } else {
                        err_exit(err, "aio_read failed");
                    }
                }
                num = aio_return(&tasks[i].aiocb);
                if (num != tasks[i].aiocb.aio_nbytes) {
                    err_quit("short write (%d/%d)", num, (int)tasks[i].aiocb.aio_nbytes);
                }
                tasks[i].op = UNUSED;
                aiolist[i] = NULL;
                numtask--;
                break;
            }
        }
        // printf("numtask = %d\n", numtask);
        if (numtask == 0) {
            if (cur_off >= sta.st_size) {
                break;
            }
        } else if (numtask != 0) {
            if (aio_suspend(aiolist, NTASK, NULL) < 0) {
                err_sys("aio_suspend failed");
            }
        }
    }
    tasks[0].aiocb.aio_fildes = outfd;
    if (aio_fsync(O_SYNC, &tasks[0].aiocb) < 0) {
        err_sys("aio_fsync failed");
    }
}
