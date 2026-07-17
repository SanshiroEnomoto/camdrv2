/* camdrv.c */
/* Created by Sanshiro Enomoto on 11 April 1999 */
/* Last updated by Sanshiro Enomoto on 1 December 2025 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include "camdrv.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sanshiro Enomoto");
MODULE_DESCRIPTION("CAMAC Driver for Hoshin CCP-USB(V2) Controller");
MODULE_VERSION("1.00");

#define DRIVER_NAME "camdrv"
#define DEVICE_NAME "camdrv"
#define CCP_VENDOR_ID 0x24b9
#define CCP_PRODUCT_ID 0x0020

#define BUFFER_SIZE 64
#define LATENCY_TIME 2
#define TIMEOUT_MS 500
#define USB_IN_TRANSFER_SIZE 512

// Debug support: define DEBUG to enable debug messages
//#define DEBUG
#ifdef DEBUG
#define dbg_print(fmt, ...) pr_info("camdrv: " fmt, ##__VA_ARGS__)
#define dbg_dev_print(dev, fmt, ...) dev_info(&(dev)->udev->dev, "camdrv: " fmt, ##__VA_ARGS__)
#else
#define dbg_print(fmt, ...) do {} while (0)
#define dbg_dev_print(dev, fmt, ...) do {} while (0)
#endif


static int major_number;
static struct class *camdrv_class = NULL;
static struct device *camdrv_device = NULL;
static dev_t dev_num;

// Device structure
struct camdrv_device {
    struct usb_device *udev;
    struct usb_interface *interface;
    struct cdev cdev;
    struct mutex mutex;
    struct usb_endpoint_descriptor *bulk_in;
    struct usb_endpoint_descriptor *bulk_out;
    unsigned char *tx_buffer;
    unsigned char *rx_buffer;
    int start_n;
    bool is_open;
    unsigned crate_number;
};

static struct usb_device_id camdrv_table[] = {
    { USB_DEVICE(CCP_VENDOR_ID, CCP_PRODUCT_ID) },
    { }
};
MODULE_DEVICE_TABLE(usb, camdrv_table);

static int camdrv_open(struct inode *inode, struct file *file);
static int camdrv_release(struct inode *inode, struct file *file);
static long camdrv_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int camdrv_probe(struct usb_interface *interface, const struct usb_device_id *id);
static void camdrv_disconnect(struct usb_interface *interface);

static int ftdi_init_sync_fifo(struct camdrv_device *dev);
static int ftdi_control_request(struct usb_device *udev, u8 request_type, u8 request, u16 value, u16 index, void *data, u16 size);

static int ccp_inout(struct camdrv_device *dev, unsigned int write_size, unsigned int read_size);
static int ccp_init(struct camdrv_device *dev, unsigned char crate_number);
static int ccp_initialize(struct camdrv_device *dev, unsigned crate_number);
static int ccp_clear(struct camdrv_device *dev, unsigned crate_number);
static int ccp_camac_action(struct camdrv_device *dev, unsigned crate, unsigned n, unsigned a, unsigned f, unsigned* data);
static int ccp_read_lam(struct camdrv_device *dev, unsigned char crate_number, unsigned *data);
static int ccp_wait_lam(struct camdrv_device *dev, unsigned char crate_number, unsigned timeout, unsigned* data);
//static int ccp_read_register(struct camdrv_device *dev, unsigned char crate_number, unsigned address, unsigned *data);
static int ccp_write_register(struct camdrv_device *dev, unsigned char crate_number, unsigned address, unsigned data);



static const struct file_operations camdrv_fops = {
    .owner = THIS_MODULE,
    .open = camdrv_open,
    .release = camdrv_release,
    .unlocked_ioctl = camdrv_ioctl,
};

static struct usb_driver camdrv_driver = {
    .name = DRIVER_NAME,
    .probe = camdrv_probe,
    .disconnect = camdrv_disconnect,
    .id_table = camdrv_table,
};


static int __init camdrv_init(void)
{
    int result;
    
    result = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (result < 0) {
        pr_err("Failed to allocate chrdev region\n");
        return result;
    }
    major_number = MAJOR(dev_num);
    
    camdrv_class = class_create(DEVICE_NAME);
    if (IS_ERR(camdrv_class)) {
        pr_err("Failed to create device class\n");
        result = PTR_ERR(camdrv_class);
        goto error_class;
    }
    
    camdrv_device = device_create(camdrv_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(camdrv_device)) {
        pr_err("Failed to create device\n");
        result = PTR_ERR(camdrv_device);
        goto error_device;
    }
    
    result = usb_register(&camdrv_driver);
    if (result) {
        pr_err("Failed to register USB driver\n");
        goto error_usb;
    }
    
    pr_info("CCP-USB(V2) kernel driver loaded\n");
    return 0;
    
  error_usb:
    device_destroy(camdrv_class, dev_num);
  error_device:
    class_destroy(camdrv_class);
  error_class:
    unregister_chrdev_region(dev_num, 1);
        
    return result;
}
module_init(camdrv_init);


static void __exit camdrv_exit(void)
{
    usb_deregister(&camdrv_driver);
    device_destroy(camdrv_class, dev_num);
    class_destroy(camdrv_class);
    unregister_chrdev_region(dev_num, 1);
    pr_info("CCP-USB(V2) kernel driver unloaded\n");
}
module_exit(camdrv_exit);


static int camdrv_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    struct camdrv_device *dev;
    int result = -ENOMEM;
    int i;
    
    dbg_print("camdrv_probe: device detected (vendor=0x%04x, product=0x%04x)\n",
             le16_to_cpu(udev->descriptor.idVendor),
             le16_to_cpu(udev->descriptor.idProduct));
    
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) {
        dev_err(&interface->dev, "camdrv_probe: failed to allocate device structure\n");
        return -ENOMEM;
    }
    
    dev->udev = usb_get_dev(udev);
    dev->interface = interface;
    
    // Find bulk endpoints
    iface_desc = interface->cur_altsetting;
    dbg_print("camdrv_probe: searching endpoints (num_endpoints=%u)\n",
             iface_desc->desc.bNumEndpoints);
    for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
        endpoint = &iface_desc->endpoint[i].desc;
        dbg_print("camdrv_probe: endpoint[%d]: addr=0x%02x, type=%u, dir=%s\n",
                 i, endpoint->bEndpointAddress,
                 usb_endpoint_type(endpoint),
                 usb_endpoint_dir_in(endpoint) ? "IN" : "OUT");
        if (usb_endpoint_is_bulk_in(endpoint)) {
            if (!dev->bulk_in) {
                dev->bulk_in = endpoint;
                dbg_print("camdrv_probe: found bulk IN endpoint: 0x%02x\n",
                         endpoint->bEndpointAddress);
            }
        } else if (usb_endpoint_is_bulk_out(endpoint)) {
            if (!dev->bulk_out) {
                dev->bulk_out = endpoint;
                dbg_print("camdrv_probe: found bulk OUT endpoint: 0x%02x\n",
                         endpoint->bEndpointAddress);
            }
        }
    }
    
    if (!dev->bulk_in || !dev->bulk_out) {
        dev_err(&interface->dev, "Could not find bulk endpoints (in=%p, out=%p)\n",
               dev->bulk_in, dev->bulk_out);
        result = -ENODEV;
        goto error;
    }
    
    dev->tx_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    dev->rx_buffer = kmalloc(USB_IN_TRANSFER_SIZE, GFP_KERNEL);
    if (!dev->tx_buffer || !dev->rx_buffer) {
        dev_err(&interface->dev, "camdrv_probe: failed to allocate buffers\n");
        result = -ENOMEM;
        goto error;
    }
    dbg_print("camdrv_probe: allocated buffers (tx=%p, rx=%p)\n",
             dev->tx_buffer, dev->rx_buffer);
    
    mutex_init(&dev->mutex);
    dev->is_open = false;
    dev->crate_number = 1;
    
    cdev_init(&dev->cdev, &camdrv_fops);
    dev->cdev.owner = THIS_MODULE;
    result = cdev_add(&dev->cdev, dev_num, 1);
    if (result) {
        dev_err(&interface->dev, "Error %d adding cdev\n", result);
        goto error;
    }
    dbg_print("camdrv_probe: cdev added successfully\n");
    
    usb_set_intfdata(interface, dev);
    
    dev_info(&interface->dev, "CCP-USB(V2) device attached\n");
    dbg_print("camdrv_probe: probe completed successfully\n");
    
    return 0;
    
  error:
    if (dev->tx_buffer) {
        kfree(dev->tx_buffer);
    }
    if (dev->rx_buffer) {
        kfree(dev->rx_buffer);
    }
    usb_put_dev(dev->udev);
    kfree(dev);
    
    dbg_print("camdrv_probe: probe failed with error %d\n", result);
    return result;
}


static void camdrv_disconnect(struct usb_interface *interface)
{
    struct camdrv_device *dev = usb_get_intfdata(interface);

    usb_set_intfdata(interface, NULL);
    
    if (dev) {
        cdev_del(&dev->cdev);
        mutex_lock(&dev->mutex);
        dev->is_open = false;
        mutex_unlock(&dev->mutex);
        kfree(dev->tx_buffer);
        kfree(dev->rx_buffer);
        usb_put_dev(dev->udev);
        kfree(dev);
        dev_info(&interface->dev, "CCP-USB(V2) device disconnected\n");
    }
}


static int camdrv_open(struct inode *inode, struct file *file)
{
    struct camdrv_device *dev;
    int result = 0;
    
    dbg_print("camdrv_open: called\n");
    
    dev = container_of(inode->i_cdev, struct camdrv_device, cdev);
    file->private_data = dev;
    
    dbg_dev_print(dev, "camdrv_open: device found, crate_number=%u\n", dev->crate_number);
    
    if (mutex_lock_interruptible(&dev->mutex)) {
        dbg_dev_print(dev, "camdrv_open: mutex lock interrupted\n");
        return -ERESTARTSYS;
    }
    
    if (dev->is_open) {
        dbg_dev_print(dev, "camdrv_open: device already open\n");
        result = -EBUSY;
        goto err_unlock;
    }
    
    dbg_dev_print(dev, "camdrv_open: initializing FTDI device\n");
    result = ftdi_init_sync_fifo(dev);
    if (result < 0) {
        dev_err(&dev->udev->dev, "Failed to initialize FTDI device: %d\n", result);
        goto err_unlock;
    }
    dbg_dev_print(dev, "camdrv_open: FTDI device initialized successfully\n");
    
    dbg_dev_print(dev, "camdrv_open: initializing CCP interface, crate=%u\n", dev->crate_number);
    result = ccp_init(dev, dev->crate_number);
    if (result < 0) {
        dev_err(&dev->udev->dev, "Failed to initialize CCP interface: %d\n", result);
        goto err_unlock;
    }
    dbg_dev_print(dev, "camdrv_open: CCP interface initialized, result=0x%02x\n", result);
    pr_info("CCP-USB(V2) opened\n");
    
    dev->is_open = true;
    mutex_unlock(&dev->mutex);
    
    dbg_dev_print(dev, "camdrv_open: successfully opened\n");
    return 0;

  err_unlock:
    mutex_unlock(&dev->mutex);
    dbg_dev_print(dev, "camdrv_open: failed with error %d\n", result);
    return result;
}


static int camdrv_release(struct inode *inode, struct file *file)
{
    struct camdrv_device *dev = file->private_data;

    if (dev) {
        mutex_lock(&dev->mutex);
        dev->is_open = false;
        mutex_unlock(&dev->mutex);
    }

    return 0;
}


static long camdrv_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned parameter = 0, data = 0;
    unsigned *user_parameter_ptr, *user_data_ptr;
    unsigned crate_number, n, a, f;
    int result = 0;

    struct camdrv_device *dev = file->private_data;
    crate_number = dev->crate_number;
    
    dbg_dev_print(dev, "camdrv_ioctl: cmd=0x%08x, arg=0x%08lx\n", cmd, arg);
    
    user_parameter_ptr = (unsigned *) arg;
    user_data_ptr = (unsigned *) arg + 1;

    if (_IOC_TYPE(cmd) != CAMDRV_IOC_MAGIC) {
        dbg_dev_print(dev, "camdrv_ioctl: invalid IOC magic (got 0x%02x, expected 0x%02x)\n", 
                     _IOC_TYPE(cmd), CAMDRV_IOC_MAGIC);
        return -EINVAL;
    }
    if (_IOC_DIR(cmd) & (_IOC_READ | _IOC_WRITE)) {
        if (get_user(parameter, user_parameter_ptr) < 0) {
            dbg_dev_print(dev, "camdrv_ioctl: failed to get parameter\n");
            return -EFAULT;
        }
        if (get_user(data, user_data_ptr) < 0) {
            dbg_dev_print(dev, "camdrv_ioctl: failed to get data\n");
            return -EFAULT;
        }
        dbg_dev_print(dev, "camdrv_ioctl: parameter=0x%08x, data=0x%08x\n", parameter, data);
    }

    if (mutex_lock_interruptible(&dev->mutex)) {
        dbg_dev_print(dev, "camdrv_ioctl: mutex lock interrupted\n");
        return -ERESTARTSYS;
    }
    
    switch (cmd) {
      case CAMDRV_IOC_INITIALIZE:
        dbg_dev_print(dev, "camdrv_ioctl: INITIALIZE, crate=%u\n", crate_number);
        result = ccp_initialize(dev, crate_number);
        break;
      case CAMDRV_IOC_CLEAR:
        dbg_dev_print(dev, "camdrv_ioctl: CLEAR, crate=%u\n", crate_number);
        result = ccp_clear(dev, crate_number);
        break;
      case CAMDRV_IOC_INHIBIT:
        dbg_dev_print(dev, "camdrv_ioctl: INHIBIT (not supported)\n");
        result = -EINVAL;
        break;
      case CAMDRV_IOC_RELEASE_INHIBIT:
        dbg_dev_print(dev, "camdrv_ioctl: RELEASE_INHIBIT (not supported)\n");
        result = -EINVAL;
        break;
      case CAMDRV_IOC_ENABLE_INTERRUPT:
        dbg_dev_print(dev, "camdrv_ioctl: ENABLE_INTERRUPT (not supported)\n");
        result = -EINVAL;
        break;
      case CAMDRV_IOC_DISABLE_INTERRUPT:
        dbg_dev_print(dev, "camdrv_ioctl: DISABLE_INTERRUPT (not supported)\n");
        result = -EINVAL;
        break;
      case CAMDRV_IOC_CAMAC_ACTION:
        n = (parameter >> 9) & 0x1f;
        a = (parameter >> 5) & 0x0f;
        f = (parameter >> 0) & 0x1f;
        dbg_dev_print(
            dev,
            "camdrv_ioctl: CAMAC_ACTION, crate=%u, n=%u, a=%u, f=%u, data=0x%08x\n",
            crate_number, n, a, f, data
        );
        result = ccp_camac_action(dev, crate_number, n, a, f, &data);
        dbg_dev_print(dev, "camdrv_ioctl: CAMAC_ACTION result=%d, data=0x%08x\n", result, data);
        put_user(data, user_data_ptr);
        break;
      case CAMDRV_IOC_READ_LAM:
        dbg_dev_print(dev, "camdrv_ioctl: READ_LAM, crate=%u\n", crate_number);
        result = ccp_read_lam(dev, crate_number, &data);
        dbg_dev_print(dev, "camdrv_ioctl: READ_LAM result=%d, data=0x%08x\n", result, data);
        put_user(data, user_data_ptr);
        break;
      case CAMDRV_IOC_WAIT_LAM:
        dbg_dev_print(dev, "camdrv_ioctl: WAIT_LAM, crate=%u, timeout=%u\n", crate_number, parameter);
        result = ccp_wait_lam(dev, crate_number, parameter, &data);
        dbg_dev_print(dev, "camdrv_ioctl: WAIT_LAM result=%d, data=0x%08x\n", result, data);
        put_user(data, user_data_ptr);
        break;
      case CAMDRV_IOC_SET_CRATE:
        dbg_dev_print(dev, "camdrv_ioctl: SET_CRATE, old=%u, new=%u\n", crate_number, parameter + 1);
        dev->crate_number = parameter;
        result = ccp_init(dev, dev->crate_number);
        dbg_dev_print(dev, "camdrv_ioctl: SET_CRATE result=%d\n", result);
        break;
      default:
        dbg_dev_print(dev, "camdrv_ioctl: unknown command 0x%08x\n", cmd);
        result = -EINVAL;
    }

    mutex_unlock(&dev->mutex);
    
    dbg_dev_print(dev, "camdrv_ioctl: returning %d\n", result);
    return result;
}


//// FTDI ////

#define FTDI_SIO_RESET_REQUEST_TYPE 0x40
#define FTDI_SIO_RESET_REQUEST 0x00
#define FTDI_SIO_RESET_SIO 0
#define FTDI_SIO_FLUSH_HOST_OUT 1
#define FTDI_SIO_FLUSH_HOST_IN 2

#define FTDI_SIO_SET_BITMODE_REQUEST_TYPE 0x40
#define FTDI_SIO_SET_BITMODE_REQUEST 0x0b
#define FTDI_BITMODE_RESET 0x00
#define FTDI_BITMODE_SYNC_FIFO 0x40

#define FTDI_SIO_SET_LATENCY_TIMER_REQUEST_TYPE 0x40
#define FTDI_SIO_SET_LATENCY_TIMER_REQUEST 0x09

#define FTDI_SIO_SET_EVENT_CHAR_REQUEST_TYPE 0x40
#define FTDI_SIO_SET_EVENT_CHAR_REQUEST 0x06

#define FTDI_INTERFACE_A 1


// Helper function to send FTDI control request
static int ftdi_control_request(struct usb_device *udev, u8 request_type, u8 request, u16 value, u16 index, void *data, u16 size)
{
    int result;
    result = usb_control_msg(
        udev, usb_sndctrlpipe(udev, 0),
        request, request_type, value, index,
        data, size, TIMEOUT_MS
    );
    if (result < 0) {
        dev_err(&udev->dev, "FTDI control request failed: %d\n", result);
    }
    
    return result;
}


// Initialize FTDI device for synchronous FIFO mode
static int ftdi_init_sync_fifo(struct camdrv_device *dev)
{
    struct usb_device *udev = dev->udev;
    int result;

    dbg_dev_print(dev, "ftdi_init_sync_fifo: starting initialization\n");

    // Reset device
    dbg_dev_print(dev, "ftdi_init_sync_fifo: resetting device\n");
    result = ftdi_control_request(
        udev, FTDI_SIO_RESET_REQUEST_TYPE,
        FTDI_SIO_RESET_REQUEST,
        FTDI_SIO_RESET_SIO, FTDI_INTERFACE_A,
        NULL, 0
    );
    if (result < 0) {
        dev_err(&udev->dev, "ftdi_init_sync_fifo: reset failed: %d\n", result);
        return result;
    }
    dbg_dev_print(dev, "ftdi_init_sync_fifo: reset successful\n");

    // Reset bit mode
    dbg_dev_print(dev, "ftdi_init_sync_fifo: resetting bit mode\n");
    result = ftdi_control_request(
        udev, FTDI_SIO_SET_BITMODE_REQUEST_TYPE,
        FTDI_SIO_SET_BITMODE_REQUEST,
        FTDI_BITMODE_RESET, FTDI_INTERFACE_A,
        NULL, 0
    );
    if (result < 0) {
        dev_err(&udev->dev, "ftdi_init_sync_fifo: bit mode reset failed: %d\n", result);
        return result;
    }
    dbg_dev_print(dev, "ftdi_init_sync_fifo: bit mode reset successful\n");

    // Set synchronous FIFO mode (0xF0 = pin direction, 0x40 = sync FIFO mode)
    dbg_dev_print(dev, "ftdi_init_sync_fifo: setting sync FIFO mode (value=0x%04x)\n", (FTDI_BITMODE_SYNC_FIFO << 8) | 0xF0);
    result = ftdi_control_request(
        udev, FTDI_SIO_SET_BITMODE_REQUEST_TYPE,
        FTDI_SIO_SET_BITMODE_REQUEST,
        (FTDI_BITMODE_SYNC_FIFO << 8) | 0xF0,
        FTDI_INTERFACE_A, NULL, 0
    );
    if (result < 0) {
        dev_err(&udev->dev, "ftdi_init_sync_fifo: set sync FIFO mode failed: %d\n", result);
        return result;
    }
    dbg_dev_print(dev, "ftdi_init_sync_fifo: sync FIFO mode set successfully\n");

    // Set latency timer
    dbg_dev_print(dev, "ftdi_init_sync_fifo: setting latency timer to %d\n", LATENCY_TIME);
    result = ftdi_control_request(
        udev, FTDI_SIO_SET_LATENCY_TIMER_REQUEST_TYPE,
        FTDI_SIO_SET_LATENCY_TIMER_REQUEST,
        LATENCY_TIME, FTDI_INTERFACE_A,
        NULL, 0
    );
    if (result < 0) {
        dev_err(&udev->dev, "ftdi_init_sync_fifo: set latency timer failed: %d\n", result);
        return result;
    }
    dbg_dev_print(dev, "ftdi_init_sync_fifo: latency timer set successfully\n");

    dbg_dev_print(dev, "ftdi_init_sync_fifo: initialization completed successfully\n");
    return 0;
}


//// CCP ////

enum ccp_command {
    cmdINITIALIZE_CCP = 0x49,
    cmdCAMAC = 0x43,
    cmdLAM = 0x4c,
    cmdWRITE_REG = 0x57,
    cmdREAD_REG = 0x52
};

enum ccp_ctrlbits {
    ctrlINITIALIZE = 0x40,
    ctrlCLEAR = 0x80
};

enum ccp_statbits {
    statQ = 0x01,
    statX = 0x02,
    statI = 0x04,
    statLE = 0x08,
    statREQ = 0x40,
    statDEM = 0x80
};

static int ccp_inout(struct camdrv_device *dev, unsigned int write_size, unsigned int read_size)
{
    struct usb_device *udev = dev->udev;
    int result;
    int actual_length, start_n;
    unsigned int i;
    
    dbg_dev_print(dev, "ccp_inout: write_size=%u, read_size=%u\n", write_size, read_size);
    
    if (!dev->bulk_in || !dev->bulk_out) {
        dev_err(&udev->dev, "ccp_inout: bulk endpoints not found\n");
        return -ENODEV;
    }

    // Print TX buffer
    dbg_dev_print(dev, "ccp_inout: TX buffer (%u bytes): ", write_size);
    for (i = 0; i < write_size; i++) {
        dbg_print("TX %d: %02x ", i, dev->tx_buffer[i]);
    }
    dbg_print("==(end TX)==\n");

    // Purge RX buffer
    dbg_dev_print(dev, "ccp_inout: purging RX buffer\n");
    ftdi_control_request(
        udev, FTDI_SIO_RESET_REQUEST_TYPE,
        FTDI_SIO_RESET_REQUEST,
        FTDI_SIO_FLUSH_HOST_IN, FTDI_INTERFACE_A,
        NULL, 0
    );
    //udelay(10);

    // Write data
    dbg_dev_print(
        dev, "ccp_inout: writing %u bytes to endpoint 0x%02x\n",
        write_size, dev->bulk_out->bEndpointAddress
    );
    result = usb_bulk_msg(
        udev, usb_sndbulkpipe(udev, dev->bulk_out->bEndpointAddress),
        dev->tx_buffer, write_size, &actual_length,
        TIMEOUT_MS
    );
    if (result < 0) {
        dev_err(&udev->dev, "ccp_inout: Write failed: %d\n", result);
        return -EIO;
    }
    dbg_dev_print(dev, "ccp_inout: wrote %d bytes (expected %u)\n", actual_length, write_size);
    //udelay(10);

    if (read_size == 0) {
        return 0;
    }
    
    // Read data
    dbg_dev_print(
        dev, "ccp_inout: reading from endpoint 0x%02x (max %u bytes)\n",
        dev->bulk_in->bEndpointAddress, USB_IN_TRANSFER_SIZE
    );
    result = usb_bulk_msg(
        udev, usb_rcvbulkpipe(udev, dev->bulk_in->bEndpointAddress),
        dev->rx_buffer, USB_IN_TRANSFER_SIZE, &actual_length,
        TIMEOUT_MS
    );
    if (result < 0) {
        dev_err(&udev->dev, "ccp_inout: Read failed: %d\n", result);
        return -EIO;
    }
    dbg_dev_print(dev, "ccp_inout: read %d bytes (expected at least %u)\n", actual_length, read_size);

    if (actual_length < read_size) {
        dev_err(
            &udev->dev, "ccp_inout: Read size mismatch: expected %u, got %d\n",
            read_size, actual_length
        );
        return -EIO;
    }

    // Find start marker (0x43)
    dbg_dev_print(dev, "ccp_inout: searching for start marker 0x43\n");
    start_n = -1;
    for (i = 0; i <= actual_length - read_size; i++) {
        unsigned char byte = ((dev->rx_buffer[i+1] & 0x0f) << 4) | (dev->rx_buffer[i] & 0x0f);
        if (byte == 0x43) {
            start_n = i + 2;
            break;
        }
    }
    
    // DEBUG: Print RX buffer
    dbg_dev_print(
        dev, "ccp_inout: RX buffer (first %u bytes out of %u received): ", 
        actual_length < 32 ? actual_length : 32,
        actual_length
    );
    for (i = 0; i < actual_length && i < 32; i++) {
        if (i >= start_n && i < start_n + read_size) {
            dbg_print("RX %03d: %02x ", i, dev->rx_buffer[i]);
        }
        else {
            dbg_print("RX %03d: (%02x) ", i, dev->rx_buffer[i]);
        }
    }
    dbg_print("==(end RX)==\n");

    if (start_n < 0) {
        dev_err(&udev->dev, "ccp_inout: start marker 0x43 not found in response\n");
        return -EIO;
    }
    dbg_dev_print(dev, "ccp_inout: found start marker at position %u\n", start_n);
    dev->start_n = start_n;

    return 0;
}


static int ccp_init(struct camdrv_device *dev, unsigned char crate_number)
{
    unsigned char cmd = cmdINITIALIZE_CCP;
    int result;

    dbg_dev_print(dev, "ccp_init: initializing crate %u\n", crate_number);

#if 0
    if (crate_number < 1 || crate_number > 7) {
        dev_err(&dev->udev->dev, "ccp_init: invalid crate number %u (must be 1-7)\n", crate_number);
#else
    if (crate_number > 7) {
        dev_err(&dev->udev->dev, "ccp_init: invalid crate number %u (must be 0-7)\n", crate_number);
#endif
        return -EINVAL;
    }

    // Reset device
    dbg_dev_print(dev, "ccp_init: resetting device\n");
    result = ftdi_control_request(
        dev->udev, FTDI_SIO_RESET_REQUEST_TYPE,
        FTDI_SIO_RESET_REQUEST,
        FTDI_SIO_RESET_SIO, FTDI_INTERFACE_A,
        NULL, 0
    );
    if (result < 0) {
        dev_err(&dev->udev->dev, "ccp_init: device reset failed: %d\n", result);
        return result;
    }

    // Prepare command
    dev->tx_buffer[0] = (cmd << 4);
    dev->tx_buffer[1] = (cmd & 0xF0);
    dev->tx_buffer[2] = (crate_number << 4);
    dev->tx_buffer[3] = (crate_number & 0xF0);
    dbg_dev_print(
        dev, "ccp_init: prepared command: 0x%02x 0x%02x 0x%02x 0x%02x\n",
        dev->tx_buffer[0], dev->tx_buffer[1], dev->tx_buffer[2], dev->tx_buffer[3]
    );
    
    // Send and receive
    result = ccp_inout(dev, 4, 4);
    if (result < 0) {
        dev_err(&dev->udev->dev, "ccp_init: ccp_inout failed: %d\n", result);
        return result;
    }

    // Extract result
    result = (
        ((dev->rx_buffer[dev->start_n + 1] & 0x0F) << 4) | (dev->rx_buffer[dev->start_n] & 0x0F)
    );
    dbg_dev_print(dev, "ccp_init: received result: 0x%02x\n", result);

    return result;
}


static int ccp_initialize(struct camdrv_device *dev, unsigned crate_number)
{
     return ccp_write_register(dev, crate_number, 5, ctrlINITIALIZE);
}


static int ccp_clear(struct camdrv_device *dev, unsigned crate_number)
{
     return ccp_write_register(dev, crate_number, 5, ctrlCLEAR);
}


static int ccp_camac_action(struct camdrv_device *dev, unsigned crate_number, unsigned n, unsigned a, unsigned f, unsigned* data)
{
    unsigned char cmd = cmdCAMAC;
    unsigned dl = 0, dm = 0, dh = 0;
    unsigned int read_size;
    unsigned status, nq, nx;
    int result;
    
    dbg_dev_print(dev, "ccp_camac_action: crate=%u, n=%u, a=%u, f=%u, &data=%p\n", crate_number, n, a, f, data);
#if 0    
    if (crate_number < 1  || crate_number > 7) {
#else
    if (crate_number > 7) {
#endif
        dev_err(&dev->udev->dev, "ccp_camac_action: invalid crate number %u\n", crate_number);
        return -EINVAL;
    }
    if (n == 0 || n >= 24 || a >= 16 || f >= 32) {
        dev_err(&dev->udev->dev, "ccp_camac_action: invalid parameters n=%u, a=%u, f=%u\n", n, a, f);
        return -EINVAL;
    }
    if (f > 15) {
        read_size = 4;
    }
    else {
        read_size = 10;
    }
    dbg_dev_print(dev, "ccp_camac_action: read_size=%u\n", read_size);
    
    if (data) {
        dl = (*data & 0x0000ff);
        dm = (*data & 0x00ff00) >> 8;
        dh = (*data & 0xff0000) >> 16;
        dbg_dev_print(dev, "ccp_camac_action: input data=0x%08x (dl=0x%02x, dm=0x%02x, dh=0x%02x)\n",
                     *data, dl, dm, dh);
        *data = 0;
    }
    
    dev->tx_buffer[0] = (cmd << 4);
    dev->tx_buffer[1] = (cmd & 0xF0);
    dev->tx_buffer[2] = (crate_number << 4);
    dev->tx_buffer[3] = (crate_number & 0xF0);
    dev->tx_buffer[4] = (n << 4);
    dev->tx_buffer[5] = (n & 0xF0);
    dev->tx_buffer[6] = (a << 4);
    dev->tx_buffer[7] = (a & 0xF0);
    dev->tx_buffer[8] = (f << 4);
    dev->tx_buffer[9] = (f & 0xF0);
    dev->tx_buffer[10] = (dl << 4);
    dev->tx_buffer[11] = (dl & 0xF0);
    dev->tx_buffer[12] = (dm << 4);
    dev->tx_buffer[13] = (dm & 0xF0);
    dev->tx_buffer[14] = (dh << 4);
    dev->tx_buffer[15] = (dh & 0xF0);
    
    result = ccp_inout(dev, 16, read_size);
    if (result < 0) {
        dev_err(&dev->udev->dev, "ccp_camac_action: ccp_inout failed: %d\n", result);
#if 0
        nq = 1;
        nx = 1;
        return (nx << 1) | nq;
#else
        return result;
#endif
    }
    
    if ((f <= 15) && data) {
        *data = (
            ((unsigned int)(dev->rx_buffer[dev->start_n + 7] & 0x0F) << 20) |
            ((unsigned int)(dev->rx_buffer[dev->start_n + 6] & 0x0F) << 16) |
            ((unsigned int)(dev->rx_buffer[dev->start_n + 5] & 0x0F) << 12) |
            ((unsigned int)(dev->rx_buffer[dev->start_n + 4] & 0x0F) << 8) |
            ((unsigned int)(dev->rx_buffer[dev->start_n + 3] & 0x0F) << 4) |
            ((unsigned int)(dev->rx_buffer[dev->start_n + 2] & 0x0F))
        );
        dbg_dev_print(dev, "ccp_camac_action: extracted data=0x%08x\n", *data);
    }
    status = (
        ((unsigned int)(dev->rx_buffer[dev->start_n + 1] & 0x0F) << 4) |
        ((unsigned int)(dev->rx_buffer[dev->start_n] & 0x0F))
    );
    nq = (status & statQ) ? 0x00 : 0x01;
    nx = (status & statX) ? 0x00 : 0x01;

    dbg_dev_print(
        dev, "ccp_camac_action: status=0x%02x, NQ=%u, NX=%u, result=%d\n",
        status, nq, nx, (nx << 1) | nq
    );

    return (nx << 1) | nq;
}


static int ccp_read_lam(struct camdrv_device *dev, unsigned char crate_number, unsigned *data)
{
    unsigned char cmd = cmdLAM;
    unsigned reply, encoded_lam = 0;
    int result;
    
    dbg_dev_print(dev, "ccp_read_lam: crate=%u\n", crate_number);
#if 0    
    if (crate_number < 1 || crate_number > 7) {
#else
    if (crate_number > 7) {
#endif
        dev_err(&dev->udev->dev, "ccp_read_lam: invalid crate number %u\n", crate_number);
        return -EINVAL;
    }

    dev->tx_buffer[0] = (cmd << 4);
    dev->tx_buffer[1] = (cmd & 0xF0);
    dev->tx_buffer[2] = (crate_number << 4);
    dev->tx_buffer[3] = (crate_number & 0xF0);
    *data = 0;
    
    result = ccp_inout(dev, 4, 6);
    if (result < 0) {
        dev_err(&dev->udev->dev, "ccp_read_lam: ccp_inout failed: %d\n", result);
        return result;
    }
                
    reply = (
        ((unsigned short)(dev->rx_buffer[dev->start_n + 3] & 0x0F) << 12) |
        ((unsigned short)(dev->rx_buffer[dev->start_n + 2] & 0x0F) << 8) |
        ((unsigned short)(dev->rx_buffer[dev->start_n + 1] & 0x0F) << 4) |
        ((unsigned short)(dev->rx_buffer[dev->start_n] & 0x0F))
    );
    dbg_dev_print(dev, "ccp_read_lam: reply=0x%04x\n", reply);

    encoded_lam = (reply & 0xff00) >> 8;
    if (encoded_lam > 0) {
        *data = 0x0001 << (encoded_lam - 1);
    }
    dbg_dev_print(dev, "ccp_read_lam: encoded_lam=0x%x, status=0x%x\n", encoded_lam, (reply & 0xff));

    return 0;
}


static int ccp_wait_lam(struct camdrv_device *dev, unsigned char crate_number, unsigned timeout, unsigned* data)
{
    unsigned long timeout_jiffies = jiffies + timeout * HZ;
    int result;

    /* The hardware does not support "interrupt on LAM". */
    /* The following code is a "polling loop" to wait for any LAM bits. */
    while (true) {
        result = ccp_read_lam(dev, crate_number, data);
        if (result < 0) {
            return result;
        }
        if (*data != 0) {
            return *data;
        }

        if (time_after_eq(jiffies, timeout_jiffies)) {
            *data = 0;
            return -ETIMEDOUT;
        }
        
        usleep_range(2000, 5000);
    }

    return *data;
}


