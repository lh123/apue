#include <stdio.h>
#include <netdb.h>

uint16_t my_ntohs(uint16_t port) {
    uint8_t hi = port >> 8;
    uint8_t lo = port & 0xFF;
    return lo << 8 | hi;
}

int main(void) {
    setservent(1);

    struct servent *serv;

    while ((serv = getservent()) != NULL) {
        printf("\n");
        printf("name: %s\n", serv->s_name);
        printf("alias:");
        for (char **alias = serv->s_aliases; *alias != NULL; alias++) {
            printf(" %s", *alias);
        }
        printf("\n");
        printf("port: %u\n", ntohs(serv->s_port));
        printf("proto: %s\n", serv->s_proto);
    }

    endservent();
}

