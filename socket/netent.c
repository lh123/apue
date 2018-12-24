#include <stdio.h>
#include <netdb.h>

int main(void) {
    struct netent *net;

    setnetent(1);

    while ((net = getnetent()) != NULL) {
        printf("name: %s\n", net->n_name);
        printf("alias:");
        for (char **alias = net->n_aliases; *alias != NULL; alias++) {
            printf(" %s", *alias);
        }
        printf("\n");
        printf("type: %s\n", net->n_addrtype == AF_INET ? "AF_INET" : "AF_INET6");
        printf("net: %u\n", net->n_net);
    }

    endnetent();
}
