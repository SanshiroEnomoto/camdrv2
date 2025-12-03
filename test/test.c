
#include <stdio.h>
#include "camlib.h"


int main(void)
{
    int n=3, a, f, data=0, q, x;
        
    if (COPEN() != 0) {
        perror("COPEN()");
        return -1;
    }

    /* set crate number if necessary (default is 1) */
    if (CSETCR(1) != 0) {
        perror("CSETCR()");
        return -1;
    }

    if (CGENZ() != 0) {
        perror("CGENZ()");
        return -1;
    }

#if 1
    a = 0; f = 16; data = 0x555555;
    CAMAC(NAF(n, a, f), &data, &q, &x);
    printf("NAF:%d,%d,%d, data:%06x, q:%d, x:%d\n", n,a,f, data, q, x);
#endif

#if 1
    a = 0; f = 0;
    CAMAC(NAF(n, a, f), &data, &q, &x);
    printf("NAF:%d,%d,%d, data:%06x, q:%d, x:%d\n", n,a,f, data, q, x);
#endif
    
#if 1
    a = 1; f = 0;
    CAMAC(NAF(n, a, f), &data, &q, &x);
    printf("NAF:%d,%d,%d, data:%06x, q:%d, x:%d\n", n,a,f, data, q, x);
#endif

#if 0
    a = 0; f = 8;
    CAMAC(NAF(n, a, f), &data, &q, &x);
    printf("NAF:%d,%d,%d, data:%06x, q:%d, x:%d\n", n,a,f, data, q, x);
#endif

#if 0
    a = 0; f = 10;
    CAMAC(NAF(n, a, f), &data, &q, &x);
    printf("NAF:%d,%d,%d, data:%06x, q:%d, x:%d\n", n,a,f, data, q, x);
#endif

    CCLOSE();

    return 0;
}
