#include <apue.h>
#include <sys/select.h>
#include <poll.h>

void sleep_us(unsigned int nusecs) {
    struct timeval tval;

    tval.tv_sec = nusecs / 1000000;
    tval.tv_usec = nusecs % 1000000;
    select(0, NULL, NULL, NULL, &tval);
}

void sleep_us_pool(unsigned int nusecs) {
    struct pollfd dummy;
    int timeout = nusecs / 1000;
    if (timeout <= 0) {
        timeout = 1;
    }
    poll(&dummy, 0, timeout);
}
