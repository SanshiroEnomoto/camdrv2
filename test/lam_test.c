/* lam_test.c */
/* Created by Sanshiro Enomoto on 23 July 1999. */
/* Last updated by Sanshiro Enomoto on 23 July 1999. */


#include <stdio.h>
#include "camlib.h"


int main(void)
{
    int n=3, a=0, f=26, data=0, q, x;
    int timeout = 3;  /* sec */
    int result;
    int i;
#if 1
    if (COPEN() != 0) {
        perror("COPEN()");
        return -1;
    }

    /* set crate number if necessary (default is 1) */
    unsigned crate_number = 1;
    if (CSETCR(crate_number) != 0) {
        perror("CSETCR()");
        return -1;
    }

    if (CGENZ() != 0) {
        perror("CGENZ()");
        return -1;
    }
#endif
#if 1
    // enable LAM (F26)
    f = 26;
    result = CAMAC(NAF(n, a, f), &data, &q, &x);
    printf("NAF:%d,%d,%d, data:%06x, q:%d, x:%d, result=%d\n", n,a,f, data, q, x, result);

    // clear LAM (F10)
    f = 9;
    result = CAMAC(NAF(n, a, f), &data, &q, &x);
    printf("NAF:%d,%d,%d, data:%06x, q:%d, x:%d, result=%d\n", n,a,f, data, q, x, result);
#endif

    for (i = 0; i < 16; i++) {
        printf("Waiting for a LAM ...");
        result = CWLAM(timeout);

        if (result == 0) {
	    printf("OK.\n");
	}
	else {
	    printf("timed out: result=%d\n", result);
	}

#if 1
        a = 0; f = 8;
        result = CAMAC(NAF(n, a, f), &data, &q, &x);
        printf("8888 NAF:%d,%d,%d, data:%06x, q:%d, x:%d, result=%d\n", n,a,f, data, q, x, result);
#endif
#if 1
        a = 0; f = 0;
        result = CAMAC(NAF(n, a, f), &data, &q, &x);
        printf("NAF:%d,%d,%d, data:%06x, q:%d, x:%d, result=%d\n", n,a,f, data, q, x, result);
#endif

    
#if 1
        // clear LAM (F10)
        f = 9;
        result = CAMAC(NAF(n, a, f), &data, &q, &x);
        printf("NAF:%d,%d,%d, data:%06x, q:%d, x:%d, result=%d\n", n,a,f, data, q, x, result);
#else
	CGENC();
	CGENZ();
#endif
    }
    
    CCLOSE();

    return 0;
}
