
#include <stdio.h>
#include <stdlib.h>
#include "toyocamac.h"

int main(void)
{
    execz();

    printf("making 1000 IN/OUT transactions... \n");
    
    printf("start: ");
    fflush(stdout);
    system("date");
    
    for (int i = 0; i < 1000; i++) {
        rlam();
    }

    printf("finish: ");
    fflush(stdout);
    system("date");

    return 0;
}
