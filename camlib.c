/* camlib.c */
/* Created by Sanshiro Enomoto on 23 July 1999. */
/* Last updated by Sanshiro Enomoto on 18 September 2002. */


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "camdrv.h"
#include "camlib.h"

static const char *device_file = "/dev/camdrv";
static int device_descripter = 0;
static unsigned ioctl_data[2];


int COPEN(void)
{
    device_descripter = open(device_file, O_RDWR);

    return (device_descripter >= 0) ? 0 : errno;
}

int CCLOSE(void)
{
    close(device_descripter);

    return 0;
}

int CSETCR(int crate_number)
{
    int result;
    ioctl_data[0] = crate_number;
    result = ioctl(device_descripter, CAMDRV_IOC_SET_CRATE, ioctl_data);

    return (result >= 0) ? 0 : errno;
}

int CGENZ(void)
{
    int result; 
    result = ioctl(device_descripter, CAMDRV_IOC_INITIALIZE);

    return (result >= 0) ? 0 : errno;
}

int CGENC(void)
{
    int result; 
    result = ioctl(device_descripter, CAMDRV_IOC_CLEAR);

    return (result >= 0) ? 0 : errno;
}

int CSETI(void)
{
    int result; 
    result = ioctl(device_descripter, CAMDRV_IOC_INHIBIT);

    return (result >= 0) ? 0 : errno;
}

int CREMI(void)
{
    int result; 
    result = ioctl(device_descripter, CAMDRV_IOC_RELEASE_INHIBIT);

    return (result >= 0) ? 0 : errno;
}

int CAMAC(int naf, int *data, int *q, int *x)
{
    int result;

    ioctl_data[0] = naf;
    ioctl_data[1] = (unsigned) *data;
    result = ioctl(device_descripter, CAMDRV_IOC_CAMAC_ACTION, ioctl_data);

    if (result < 0) {
        return errno;
    }

    *data = ioctl_data[1] & 0x00ffffff;
    *q = ! (result & 0x0001);
    *x = ! (result & 0x0002);

    return 0;
}

int CELAM(int mask)
{
    int result; 
    result = ioctl(device_descripter, CAMDRV_IOC_ENABLE_INTERRUPT);

    return (result >= 0) ? 0 : errno;
}

int CDLAM(void)
{
    int result; 
    result = ioctl(device_descripter, CAMDRV_IOC_DISABLE_INTERRUPT);

    return (result >= 0) ? 0 : errno;
}

int CWLAM(int timeout)
{
    int result; 
    
    CELAM(~0);

    ioctl_data[0] = timeout;
    ioctl_data[1] = 0;
    result = ioctl(device_descripter, CAMDRV_IOC_WAIT_LAM, ioctl_data);

    return (result > 0) ? 0 : errno;
}
