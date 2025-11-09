/* camaction_test.c */
/* Created by Enomoto Sanshiro on 23 July 1999. */
/* Last updated by Enomoto Sanshiro on 23 July 1999. */


#include <stdio.h>
#include "toyocamac.h"

#define FUNCTION_READ 0 
#define FUNCTION_CLEAR 9 

//#define DEBUG


int main(void)
{
    int number_of_events = 5;
    int event_count;
    unsigned lam_bits;
    unsigned n, a, f, data;
    unsigned nxq;

    /* set crate number if necessary (default is 0) */
    /* setcn(0); */

    execz();
    
    for (event_count = 0; event_count < number_of_events; event_count++) {
        while (! (lam_bits = rlam())) {
	    ;
	}
	fprintf(stderr, "LAM BITS: %04x\n", lam_bits);
#ifdef DEBUG
	fprintf(stderr, "LAM BITS: %04x\n", lam_bits);
#endif

	for (n = 0; n < 24; n++) {
            if (lam_bits & (0x0001 << (n - 1))) {
		a = 0;
		f = FUNCTION_READ;

	        while (! (nxq = camac_24(n, a, f, &data))) {
		    fprintf(stderr, "[%02d:%02d:%02d] ", event_count, n, a);
		    fprintf(stdout, "%d\n", data);
		    a++;
#ifdef DEBUG
                    fprintf(stderr, "NXQ: %04x\n", nxq);
#endif
		}
                fprintf(stderr, "NXQ: %04x\n", nxq);
                camac_0(n, 0, FUNCTION_CLEAR);
	    }
	}

	execc();
    }

    return 0;
}
