#include <stdio.h>
#include <netdb.h>

int main(void) {
    struct protoent *prot;

    setprotoent(1);
    
    while ((prot = getprotoent()) != NULL) {
        printf("\n");
        printf("name: %s\n", prot->p_name);
        printf("alias:");
        for (char **alias = prot->p_aliases; *alias != NULL; alias++) {
            printf(" %s", *alias);
        }
        printf("\n");
        printf("proto: %d\n", prot->p_proto);
    }
    
    endprotoent();

}
