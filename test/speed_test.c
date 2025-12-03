
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "toyocamac.h"

int main(void)
{
    int N = 1000;
    unsigned n=3, a=0, f=0, data;
    time_t start, end;
    
    execz();

    printf("making %d CAMAC transactions... \n", N);
    
    printf("start: ");
    fflush(stdout);
    system("date");
    
    start = time(NULL);
    
    for (int i = 0; i < N; i++) {
        camac_24(n, a, f, &data);
    }

    end = time(NULL);
    
    printf("finish: ");
    fflush(stdout);
    system("date");

    printf("lapse: %ld\n", end-start);
    
    return 0;
}
