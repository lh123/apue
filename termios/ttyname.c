#include <apue.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

struct devdir {
    struct devdir *d_next;
    char *d_name;
};

static struct devdir *head;
static struct devdir *tail;
static char pathname[_POSIX_PATH_MAX + 1];

static void add(char *dirname) {
    int len = strlen(dirname);
    // xxx/. || xxx/..
    // Skip ., .., and /dev/fd
    if (dirname[len - 1] == '.' && (dirname[len - 2] == '/' || (dirname[len - 2] == '.' && dirname[len - 3] == '/'))) {
        return;
    }
    if (strcmp(dirname, "/dev/fd") == 0) {
        return;
    }
    struct devdir *ddp = malloc(sizeof(struct devdir));
    if (ddp == NULL) {
        return;
    }
    if ((ddp->d_name = strdup(dirname)) == NULL) {
        free(ddp);
        return;
    }
    ddp->d_next = NULL;
    if (tail == NULL) {
        head = ddp;
        tail = ddp;
    } else {
        tail->d_next = ddp;
        tail = ddp;
    }
}

static void cleanup(void) {
    struct devdir *ddp = head, *nddp;
    while (ddp != NULL) {
        nddp = ddp->d_next;
        free(ddp->d_name);
        free(ddp);
        ddp = nddp;
    }
    head = NULL;
    tail = NULL;
}

static char *searchdir(char *dirname, struct stat *fdstatp) {
    strcpy(pathname, dirname);
    DIR *dp = opendir(dirname);
    if (dp == NULL) {
        return NULL;
    }
    strcat(pathname, "/");
    int devlen = strlen(pathname);
    struct dirent *dirp;
    struct stat devstat;
    while ((dirp = readdir(dp)) != NULL) {
        strncpy(pathname + devlen, dirp->d_name, _POSIX_PATH_MAX - devlen);
        if (strcmp(pathname, "/dev/stdin") == 0 ||
            strcmp(pathname, "/dev/stdout") == 0 ||
            strcmp(pathname, "/dev/stderr") == 0) {
            continue;
        }
        if (stat(pathname, &devstat) < 0) {
            continue;
        }
        if (S_ISDIR(devstat.st_mode)) {
            add(pathname);
            continue;
        }
        if (devstat.st_ino == fdstatp->st_ino &&
            devstat.st_dev == fdstatp->st_dev) {
            closedir(dp);
            return pathname;
        }
    }
    closedir(dp);
    return NULL;
}

char *my_ttyname(int fd) {
    if (isatty(fd) == 0) {
        return NULL;
    }
    struct stat fdstat;
    if (fstat(fd, &fdstat) < 0) {
        return NULL;
    }
    if (S_ISCHR(fdstat.st_mode) == 0) {
        return NULL;
    }
    // bfs
    char *rval = searchdir("/dev", &fdstat);
    if (rval == NULL) {
        struct devdir *ddp;
        for (ddp = head; ddp != NULL; ddp = ddp->d_next) {
            if ((rval = searchdir(ddp->d_name, &fdstat)) != NULL) {
                break;
            }
        }
    }
    cleanup();
    return rval;
}

int main(void) {
    char *name;
    if (isatty(0)) {
        name = my_ttyname(0);
        if (name == NULL) {
            name = "undefined";
        }
    } else {
        name = "not a tty";
    }
    printf("fd 0: %s\n", name);

    if (isatty(1)) {
        name = my_ttyname(1);
        if (name == NULL) {
            name = "undefined";
        }
    } else {
        name = "not a tty";
    }
    printf("fd 1: %s\n", name);

    if (isatty(2)) {
        name = my_ttyname(2);
        if (name == NULL) {
            name = "undefined";
        }
    } else {
        name = "not a tty";
    }
    printf("fd 2: %s\n", name);
}
