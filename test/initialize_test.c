/* inhibit_test.c */
/* Created by Sanshiro Enomoto on 23 July 1999. */
/* Last updated by Sanshiro Enomoto on 23 July 1999. */


#include <stdio.h>
#include "camlib.h"


int main(void)
{
    if (COPEN() != 0) {
        perror("COPEN()");
        return -1;
    }

#if 0
    /* set crate number if necessary (default is 0) */
    if (CSETCR(0) != 0) {
        perror("CSETCR()");
        return -1;
    }
#endif

    if (CGENZ() != 0) {
        perror("CGENZ()");
        return -1;
    }

    if (CCLOSE() != 0) {
        perror("CCLOSE()");
    }

    return 0;
}
