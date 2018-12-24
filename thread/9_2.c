#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

static void pr_ids(const char *name) {
    printf("%s: pid = %ld, ppid = %ld, pgrp = %ld, tpgrp = %ld\n", name, (long)getpid(), (long)getppid(), (long)getpgrp(), (long)tcgetpgrp(STDIN_FILENO));
    fflush(stdout);
} 

int main(void) {
    pid_t pid;
    if ((pid = fork()) < 0) {
        printf("fork error\n");
        exit(-1);
    } else if (pid == 0) {
        //child
        printf("before setsid\n");
        pr_ids("child");
        if (setsid() < 0) {
            printf("setsid error\n");
            exit(-1);
        }
        printf("after setsid\n");
        pr_ids("child");
        execlp("ps", "ps", "-ef", NULL);
    } else {
        sleep(5);
        pr_ids("parents");
        execlp("ps", "ps", "-ef", NULL);
    }
}
