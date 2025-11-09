
import os, fcntl, struct, errno

DEVICE_FILE = "/dev/camdrv"

_IOC_NONE = 0
_IOC_READ = 2
_IOC_WRITE = 1

_IOC_DIRSHIFT = 30
_IOC_TYPESHIFT = 8
_IOC_NRSHIFT = 0
_IOC_SIZESHIFT = 16

def _IOC(dir, type, nr, size):
    return ((dir << _IOC_DIRSHIFT) | (type << _IOC_TYPESHIFT) | (nr << _IOC_NRSHIFT) | (size << _IOC_SIZESHIFT))

def _IO(type, nr):
    return _IOC(_IOC_NONE, type, nr, 0)

def _IOR(type, nr, size):
    return _IOC(_IOC_READ, type, nr, size)

def _IOW(type, nr, size):
    return _IOC(_IOC_WRITE, type, nr, size)

def _IOWR(type, nr, size):
    return _IOC(_IOC_READ | _IOC_WRITE, type, nr, size)


_IOC_SIZE_UINT2 = 8

CAMDRV_IOC_MAGIC = 0xCC
CAMDRV_IOC_INITIALIZE = _IO(CAMDRV_IOC_MAGIC, 1)
CAMDRV_IOC_CLEAR = _IO(CAMDRV_IOC_MAGIC, 2)
CAMDRV_IOC_INHIBIT = _IO(CAMDRV_IOC_MAGIC, 3)
CAMDRV_IOC_RELEASE_INHIBIT = _IO(CAMDRV_IOC_MAGIC, 4)
CAMDRV_IOC_ENABLE_INTERRUPT = _IO(CAMDRV_IOC_MAGIC, 5)
CAMDRV_IOC_DISABLE_INTERRUPT = _IO(CAMDRV_IOC_MAGIC, 6)
CAMDRV_IOC_CAMAC_ACTION = _IOWR(CAMDRV_IOC_MAGIC, 7, _IOC_SIZE_UINT2)
CAMDRV_IOC_READ_LAM = _IOR(CAMDRV_IOC_MAGIC, 8, _IOC_SIZE_UINT2)
CAMDRV_IOC_WAIT_LAM = _IOWR(CAMDRV_IOC_MAGIC, 9, _IOC_SIZE_UINT2)
CAMDRV_IOC_SET_CRATE = _IOW(CAMDRV_IOC_MAGIC, 10, _IOC_SIZE_UINT2)


_device_descriptor = None


def COPEN():
    global _device_descriptor
    try:
        _device_descriptor = os.open(DEVICE_FILE, os.O_RDWR)
        return 0
    except OSError as e:
        return e.errno


def CCLOSE():
    global _device_descriptor
    if _device_descriptor is not None:
        try:
            os.close(_device_descriptor)
            _device_descriptor = None
            return 0
        except OSError as e:
            return e.errno
    return 0


def CSETCR(crate_number):
    if _device_descriptor is None:
        return errno.EBADF
    
    try:
        # unsigned[2]の配列をパック（リトルエンディアン）
        ioctl_data = struct.pack('<II', crate_number, 0)
        fcntl.ioctl(_device_descriptor, CAMDRV_IOC_SET_CRATE, ioctl_data)
        return 0
    except OSError as e:
        return e.errno


def CGENZ():
    if _device_descriptor is None:
        return errno.EBADF
    
    try:
        fcntl.ioctl(_device_descriptor, CAMDRV_IOC_INITIALIZE)
        return 0
    except OSError as e:
        return e.errno


def CGENC():
    if _device_descriptor is None:
        return errno.EBADF
    
    try:
        fcntl.ioctl(_device_descriptor, CAMDRV_IOC_CLEAR)
        return 0
    except OSError as e:
        return e.errno


def CAMAC(n, a, f, data):
    """
    execute a CAMAC action
    Args:
        n, a, f: number, address, function
        data: data value
    Returns:
        (q, x, data, errno)
    """
    if _device_descriptor is None:
        return (errno.EBADF, data, 0, 0)
    
    try:
        # unsigned[2]の配列をパック
        naf = ((n << 9) | (a << 5) | f) & 0x3fff
        ioctl_data = struct.pack('<II', naf, data & 0x00ffffff)
        result = fcntl.ioctl(_device_descriptor, CAMDRV_IOC_CAMAC_ACTION, ioctl_data)
        
        if result < 0:
            return (0, 0, 0, errno.EIO)
    except OSError as e:
        return (e.errno, data, 0, 0)
        
    q = 0 if (result & 0x0001) else 1
    x = 0 if (result & 0x0002) else 1
    _, data_out = struct.unpack('<II', ioctl_data)
    data_out = data_out & 0x00ffffff

    return (q, x, data_out, 0)


def CWLAM(timeout):
    """Wait for a LAM"""
    
    if _device_descriptor is None:
        return errno.EBADF
    
    try:
        ioctl_data = struct.pack('<II', timeout, 0)
        result = fcntl.ioctl(_device_descriptor, CAMDRV_IOC_WAIT_LAM, ioctl_data)
    except OSError as e:
        return e.errno

    return 0 if result > 0 else errno.ETIMEDOUT
