#include <apue.h>
#include <stdlib.h>
#include <signal.h>

static void sig_catch(int signo) {
    printf("signal caught\n");
    tty_reset(STDIN_FILENO);
    exit(0);
}

int main(void) {
    if (signal(SIGINT, sig_catch) == SIG_ERR) {
        err_sys("signal(SIGINT) error");
    }
    if (signal(SIGQUIT, sig_catch) == SIG_ERR) {
        err_sys("signal(SIGQUIT) error");
    }
    if (signal(SIGTERM, sig_catch) == SIG_ERR) {
        err_sys("signal(SIGTERM) error");
    }

    if (tty_raw(STDIN_FILENO) < 0) {
        err_sys("tty_raw error");
    }
    printf("Enter raw mode characters, terminate with DELETE\n");
    int i;
    char c;
    while ((i = read(STDIN_FILENO, &c, 1)) == 1) {
        if ((c &= 0xFF) == 0177) { // 0177 = ASCII DELETE
            break;
        }
        printf("%o\n", c);
    }
    if (tty_reset(STDIN_FILENO) < 0) {
        err_sys("tty_reset error");
    }
    if (i <= 0) {
        err_sys("read error");
    }
    if (tty_cbreak(STDIN_FILENO) < 0) {
        err_sys("tty_cbreak error");
    }
    printf("\nEnter cbreak mode characters, terminate with SIGINT\n");
    while ((i = read(STDIN_FILENO, &c, 1)) == 1) {
        c &= 0xFF;
        printf("%o\n", c);
    }
    if (tty_reset(STDIN_FILENO) < 0) {
        err_sys("tty_reset error");
    }
    if (i <= 0) {
        err_sys("read error");
    }
}
