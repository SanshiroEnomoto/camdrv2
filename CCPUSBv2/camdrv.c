/* camdrv.c */
/* Created by Sanshiro Enomoto on 11 April 1999 */
/* Last updated by Sanshiro Enomoto on 8 November 2025 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include "camdrv.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Enomoto Sanshiro");
MODULE_DESCRIPTION("Camac Driver for Hoshin CCP-USB(V2) Controller");
MODULE_VERSION("1.00");

#define DRIVER_NAME "camdrv"
#define DEVICE_NAME "camdrv"
#define CCP_VENDOR_ID 0x24b9
#define CCP_PRODUCT_ID 0x0020


#define BUFFER_SIZE 64
#define LATENCY_TIME 2
#define EE_BUFFER_SIZE 512
#define SET_RD_SIZE 512
#define TIMEOUT_MS 500

// FTDI control commands
#define FTDI_SIO_RESET_REQUEST_TYPE 0x40
#define FTDI_SIO_RESET_REQUEST 0x00
#define FTDI_SIO_RESET_SIO 0
#define FTDI_SIO_RESET_PURGE_RX 1
#define FTDI_SIO_RESET_PURGE_TX 2

#define FTDI_SIO_SET_BITMODE_REQUEST_TYPE 0x40
#define FTDI_SIO_SET_BITMODE_REQUEST 0x0b
#define FTDI_BITMODE_RESET 0x00
#define FTDI_BITMODE_SYNC_FIFO 0x40

#define FTDI_SIO_SET_LATENCY_TIMER_REQUEST_TYPE 0x40
#define FTDI_SIO_SET_LATENCY_TIMER_REQUEST 0x09

#define FTDI_SIO_SET_EVENT_CHAR_REQUEST_TYPE 0x40
#define FTDI_SIO_SET_EVENT_CHAR_REQUEST 0x06

#define FTDI_SIO_SET_USB_PARAMETERS_REQUEST_TYPE 0x40
#define FTDI_SIO_SET_USB_PARAMETERS_REQUEST 0x07

// USB interface
#define FTDI_INTERFACE_A 0

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

static int ccp_init(struct camdrv_device *dev, unsigned char crate_number);
static int ccp_inout(struct camdrv_device *dev, unsigned int write_size, unsigned int read_size);
static int initialize(struct camdrv_device *dev, unsigned crate_number);
static int clear(struct camdrv_device *dev, unsigned crate_number);
static int camac_action(struct camdrv_device *dev, unsigned crate, unsigned n, unsigned a, unsigned f, unsigned* data);
static int read_lam(struct camdrv_device *dev, unsigned char crate_number, unsigned *data);
static int wait_lam(struct camdrv_device *dev, unsigned char crate_number, unsigned timeout, unsigned* data);


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
    
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) {
        return -ENOMEM;
    }
    
    dev->udev = usb_get_dev(udev);
    dev->interface = interface;
    
    // Find bulk endpoints
    iface_desc = interface->cur_altsetting;
    for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
        endpoint = &iface_desc->endpoint[i].desc;
        if (usb_endpoint_is_bulk_in(endpoint)) {
            if (!dev->bulk_in)
                dev->bulk_in = endpoint;
        } else if (usb_endpoint_is_bulk_out(endpoint)) {
            if (!dev->bulk_out)
                dev->bulk_out = endpoint;
        }
    }
    
    if (!dev->bulk_in || !dev->bulk_out) {
        dev_err(&interface->dev, "Could not find bulk endpoints\n");
        result = -ENODEV;
        goto error;
    }
    
    dev->tx_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    dev->rx_buffer = kmalloc(SET_RD_SIZE, GFP_KERNEL);
    if (!dev->tx_buffer || !dev->rx_buffer) {
        result = -ENOMEM;
        goto error;
    }
    
    mutex_init(&dev->mutex);
    dev->is_open = false;
    dev->crate_number = 1;  // in CCP, crate 0 -> number 1
    
    cdev_init(&dev->cdev, &camdrv_fops);
    dev->cdev.owner = THIS_MODULE;
    result = cdev_add(&dev->cdev, dev_num, 1);
    if (result) {
        dev_err(&interface->dev, "Error %d adding cdev\n", result);
        goto error;
    }
    
    usb_set_intfdata(interface, dev);
    
    dev_info(&interface->dev, "CCP-USB(V2) device attached\n");
    
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
    
    dev = container_of(inode->i_cdev, struct camdrv_device, cdev);
    file->private_data = dev;
    
    if (mutex_lock_interruptible(&dev->mutex)) {
        return -ERESTARTSYS;
    }
    
    if (dev->is_open) {
        result = -EBUSY;
        goto err_unlock;
    }
    
    result = ftdi_init_sync_fifo(dev);
    if (result < 0) {
        dev_err(&dev->udev->dev, "Failed to initialize FTDI device\n");
        goto err_unlock;
    }
    
    result = ccp_init(dev, dev->crate_number);
    if (result < 0) {
        dev_err(&dev->udev->dev, "Failed to initialize CCP interface\n");
        goto err_unlock;
    }
    pr_info("CCP-USB(V2) opened\n");
    
    dev->is_open = true;
    mutex_unlock(&dev->mutex);
    
    return 0;

  err_unlock:
    mutex_unlock(&dev->mutex);
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
    
    user_parameter_ptr = (unsigned *) arg;
    user_data_ptr = (unsigned *) arg + 1;

    if (_IOC_TYPE(cmd) != CAMDRV_IOC_MAGIC) {
        return -EINVAL;
    }
    if (_IOC_DIR(cmd) & (_IOC_READ | _IOC_WRITE)) {
	if (get_user(parameter, user_parameter_ptr) < 0) {
	    return -EFAULT;
	}
	if (get_user(data, user_data_ptr) < 0) {
	    return -EFAULT;
	}
    }

    if (mutex_lock_interruptible(&dev->mutex)) {
        return -ERESTARTSYS;
    }
    
    switch (cmd) {
      case CAMDRV_IOC_INITIALIZE:
        result = initialize(dev, crate_number);
        break;
      case CAMDRV_IOC_CLEAR:
        result = clear(dev, crate_number);
        break;
      case CAMDRV_IOC_INHIBIT:
	result = -EINVAL;
        break;
      case CAMDRV_IOC_RELEASE_INHIBIT:
	result = -EINVAL;
        break;
      case CAMDRV_IOC_ENABLE_INTERRUPT:
	result = -EINVAL;
        break;
      case CAMDRV_IOC_DISABLE_INTERRUPT:
	result = -EINVAL;
        break;
      case CAMDRV_IOC_CAMAC_ACTION:
        n = (parameter >> 9) & 0x1f;
        a = (parameter >> 5) & 0x0f;
        f = (parameter >> 0) & 0x1f;
        result = camac_action(dev, crate_number, n, a, f, &data);
        put_user(data, user_data_ptr);
        break;
      case CAMDRV_IOC_READ_LAM:
        result = read_lam(dev, crate_number, &data);
        put_user(data, user_data_ptr);
        break;
      case CAMDRV_IOC_WAIT_LAM:
        result = wait_lam(dev, crate_number, parameter, &data);
        put_user(data, user_data_ptr);
        break;
      case CAMDRV_IOC_SET_CRATE:
        dev->crate_number = parameter + 1;   // in CCP, crate 0 -> number 1
        result = ccp_init(dev, crate_number);
        break;
      default:
	result = -EINVAL;
    }

    mutex_unlock(&dev->mutex);
    
    return result;
}


//// FTDI ////

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

    // Reset device
    result = ftdi_control_request(
        udev, FTDI_SIO_RESET_REQUEST_TYPE,
        FTDI_SIO_RESET_REQUEST,
        FTDI_SIO_RESET_SIO, FTDI_INTERFACE_A,
        NULL, 0
    );
    if (result < 0) {
        return result;
    }

    // Reset bit mode
    result = ftdi_control_request(
        udev, FTDI_SIO_SET_BITMODE_REQUEST_TYPE,
        FTDI_SIO_SET_BITMODE_REQUEST,
        FTDI_BITMODE_RESET, FTDI_INTERFACE_A,
        NULL, 0
    );
    if (result < 0) {
        return result;
    }

    // Set synchronous FIFO mode (0xF0 = pin direction, 0x40 = sync FIFO mode)
    result = ftdi_control_request(
        udev, FTDI_SIO_SET_BITMODE_REQUEST_TYPE,
        FTDI_SIO_SET_BITMODE_REQUEST,
        (0xF0 << 8) | FTDI_BITMODE_SYNC_FIFO,
        FTDI_INTERFACE_A, NULL, 0
    );
    if (result < 0) {
        return result;
    }

    // Set latency timer
    result = ftdi_control_request(
        udev, FTDI_SIO_SET_LATENCY_TIMER_REQUEST_TYPE,
        FTDI_SIO_SET_LATENCY_TIMER_REQUEST,
        LATENCY_TIME, FTDI_INTERFACE_A,
        NULL, 0
    );
    if (result < 0) {
        return result;
    }

    // Set USB parameters (buffer size)
    result = ftdi_control_request(
        udev, FTDI_SIO_SET_USB_PARAMETERS_REQUEST_TYPE,
        FTDI_SIO_SET_USB_PARAMETERS_REQUEST,
        EE_BUFFER_SIZE, FTDI_INTERFACE_A,
        NULL, 0
    );
    if (result < 0) {
        return result;
    }

    return 0;
}


//// CCP ////

enum ccp_command {
    cmdINITIALIZE_CCP = 73,
    cmdCAMAC = 67,
    cmdLAM = 76
};

enum ccp_ctrlbits {
    ctrlINITIALIZE = 0x40,
    ctrlCLEAR = 0x80
};


static int ccp_init(struct camdrv_device *dev, unsigned char crate_number)
{
    unsigned char cc_com = cmdINITIALIZE_CCP;
    int result;

    if (crate_number < 1 || crate_number > 7) {
        return -EINVAL;
    }

    // Reset device
    result = ftdi_control_request(
        dev->udev, FTDI_SIO_RESET_REQUEST_TYPE,
        FTDI_SIO_RESET_REQUEST,
        FTDI_SIO_RESET_SIO, FTDI_INTERFACE_A,
        NULL, 0
    );
    if (result < 0) {
        return result;
    }

    // Prepare command
    dev->tx_buffer[0] = (cc_com << 4);
    dev->tx_buffer[1] = (cc_com & 0xF0);
    dev->tx_buffer[2] = (crate_number << 4);
    dev->tx_buffer[3] = (crate_number & 0xF0);
    
    // Send and receive
    result = ccp_inout(dev, 4, 4);
    if (result < 0) {
        return result;
    }

    // Extract result
    result = (
        ((dev->rx_buffer[dev->start_n + 1] & 0x0F) << 4) | (dev->rx_buffer[dev->start_n] & 0x0F)
    );

    return result;
}


static int ccp_inout(struct camdrv_device *dev, unsigned int write_size, unsigned int read_size)
{
    struct usb_device *udev = dev->udev;
    int result;
    int actual_length;
    unsigned int i;
    unsigned char c_com_int;
    
    if (!dev->bulk_in || !dev->bulk_out) {
        return -ENODEV;
    }

    // Purge RX buffer
    ftdi_control_request(
        udev, FTDI_SIO_RESET_REQUEST_TYPE,
        FTDI_SIO_RESET_REQUEST,
        FTDI_SIO_RESET_PURGE_RX, FTDI_INTERFACE_A,
        NULL, 0
    );

    // Write data
    result = usb_bulk_msg(
        udev, usb_sndbulkpipe(udev, dev->bulk_out->bEndpointAddress),
        dev->tx_buffer, write_size, &actual_length,
        TIMEOUT_MS
    );
    if (result < 0) {
        dev_err(&udev->dev, "Write failed: %d\n", result);
        return -EIO;
    }

    // Read data
    result = usb_bulk_msg(
        udev, usb_rcvbulkpipe(udev, dev->bulk_in->bEndpointAddress),
        dev->rx_buffer, SET_RD_SIZE, &actual_length,
        TIMEOUT_MS
    );
    if (result < 0) {
        dev_err(&udev->dev, "Read failed: %d\n", result);
        return -EIO;
    }

    if (actual_length < read_size) {
        dev_err(
            &udev->dev, "Read size mismatch: expected %u, got %d\n",
            read_size, actual_length
        );
        return -EIO;
    }

    // Find start marker (0x43)
    for (i = 0; i <= actual_length - read_size; i++) {
        c_com_int = (dev->rx_buffer[i + 1] << 4) | (dev->rx_buffer[i] & 0x0F);
        if (c_com_int == 0x43) {
            dev->start_n = i + 2;
            return 0;
        }
    }

    return -EIO;
}


static int initialize(struct camdrv_device *dev, unsigned crate_number)
{
    return camac_action(dev, crate_number, 0, 0, ctrlINITIALIZE, 0);
}


static int clear(struct camdrv_device *dev, unsigned crate_number)
{
    return camac_action(dev, crate_number, 0, 0, ctrlCLEAR, 0);
}


static int camac_action(struct camdrv_device *dev, unsigned crate_number, unsigned n, unsigned a, unsigned f, unsigned* data)
{
    unsigned char cmd = cmdCAMAC;
    unsigned dl, dm, dh;
    unsigned int read_size;
    int result;
    
    if (crate_number <= 0  || crate_number >= 8) {
        return -EINVAL;
    }
    if (n >= 24 || a >= 16 || f >= 32) {
        return -EINVAL;
    }
    if (f > 15) {
        read_size = 4;
    }
    else {
        read_size = 10;
    }
    dl = (*data & 0x0000ff);
    dm = (*data & 0x00ff00) >> 8;
    dh = (*data & 0xff0000) >> 16;
    *data = 0;
    
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
        return result;
    }
    
    if (f > 15) {
        *data = (
            ((dev->rx_buffer[dev->start_n + 1] & 0x0F) << 4) |
            (dev->rx_buffer[dev->start_n] & 0x0F)
        );
    }
    else {
        *data = (
            ((unsigned int)(dev->rx_buffer[dev->start_n + 7] & 0x0F) << 28) |
            ((unsigned int)(dev->rx_buffer[dev->start_n + 6] & 0x0F) << 24) |
            ((unsigned int)(dev->rx_buffer[dev->start_n + 5] & 0x0F) << 20) |
            ((unsigned int)(dev->rx_buffer[dev->start_n + 4] & 0x0F) << 16) |
            ((unsigned int)(dev->rx_buffer[dev->start_n + 3] & 0x0F) << 12) |
            ((unsigned int)(dev->rx_buffer[dev->start_n + 2] & 0x0F) << 8) |
            ((unsigned int)(dev->rx_buffer[dev->start_n + 1] & 0x0F) << 4) |
            ((unsigned int)(dev->rx_buffer[dev->start_n] & 0x0F))
        );
    }
    
    return 0;
}


static int read_lam(struct camdrv_device *dev, unsigned char crate_number, unsigned *data)
{
    unsigned char cmd = cmdLAM;
    unsigned reply, encoded_lam = 0;
    int result;
    
    if (crate_number < 1 || crate_number > 7) {
        return -EINVAL;
    }

    dev->tx_buffer[0] = (cmd << 4);
    dev->tx_buffer[1] = (cmd & 0xF0);
    dev->tx_buffer[2] = (crate_number << 4);
    dev->tx_buffer[3] = (crate_number & 0xF0);
    *data = 0;
    
    result = ccp_inout(dev, 4, 6);
    if (result < 0) {
        return result;
    }
                
    reply = (
        ((unsigned short)(dev->rx_buffer[dev->start_n + 3] & 0x0F) << 12) |
        ((unsigned short)(dev->rx_buffer[dev->start_n + 2] & 0x0F) << 8) |
        ((unsigned short)(dev->rx_buffer[dev->start_n + 1] & 0x0F) << 4) |
        ((unsigned short)(dev->rx_buffer[dev->start_n] & 0x0F))
    );

    encoded_lam = (reply & 0xff00) >> 8;
    if (encoded_lam > 0) {
        *data = 0x0001 << (encoded_lam - 1);
    }

    return *data;
}


static int wait_lam(struct camdrv_device *dev, unsigned char crate_number, unsigned timeout, unsigned* data)
{
    unsigned long timeout_jiffies;

    /* The hardware does not support "interrupt on LAM". */
    /* The following code is a "polling loop" to wait for any LAM bits. */
    timeout_jiffies = jiffies + timeout * HZ;
    while ((read_lam(dev, crate_number, data) >= 0) && (*data == 0)) {
	schedule();
	if (jiffies > timeout_jiffies) {
	    *data = 0;
	    return -ETIMEDOUT;
	}
    }

    return *data;
}
