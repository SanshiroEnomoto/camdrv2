/* camdrv.c */
/* CAMAC device driver for Hoshin CCP-USB on modern Linux */
/* Created by Enomoto Sanshiro on 3 October 2013. */
/* Updated for modern Linux kernel by Enomoto Sanshiro. */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "camdrv.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Enomoto Sanshiro");
MODULE_DESCRIPTION("Camac Driver for Hoshin CCP-USB Controller");

#define VENDOR_ID 0x24b9
#define PRODUCT_ID 0x0011
#define BUFFER_SIZE 64
#define camdrv_name "camdrv"

typedef unsigned char byte;

struct usb_device* ccp_udev = 0;
int addr_in, addr_out;

static int open_count;
static int crate_number = 0;
static byte* buffer_snd;
static byte* buffer_rcv;


static int camdrv_probe(struct usb_interface* intf, const struct usb_device_id* id);
static void camdrv_disconnect(struct usb_interface* intf);
static int camdrv_open(struct inode* inode, struct file* filep);
static int camdrv_release(struct inode* inode, struct file* filep);
static long camdrv_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);

static int ccp_init(void);
static int ccp_reg_read(byte address, byte* data);
static int ccp_reg_write(byte address, byte data);

static int initialize(void);
static int clear(void);
static int inhibit(void);
static int release_inhibit(void);
static int camac_action(unsigned n, unsigned a, unsigned f, unsigned* data);
static int read_lam(unsigned* data);
static int enable_interrupt(void);
static int disable_interrupt(void);
static int wait_lam(unsigned time_out, unsigned* data);


static struct usb_device_id camdrv_id_table[] = {
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
    {}
};
MODULE_DEVICE_TABLE(usb, camdrv_id_table);


static struct usb_driver camdrv_driver = {
    .name = camdrv_name,
    .id_table = camdrv_id_table,
    .probe = camdrv_probe,
    .disconnect = camdrv_disconnect
};


static struct file_operations camdrv_fopts = {
    .owner = THIS_MODULE,
    .open = camdrv_open,
    .release = camdrv_release,
    .unlocked_ioctl = camdrv_ioctl
};

static struct usb_class_driver camdrv_class_driver = {
    .name = camdrv_name,
    .fops = &camdrv_fopts
};



static int __init camdrv_init(void)
{
    int result;

    result = usb_register(&camdrv_driver);
    if (result < 0) {
        printk(KERN_WARNING "%s: USB registration failure: %d\n", camdrv_name, result);
        return result;
    }
    printk(KERN_INFO "%s: USB registered.\n", camdrv_name);

    return 0;
}

module_init(camdrv_init);

static void __exit camdrv_exit(void)
{
    usb_deregister(&camdrv_driver);
    printk(KERN_INFO "%s: removed.\n", camdrv_name);
}

module_exit(camdrv_exit);

static int camdrv_probe(struct usb_interface* intf, const struct usb_device_id* id)
{
    struct usb_host_interface* host_intf;
    struct usb_interface_descriptor* intf_desc;
    struct usb_host_endpoint* endpoint;
    int address, type, direction;
    size_t max_packet_size;
    int i;

    addr_in = -1;
    addr_out = -1;

    host_intf = intf->cur_altsetting;
    intf_desc = &host_intf->desc;
    printk(KERN_INFO "%s: probed: interface %d\n", camdrv_name, intf_desc->bInterfaceNumber);

    for (i = 0; i < intf_desc->bNumEndpoints; i++) {
        endpoint = &host_intf->endpoint[i];
        type = endpoint->desc.bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
        address = endpoint->desc.bEndpointAddress;
        direction = address & USB_ENDPOINT_DIR_MASK;
        max_packet_size = endpoint->desc.wMaxPacketSize;
        if (type == USB_ENDPOINT_XFER_BULK) {
            if (direction == USB_DIR_IN) {
                addr_in = address;
                printk(
                    KERN_INFO "%s: endpoint bulk-in at %d (size=%d).\n", 
                    camdrv_name, addr_in, (int) max_packet_size
                );
            }
            else if (direction == USB_DIR_OUT) {
                addr_out = address;
                printk(
                    KERN_INFO "%s: endpoint bulk-out at %d (size=%d).\n", 
                    camdrv_name, addr_out, (int) max_packet_size
                );
            }
        }
    }

    if ((addr_in < 0) || (addr_out < 0)) {
        return -ENODEV;
    }

    if (usb_register_dev(intf, &camdrv_class_driver) != 0) {
        printk(KERN_WARNING "%s: unable to get minor\n", camdrv_name);
        return -EBUSY;
    }
    printk(KERN_INFO "%s: minor = %d.\n", camdrv_name, intf->minor);

    buffer_snd = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!buffer_snd) {
        usb_deregister_dev(intf, &camdrv_class_driver);
        return -ENOMEM;
    }

    buffer_rcv = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!buffer_rcv) {
        kfree(buffer_snd);
        usb_deregister_dev(intf, &camdrv_class_driver);
        return -ENOMEM;
    }

    ccp_udev = interface_to_usbdev(intf);
    open_count = 0;

    return 0;
}

