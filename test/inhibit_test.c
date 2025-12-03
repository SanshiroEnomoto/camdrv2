/* inhibit_test.c */
/* Created by Sanshiro Enomoto on 23 July 1999. */
/* Last updated by Sanshiro Enomoto on 23 July 1999. */


#include <stdio.h>
#include "camlib.h"


int main(void)
{
    if (COPEN() < 0) {
        perror("COPEN()");
        return -1;
    }

    /* set crate number if necessary (default is 1) */
    if (CSETCR(1) < 0) {
        perror("CSETCR()");
        return -1;
    }

    if (CGENZ() < 0) {
        perror("CGENZ()");
        return -1;
    }
    
    printf("press <ENTER> to set inhibit...");
    (void) getchar();
    if (CSETI() < 0) {
        perror("CSETI()");
    }
    
    printf("press <ENTER> to release inhibit...");
    (void) getchar();
    if (CREMI() < 0) {
        perror("CREMI()");
    }

    if (CCLOSE() < 0) {
        perror("CCLOSE()");
    }

    return 0;
}