static int ccp_write_register(struct camdrv_device *dev, unsigned char crate_number, unsigned address, unsigned data)
{
    unsigned char cmd = cmdWRITE_REG;
    int result;

#if 0    
    if (crate_number < 1 || crate_number > 7) {
#else
    if (crate_number > 7) {
#endif
        return -EINVAL;
    }

    dev->tx_buffer[0] = (cmd << 4);
    dev->tx_buffer[1] = (cmd & 0xF0);
    dev->tx_buffer[2] = (address << 4);
    dev->tx_buffer[3] = (address & 0xF0);
    dev->tx_buffer[4] = ((data & 0x0f) << 4);
    dev->tx_buffer[5] = (data & 0xf0);

    result = ccp_inout(dev, 6, 0);
    if (result < 0) {
        return result;
    }

    return 0;
}

#if 0
// not used
static int ccp_read_register(struct camdrv_device *dev, unsigned char crate_number, unsigned /*address*/, unsigned *data)
{
    unsigned char cmd = cmdREAD_REG;
    unsigned reply;
    int result;

#if 0
    if (crate_number < 1 || crate_number > 7) {
#else
    if (crate_number > 7) {
#endif
        return -EINVAL;
    }

    dev->tx_buffer[0] = (cmd << 4);
    dev->tx_buffer[1] = (cmd & 0xF0);
    dev->tx_buffer[2] = (crate_number << 4);
    dev->tx_buffer[3] = (crate_number & 0xF0);

    //??? where does the address go?
    
    result = ccp_inout(dev, 4, 4);
    if (result < 0) {
        return result;
    }

    reply = (
        ((unsigned short)(dev->rx_buffer[dev->start_n + 3] & 0x0F) << 12) |
        ((unsigned short)(dev->rx_buffer[dev->start_n + 2] & 0x0F) << 8) |
        ((unsigned short)(dev->rx_buffer[dev->start_n + 1] & 0x0F) << 4) |
        ((unsigned short)(dev->rx_buffer[dev->start_n] & 0x0F))
    );

    if (data) {
        *data = reply;
    }

    return 0;
}
#endif