static void camdrv_disconnect(struct usb_interface* intf)
{
    if (buffer_snd) {
        kfree(buffer_snd);
        buffer_snd = NULL;
    }
    if (buffer_rcv) {
        kfree(buffer_rcv);
        buffer_rcv = NULL;
    }

    usb_deregister_dev(intf, &camdrv_class_driver);

    printk(KERN_INFO "%s: disconnected: interface %d\n", 
           camdrv_name, intf->cur_altsetting->desc.bInterfaceNumber);
}

static int camdrv_open(struct inode* inode, struct file* filep)
{
    if (open_count) {
        return -EBUSY;
    }
    open_count++;

    if (ccp_init() == 0) {
        printk(KERN_INFO "%s: opened.\n", camdrv_name);
    }

    return 0;
}

static int camdrv_release(struct inode* inode, struct file* filep)
{
    printk(KERN_INFO "%s: closed.\n", camdrv_name);
    open_count--;

    return 0;
}

static long camdrv_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    unsigned parameter = 0, data = 0;
    unsigned *user_parameter_ptr, *user_data_ptr;
    unsigned n, a, f;
    int result = -EINVAL;

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

    switch (cmd) {
      case CAMDRV_IOC_INITIALIZE:
        result = initialize();
        break;
      case CAMDRV_IOC_CLEAR:
        result = clear();
        break;
      case CAMDRV_IOC_INHIBIT:
        result = inhibit();
        break;
      case CAMDRV_IOC_RELEASE_INHIBIT:
        result = release_inhibit();
        break;
      case CAMDRV_IOC_ENABLE_INTERRUPT:
        result = enable_interrupt();
        break;
      case CAMDRV_IOC_DISABLE_INTERRUPT:
        result = disable_interrupt();
        break;
      case CAMDRV_IOC_CAMAC_ACTION:
        n = (parameter >> 9) & 0x1f;
        a = (parameter >> 5) & 0x0f;
        f = (parameter >> 0) & 0x1f;
        result = camac_action(n, a, f, &data);
        put_user(data, user_data_ptr);
        break;
      case CAMDRV_IOC_READ_LAM:
        result = read_lam(&data);
        put_user(data, user_data_ptr);
        break;
      case CAMDRV_IOC_WAIT_LAM:
        result = wait_lam(parameter, &data);
        put_user(data, user_data_ptr);
        break;
      case CAMDRV_IOC_SET_CRATE:
        crate_number = parameter;
        result = ccp_init();
        break;
      default:
	result = -EINVAL;
    }

    return result;
}


/* CCP I/O */

static int ccp_out(byte* data, int size)
{
    unsigned pipe;
    int result, count;

    pipe = usb_sndbulkpipe(ccp_udev, addr_out);
    result = usb_bulk_msg(ccp_udev, pipe, data, size, &count, 10*HZ);
    if (result != 0) {
        printk(KERN_WARNING "%s: unable to write to USB: error=%d\n", camdrv_name, result);
        return -1;
    }
    if (count != size) {
        printk(KERN_WARNING "%s: unable to write to USB: count=%d\n", camdrv_name, count);
        return -1;
    }

    return count;
}

static const byte* ccp_in(int size)
{
    unsigned pipe_rcv;
    int result, count;

    pipe_rcv = usb_rcvbulkpipe(ccp_udev, addr_in);
    result = usb_bulk_msg(
        ccp_udev, pipe_rcv, buffer_rcv, BUFFER_SIZE, &count, 10*HZ
        /* ccp_udev, pipe_rcv, buffer_rcv, size+2, &count, 10*HZ */
    );
    if (result != 0) {
        printk(
            KERN_WARNING "%s: unable to read from USB: error=%d\n", 
            camdrv_name, result
        );
        return 0;
    }
    if ((result != 0) || (count < size)) {
        printk(
            KERN_WARNING "%s: unable to read from USB: count=%d\n", 
            camdrv_name, count
        );
        return 0;
    }

    /* somehow CCP-USB sends out two extra bytes in the beginning (49-96). */
    return &(buffer_rcv[(count > size) ? count - size : 0]);
}


/* CAMAC service functions */

enum ccp_command {
    cmdINITIALIZE_CCP = 73,
    cmdCAMAC = 67,
    cmdLAM = 76,
    cmdREGREAD = 82,
    cmdREGWRITE = 87,
    replyOK = 67
};

enum ccp_ctrlbits {
    ctrlCLEAR = 0x80,
    ctrlINITIALIZE = 0x40
};

enum ccp_statbits {
    statQ = 0x01,
    statX = 0x02,
    statI = 0x04
};

