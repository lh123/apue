#include <stdio.h>
#include <netdb.h>

int main(void) {
    struct hostent *host;

    sethostent(1);
    printf("begin\n");
    while ((host = gethostent()) != NULL) {
        printf("name: %s\n", host->h_name);
        printf("alias:");
        for(char **alias = host->h_aliases; *alias != NULL; alias++) {
            printf(" %s", *alias);
        }
        printf("\n");
        printf("addrtype: %s\n", host->h_addrtype == AF_INET ? "AF_INET" : "AF_INET6");
        printf("length: %d\n", host->h_length);
        printf("addr:");
        for (char **addr = host->h_addr_list; addr != NULL; addr++) {
            printf(" %s", *addr);
        }
        printf("\n");
    }
    printf("end\n");
    endhostent();
}
