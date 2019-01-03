#include <apue.h>
#include <stdio.h>
#include <signal.h>
#include <termios.h>

#define MAX_PASS_LEN 8

char *my_getpass(const char *prompt) {
    static char buf[MAX_PASS_LEN + 1];

    FILE *fp = fopen(ctermid(NULL), "r+");
    if (fp == NULL) {
        return NULL;
    }
    setbuf(fp, NULL);

    sigset_t sig, osig;
    sigemptyset(&sig);
    sigaddset(&sig, SIGINT);
    sigaddset(&sig, SIGTSTP);
    sigprocmask(SIG_BLOCK, &sig, &osig);

    struct termios ts, ots;
    tcgetattr(fileno(fp), &ts);
    ots = ts;
    ts.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    tcsetattr(fileno(fp), TCSAFLUSH, &ts);
    fputs(prompt, fp);

    char *ptr = buf;
    int c;
    while ((c = getc(fp)) != EOF && c != '\n') {
        if (ptr < &buf[MAX_PASS_LEN]) {
            *ptr++ = c;
        }
    }
    *ptr = 0;
    putc('\n', fp);

    tcsetattr(fileno(fp), TCSAFLUSH, &ots);
    sigprocmask(SIG_SETMASK, &osig, NULL);
    fclose(fp);
    return buf;
}

int main(void) {
    char *ptr = my_getpass("Enter password:");
    if (ptr == NULL) {
        err_sys("getpass error");
    }
    printf("password: %s\n", ptr);

    while (*ptr != 0) {
        *ptr++ = 0;
    }
}
