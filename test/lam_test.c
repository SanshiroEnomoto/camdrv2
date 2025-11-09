/* lam_test.c */
/* Created by Sanshiro Enomoto on 23 July 1999. */
/* Last updated by Sanshiro Enomoto on 23 July 1999. */


#include <stdio.h>
#include "camlib.h"


int main(void)
{
    int timeout = 3;  /* sec */
    int result;
    int i;

    COPEN();

    /* set crate number if necessary (default is 0) */
    /* CSETCR(0); */

    CGENZ();
    
    for (i = 0; i < 16; i++) {
        printf("Waiting LAM ...");
        result = CWLAM(timeout);

        if (result == 0) {
	    printf("OK.\n");
	}
	else {
	    printf("timed out.\n");
	}	
	CGENC();
	CGENZ();
    }

    CCLOSE();

    return 0;
}
