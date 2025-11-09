/* inhibit_test.c */
/* Created by Sanshiro Enomoto on 23 July 1999. */
/* Last updated by Sanshiro Enomoto on 23 July 1999. */


#include <stdio.h>
#include "camlib.h"


int main(void)
{
    COPEN();

    /* set crate number if necessary (default is 0) */
    CSETCR(0);

    CGENZ();
    
    printf("press <ENTER> to set inhibit...");
    (void) getchar();
    CSETI();

    printf("press <ENTER> to release inhibit...");
    (void) getchar();
    CREMI();

    CCLOSE();

    return 0;
}
