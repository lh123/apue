#include <apue.h>
#include <stdio.h>

int main(void) {
    FILE *fin = popen("/nosuchfile", "w");
    if (fin == NULL) {
        err_sys("popen error");
    }
    pclose(fin);
}
