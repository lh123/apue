#include <apue.h>
#include <stdio.h>
#include <string.h>

static char ctermid_name[L_ctermid];

char *my_ctermid(char *str) {
    if (str == NULL) {
        str = ctermid_name;
    }
    return strcpy(str, "/dev/tty");
}

int main(void) {
    printf("ctermid: %s\n", ctermid(NULL));
    printf("my_ctermid: %s\n", my_ctermid(NULL));
}
