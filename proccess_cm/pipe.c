#include <apue.h>
#include <unistd.h>

int main(void) {
    int fd[2];
    if (pipe(fd) < 0) {
        err_sys("pipe error");
    }

    pid_t pid = fork();
    if (pid < 0) {
        err_sys("fork error");
    } else if (pid == 0) {
        close(fd[1]); // close write for child
        char line[MAXLINE];
        int n = read(fd[0], line, MAXLINE);
        write(STDOUT_FILENO, line, n);
    } else {
        close(fd[0]); // close read for parent
        const char msg[] = "hello world\n";
        write(fd[1], msg, sizeof(msg) - 1);
    }
}