static int ccp_init(void)
{
    const byte* reply;
    int status;
    
    buffer_snd[0] = cmdINITIALIZE_CCP;
    buffer_snd[1] = crate_number;

    if (ccp_out(buffer_snd, 2) < 0) {
        return -EIO;
    }
    if ((reply = ccp_in(2)) == 0) {
        return -EIO;
    }
    if (reply[0] != replyOK) {
        printk(KERN_WARNING "%s: CCP initialization error: %d\n", camdrv_name, (int) reply[0]);
        return -EIO;
    }
    printk(KERN_INFO "%s: CCP initialized: status = %d\n", camdrv_name, (int) reply[1]);

    status = reply[1];

    return status;
}

static int ccp_reg_read(byte address, byte* data)
{
    const byte* reply;
    int status;
    
    buffer_snd[0] = cmdREGREAD;
    buffer_snd[1] = crate_number;
    buffer_snd[2] = address;

    if (ccp_out(buffer_snd, 3) < 0) {
        return -EIO;
    }
    if ((reply = ccp_in(3)) == 0) {
        return -EIO;
    }
    if (reply[0] != replyOK) {
        printk(KERN_WARNING "%s: CCP read error: %d\n", camdrv_name, (int) reply[0]);
        return -EIO;
    }

    status = reply[1];
    *data = reply[2];

    return status;
}

static int ccp_reg_write(byte address, byte data)
{
    const byte* reply;
    int status;
    
    buffer_snd[0] = cmdREGWRITE;
    buffer_snd[1] = crate_number;
    buffer_snd[2] = address;
    buffer_snd[3] = data;

    if (ccp_out(buffer_snd, 4) < 0) {
        return -EIO;
    }
    if ((reply = ccp_in(2)) == 0) {
        return -EIO;
    }
    if (reply[0] != replyOK) {
        printk(KERN_WARNING "%s: CCP read error: %d\n", camdrv_name, (int) reply[0]);
        return -EIO;
    }

    status = reply[1];

    return status;
}

static int initialize(void)
{
    int cc = crate_number, result;

    crate_number = cc+1;  /* CCP bug??? */
    result = camac_action(0, 0, ctrlINITIALIZE, 0);
    crate_number = cc;

    return result;
}

static int clear(void)
{
    return camac_action(0, 0, ctrlCLEAR, 0);
}

static int inhibit(void)
{
    /* this function is not supported by the hardware */
    return 0;
}

static int release_inhibit(void)
{
    /* this function is not supported by the hardware */
    return 0;
}

static int enable_interrupt(void)
{
    /* this function is not supported by the hardware */
    return 0;
}

static int disable_interrupt(void)
{
    /* this function is not supported by the hardware */
    return 0;
}

static int camac_action(unsigned n, unsigned a, unsigned f, unsigned* data)
{
    int nq, nx;
    const byte* reply;

    buffer_snd[0] = cmdCAMAC;
    buffer_snd[1] = crate_number;
    buffer_snd[2] = n;
    buffer_snd[3] = a;
    buffer_snd[4] = f;
    if (data) {
        buffer_snd[5] = (*data >> 0) & 0x00ff;
        buffer_snd[6] = (*data >> 8) & 0x00ff;
        buffer_snd[7] = (*data >> 16) & 0x00ff;
    }

    if (ccp_out(buffer_snd, 8) < 0) {
        return -EIO;
    }
    if ((reply = ccp_in(5)) == 0) {
        return -EIO;
    }
    if (reply[0] != replyOK) {
        printk(KERN_WARNING "%s: CCP read error: %d\n", camdrv_name, (int) reply[0]);
        return -EIO;
    }

    if (data) {
        *data = (reply[4] << 16) | (reply[3] << 8) | reply[2];
    }
    nq = (reply[1] & statQ) ? 0x00 : 0x01;
    nx = (reply[1] & statX) ? 0x00 : 0x01;

    return ((nx << 1) | nq);
}

static int read_lam(unsigned* data)
{
    const byte* reply;
    unsigned short encoded_lam;

    buffer_snd[0] = cmdLAM;
    buffer_snd[1] = crate_number;

    if (ccp_out(buffer_snd, 2) < 0) {
        *data = 0;
        return -EIO;
    }
    if ((reply = ccp_in(3)) == 0) {
        *data = 0;
        return -EIO;
    }
    if (reply[0] != replyOK) {
        printk(KERN_WARNING "%s: CCP read error: %d\n", camdrv_name, (int) reply[0]);
        *data = 0;
        return -EIO;
    }
    
    encoded_lam = reply[2];

    if (encoded_lam > 0) {
        *data = 0x0001 << (encoded_lam - 1);
    }
    else {
        *data = 0;
    }

    return *data;
}

static int wait_lam(unsigned timeout, unsigned* data)
{
    unsigned long timeout_jiffies;

    /* The hardware does not support "interrupt on LAM". */
    /* The following code is a "polling loop" to wait for any LAM bits. */
    timeout_jiffies = jiffies + timeout * HZ;
    while ((read_lam(data) >= 0) && (*data == 0)) {
	schedule();
	if (jiffies > timeout_jiffies) {
	    *data = 0;
	    return -ETIMEDOUT;
	}
    }

    return *data;
}
