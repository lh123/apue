#include <apue.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ioctl.h>

static void sig_term(int signo) {
    printf("child caughted SIGTERM\n");
}

static void sig_winch(int signo) {
    struct winsize size;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &size) < 0) {
        err_sys("TIOCGWINSZ error");
    }
    printf("child row: %d, col: %d\n", size.ws_row, size.ws_col);
}

int main(void) {
    struct sigaction act;
    act.sa_handler = sig_term;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    if (sigaction(SIGTERM, &act, NULL) < 0) {
        err_sys("sigaction SIGTERM error");
    }

    act.sa_handler = sig_winch;
    if (sigaction(SIGWINCH, &act, NULL) < 0) {
        err_sys("sigaction SIGWINCH error");
    }

    while (1) {
        pause();
    }
}
