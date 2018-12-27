#include <apue.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PRINT_STAT(type, value) printf(#value ": " type "\n", value)

int main(void) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8888);

    int sockfd = socket(addr.sin_family, SOCK_STREAM, 0);
    struct stat sta;
    if (fstat(sockfd, &sta) < 0) {
        err_sys("fstat error");
    }
    PRINT_STAT("%ld", sta.st_atim.tv_sec);
    PRINT_STAT("%ld", sta.st_atim.tv_nsec);
    PRINT_STAT("%ld", sta.st_blksize);
    PRINT_STAT("%ld", sta.st_blocks);
    PRINT_STAT("%ld", sta.st_ctim.tv_sec);
    PRINT_STAT("%ld", sta.st_ctim.tv_nsec);
    PRINT_STAT("%lu", sta.st_dev);
    PRINT_STAT("%u", sta.st_gid);
    PRINT_STAT("%lu", sta.st_ino);
    PRINT_STAT("%u", sta.st_mode);
    PRINT_STAT("%ld", sta.st_mtim.tv_sec);
    PRINT_STAT("%ld", sta.st_mtim.tv_nsec);
    PRINT_STAT("%lu", sta.st_nlink);
    PRINT_STAT("%lu", sta.st_rdev);
    PRINT_STAT("%ld", sta.st_size);
    PRINT_STAT("%u", sta.st_uid);
    close(sockfd);
}
