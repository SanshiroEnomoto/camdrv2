/* toyocamac.c */
/* Created by Sanshiro Enomoto on 23 July 1999. */
/* Last updated by Sanshiro Enomoto on 18 September 2002. */


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include "camdrv.h"
#include "toyocamac.h"

static const char *device_file = "/dev/camdrv";
static int device_descripter = 0;
static unsigned ioctl_data[2];
static unsigned crate_number = 0;

#define NAF(n, a, f) ((((n) << 9) | ((a) << 5) | (f)) & 0x00003fff)
#define CHECK_OPENED ((device_descripter > 0) ? 1 : camdrv_open())


static int camdrv_open(void)
{
    device_descripter = open(device_file, O_RDWR);

    return device_descripter;
}

#if 0
static void camdrv_close(void)
{
    close(device_descripter);
}
#endif

void setcn(unsigned crate_number)
{
    CHECK_OPENED;

    ioctl_data[0] = crate_number;
    ioctl(device_descripter, CAMDRV_IOC_SET_CRATE, ioctl_data);
}

unsigned getcn(void)
{
    return crate_number;
}

void execz(void)
{
    CHECK_OPENED;
    ioctl(device_descripter, CAMDRV_IOC_INITIALIZE);
}

void execc(void)
{
    CHECK_OPENED;
    ioctl(device_descripter, CAMDRV_IOC_CLEAR);
}

void seti(void)
{
    CHECK_OPENED;
    ioctl(device_descripter, CAMDRV_IOC_INHIBIT);
}

void clri(void)
{
    CHECK_OPENED;
    ioctl(device_descripter, CAMDRV_IOC_RELEASE_INHIBIT);
}

void setei(void)
{
    CHECK_OPENED;
    ioctl(device_descripter, CAMDRV_IOC_ENABLE_INTERRUPT);
}

void clrei(void)
{
    CHECK_OPENED;
    ioctl(device_descripter, CAMDRV_IOC_DISABLE_INTERRUPT);
}

unsigned long rlam(void)
{
    CHECK_OPENED;

    ioctl_data[0] = 0;
    ioctl_data[1] = 0;
    ioctl(device_descripter, CAMDRV_IOC_READ_LAM, ioctl_data);

    return ioctl_data[1];
}

unsigned int camac_0(unsigned n, unsigned a, unsigned f)
{
    unsigned data;
    return camac_24(n, a, f, &data);
}

unsigned int camac_16(unsigned n, unsigned a, unsigned f, unsigned *data)
{
    return camac_24(n, a, f, data);
}

unsigned int camac_24(unsigned n, unsigned a, unsigned f, unsigned *data)
{
    unsigned nxq = 0;
    int result;

    CHECK_OPENED;

    ioctl_data[0] = NAF(n, a, f);
    ioctl_data[1] = *data;
    result = ioctl(device_descripter, CAMDRV_IOC_CAMAC_ACTION, ioctl_data);

    if (result < 0) {
        return ~0;
    }

    *data = ioctl_data[1] & 0x00ffffff;
    nxq = result & 0x0003;

    return nxq;
}

unsigned int camac_16w(unsigned n, unsigned a, unsigned f, unsigned data)
{
    return camac_24(n, a, f, &data);
}

unsigned int camac_24w(unsigned n, unsigned a, unsigned f, unsigned data)
{
    return camac_24(n, a, f, &data);
}
