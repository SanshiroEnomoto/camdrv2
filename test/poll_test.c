/* poll_test.c */
/* Created by Sanshiro Enomoto on 27 October 2005. */
/* Last updated by Sanshiro Enomoto on 27 October 2005. */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include "camdrv.h"


#define N_REPEATS 32
#define TIMEOUT_SEC 3


int main(void)
{
    int fd;
    struct pollfd fd_set[2];
    static unsigned ioctl_data[2];
    char buffer[1024];
    int i;

    if ((fd = open("/dev/camdrv", O_RDWR)) == -1) {
	perror("open()");
	exit(EXIT_FAILURE);
    }

    ioctl(fd, CAMDRV_IOC_INITIALIZE);

    /* stdin */
    fd_set[0].fd = 0;
    fd_set[0].events = POLLIN;
    /* CAMAC */
    fd_set[1].fd = fd;
    fd_set[1].events = POLLIN;

    for (i = 0; i < N_REPEATS; i++) {
	ioctl(fd, CAMDRV_IOC_ENABLE_INTERRUPT);

	if (poll(fd_set, 2, TIMEOUT_SEC * 1000) < 0) {
	    perror("ERROR: poll()");
	    exit(EXIT_FAILURE);
	}
	printf(".");
	fflush(stdout);

	if (fd_set[0].revents & POLLIN) {
	    printf("stdin: ");
	    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
                printf("%s", buffer);
            }
            else {
                printf("EOF");
            }
	}
	if (fd_set[1].revents & POLLIN) {
	    printf("CAMAC: ");
    
	    ioctl_data[0] = 100; /* timeout */
	    ioctl_data[1] = 0;
	    if (ioctl(fd, CAMDRV_IOC_WAIT_LAM, ioctl_data) > 0) {
		printf("interrupt handled.\n");
		ioctl(fd, CAMDRV_IOC_CLEAR);
	    }
	    else {
		perror("ERROR: ioctl(WAIT_LAM)");
	    }
	}
    }

    close(fd);

    return 0;
}
