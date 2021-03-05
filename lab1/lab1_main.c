#include "lab1.h"

#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/time.h>
#include <linux/slab.h>

#define DRIVER_NAME "lab1_dev"
#define CLASS_NAME  "lab1_class"
#define DEV_NAME    "lab1"


static dev_t dev;
static struct cdev cdev;
static struct class * class;

static struct lab1_history * history;

static ssize_t dev_read(
        struct file * filp,
        char __user * ubuf,
        size_t count,
        loff_t * off
) {
    size_t len;
    char * buf;

    if (*off != 0) {
        return 0;
    }

    // TODO lock
    len = lab1_history_print(history, &buf);

    if (count < len) {
        kfree(buf);
        return -EINVAL;
    }

    if (len == 0) {
        kfree(buf);
        return 0;
    }

    if (copy_to_user(ubuf, buf, count) != 0) {
        kfree(buf);
        return -EFAULT;
    }

    kfree(buf);

    *off = len;
    return len;
}

static ssize_t dev_write(
        struct file * filp,
        const char __user * ubuf,
        size_t len,
        loff_t * off
) {
    // TODO lock
    history = lab1_history_new(len, history);

    return len;
}

static struct file_operations dev_fops = {
    .owner = THIS_MODULE,
    .read  = dev_read,
    .write = dev_write
};

static int __init ch_drv_init(void) {
    /* Allocate 1 char device number */
    if (alloc_chrdev_region(&dev, 0, 1, DRIVER_NAME) < 0) {
        return -1;
    }

    /* Register own devices class */
    if ((class = class_create(THIS_MODULE, CLASS_NAME)) == NULL) {
        unregister_chrdev_region(dev, 1);
        return -1;
    }

    /* Register own device */
    if (device_create(class, NULL, dev, NULL, DEV_NAME) == NULL) {
        class_destroy(class);
        unregister_chrdev_region(dev, 1);
        return -1;
    }

    history = NULL;

    /* Initialize cdev stucture (char device structure, a part of inode structure) */
    cdev_init(&cdev, &dev_fops);

    /* Add initialized cdev structure to the kernel for allocated device (1 minor devices) */
    if (cdev_add(&cdev, dev, 1) == -1) {
        device_destroy(class, dev);
        class_destroy(class);
        unregister_chrdev_region(dev, 1);
        return -1;
    }

    return 0;
}

static void __exit ch_drv_exit(void) {
    cdev_del(&cdev);
    device_destroy(class, dev);
    class_destroy(class);
    unregister_chrdev_region(dev, 1);
    lab1_history_delete(history);
}

module_init(ch_drv_init);
module_exit(ch_drv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eridan Domoratskiy & Eugene Lazurin");
MODULE_DESCRIPTION("Lab 1 I/O systems");
