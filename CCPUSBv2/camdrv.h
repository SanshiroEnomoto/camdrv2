/* camdrv.h */
/* Created by Sanshiro Enomoto on 11 April 1999. */


#ifndef __CAMDRV_H__
#define __CAMDRV_H__

#include <linux/ioctl.h>


#define CAMDRV_IOC_MAGIC 0xCC

#define CAMDRV_IOC_INITIALIZE         _IO(CAMDRV_IOC_MAGIC, 1)
#define CAMDRV_IOC_CLEAR              _IO(CAMDRV_IOC_MAGIC, 2)
#define CAMDRV_IOC_INHIBIT            _IO(CAMDRV_IOC_MAGIC, 3)
#define CAMDRV_IOC_RELEASE_INHIBIT    _IO(CAMDRV_IOC_MAGIC, 4)
#define CAMDRV_IOC_ENABLE_INTERRUPT   _IO(CAMDRV_IOC_MAGIC, 5)
#define CAMDRV_IOC_DISABLE_INTERRUPT  _IO(CAMDRV_IOC_MAGIC, 6)
#define CAMDRV_IOC_CAMAC_ACTION       _IOWR(CAMDRV_IOC_MAGIC, 7, unsigned[2])
#define CAMDRV_IOC_READ_LAM           _IOR(CAMDRV_IOC_MAGIC, 8, unsigned[2])
#define CAMDRV_IOC_WAIT_LAM           _IOWR(CAMDRV_IOC_MAGIC, 9, unsigned[2])
#define CAMDRV_IOC_SET_CRATE          _IOW(CAMDRV_IOC_MAGIC, 10, unsigned[2])


#endif
