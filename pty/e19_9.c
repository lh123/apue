#include <apue.h>
#include <errno.h>
#include <sys/wait.h>

#define PROGRAM "./a.out"

static int fdm;
static pid_t pid;

static void sig_term(int signo) {
    kill(pid, SIGTERM);
}

static void sig_winch(int signo) {
    struct winsize size;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &size) < 0) {
        err_sys("TIOCGWINSZ error");
    }
    if (ioctl(fdm, TIOCSWINSZ, &size) < 0) {
        err_sys("TIOCSWINSZ error");
    }
}

int main(void) {
    pid = pty_fork(&fdm, NULL, 0, NULL, NULL);
    if (pid < 0) {
        err_sys("pty_fork error");
    } else if (pid == 0) {
        execl(PROGRAM, "a.out", NULL);
        err_sys("execl for %s error", PROGRAM);
    }

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

    char buf[MAXLINE];
    
    while (1) {
        int nread = read(fdm, buf, MAXLINE);
        if (nread < 0 && errno != EINTR) {
            err_sys("read master pty error");
        }
        if (nread == 0) {
            break;
        }
        write(STDOUT_FILENO, "child: ", 7);
        if (write(STDOUT_FILENO, buf, nread) != nread) {
            err_sys("write to stdout error");
        }
    }
}
