#include <apue.h>
#include <stdio.h>
#include <termios.h>

#define CASE_CS(X) \
            case CS ## X: \
                printf(#X " bits/byte\n"); \
                break

int main(void) {
    struct termios term;
    if (tcgetattr(STDIN_FILENO, &term) < 0) {
        err_sys("tcgetattr error");
    }
    switch (term.c_cflag & CSIZE) {
    CASE_CS(5);
    CASE_CS(6);
    CASE_CS(7);
    CASE_CS(8);
    default:
        printf("unknown bits/byte\n");
    }
    term.c_cflag &= ~CSIZE;
    term.c_cflag |= CS8;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &term) < 0) {
        err_sys("tcsetattr error");
    }
}
