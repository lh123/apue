#include <apue.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

static void pr_winsize(int fd) {
    struct winsize size;
    if (ioctl(fd, TIOCGWINSZ, &size) < 0) {
        err_sys("TIOCGWINSZ error");
    }
    printf("%d rows, %d columns\n", size.ws_row, size.ws_col);
}

static void sig_winch(int signo) {
    printf("SIGWINCH received\n");
    pr_winsize(STDIN_FILENO);
}

int main(void) {
    if (isatty(STDIN_FILENO) == 0) {
        exit(1);
    }
    struct sigaction act;
    act.sa_handler = sig_winch;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    if (sigaction(SIGWINCH, &act, NULL) < 0) {
        err_sys("sigaction error");
    }
    pr_winsize(STDIN_FILENO);
    for (;;) {
        pause();
    }
}
