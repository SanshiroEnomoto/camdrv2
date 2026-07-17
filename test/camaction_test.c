/* camaction_test.c */
/* Created by Sanshiro Enomoto on 23 July 1999. */
/* Last updated by Sanshiro Enomoto on 23 July 1999. */


#include <stdio.h>
#include <unistd.h>
#include "toyocamac.h"

#define FUNCTION_READ 0 
#define FUNCTION_CLEAR 9
#define FUNCTION_CLEAR_LAM 10
#define FUNCTION_ENABLE_LAM 26



int main(void)
{
    int lam_station_number = 3;
    
    int number_of_events = 100;
    int event_count;
    unsigned lam_bits;
    unsigned n, a, data;
    unsigned nxq;

    /* set crate number if necessary (default is 1) */
    /**/ setcn(1);

    //execz();

#if 1
    camac_0(lam_station_number, 0, FUNCTION_CLEAR_LAM);
    camac_0(lam_station_number, 0, FUNCTION_ENABLE_LAM);
#endif

    for (event_count = 0; event_count < number_of_events; event_count++) {
        while (1) {
            lam_bits = rlam();
            fprintf(stderr, "LAM BITS: %04x\n", lam_bits);
            if (lam_bits != 0) {
                break;
            }
	    usleep(300000);
        }

	for (n = 0; n < 24; n++) {
            if (lam_bits & (0x0001 << (n - 1))) {
	        for (a = 0; a < 16; a++) {
		    fprintf(stderr, "[i,N,A:%02d,%02d,%02d]", event_count, n, a);
                    nxq = camac_24(n, a, FUNCTION_READ, &data);
                    fprintf(stderr, "(nxq:%x): %x\n", nxq, data);
                    if (nxq & 0x02) { // No-X
                        break;
                    }
		}
                break;
                camac_0(n, 0, FUNCTION_CLEAR);
	    }
	}
#if 1
        camac_0(lam_station_number, 0, FUNCTION_CLEAR_LAM);
#else    
	execc();
#endif
    }

    return 0;
}
