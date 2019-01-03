#include <apue.h>
#include <unistd.h>
#include <termios.h>

int main(void) {
    struct termios term;
    if (isatty(STDIN_FILENO) == 0) {
        err_quit("standard input is not a terminal device");
    }
    long vdisable = fpathconf(STDIN_FILENO, _PC_VDISABLE);
    printf("vdisable: 0x%lx\n", vdisable);
    if (vdisable < 0) {
        err_quit("fpathconf error or _POSIX_VDISABLE not in effect");
    }
    
    if (tcgetattr(STDIN_FILENO, &term) < 0) {
        err_sys("tcgetattr error");
    }
    term.c_cc[VINTR] = vdisable; // disable INTR character
    term.c_cc[VEOF] = 2; // EOF is Ctrl-B

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &term) < 0) {
        err_sys("tcsetattr error");
    }
}
