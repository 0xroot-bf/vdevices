#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include "debug_device.h"

MODULE_LICENSE("GPL");

#define DEBUG_DEVICE_NAME "debug"

/* parameters */
static int debug_ndevices = DEBUG_NDEVICES;
static unsigned long debug_buffer_size = DEBUG_BUFFER_SIZE;
static unsigned long debug_block_size = DEBUG_BLOCK_SIZE;

module_param(debug_ndevices, int, S_IRUGO);
module_param(debug_buffer_size, ulong, S_IRUGO);
module_param(debug_block_size, ulong, S_IRUGO);

static unsigned int debug_major = 0;
static struct debug_dev *debug_devices = NULL;
static struct class *debug_class = NULL;

int debug_open(struct inode *inode, struct file *filp)
{
	unsigned int mj = imajor(inode);
	unsigned int mn = iminor(inode);
	
	struct debug_dev *dev = NULL;
	
	if (mj != debug_major || mn < 0 || mn >= debug_ndevices)
	{
		printk(KERN_WARNING "[DEBUG ERROR] No device found with minor=%d and major=%d\n", mj, mn);
		return -ENODEV; /* No such device */
	}
	
	/* store a pointer to struct debug_dev here for other methods */
	dev = &debug_devices[mn];
	filp->private_data = dev; 
	
	if (inode->i_cdev != &dev->cdev)
	{
		printk(KERN_WARNING "[DEBUG ERROR] open: internal error\n");
		return -ENODEV; /* No such device */
	}
	
	/* if opened the 1st time, allocate the buffer */
	if (dev->data == NULL)
	{
		dev->data = (unsigned char*)kzalloc(dev->buffer_size, GFP_KERNEL);
		if (dev->data == NULL)
		{
			printk(KERN_WARNING "[DEBUG ERROR] open(): out of memory\n");
			return -ENOMEM;
		}
	}
	return 0;
}

int 
debug_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t 
debug_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct debug_dev *dev = (struct debug_dev *)filp->private_data;
	ssize_t retval = 0;
	
	if (mutex_lock_killable(&dev->debug_mutex))
		return -EINTR;
	
	if (*f_pos >= dev->buffer_size) /* EOF */
		goto out;
	
	if (*f_pos + count > dev->buffer_size)
		count = dev->buffer_size - *f_pos;
	
	if (count > dev->block_size)
		count = dev->block_size;
	
	if (copy_to_user(buf, &(dev->data[*f_pos]), count) != 0)
	{
		retval = -EFAULT;
		goto out;
	}
	
	*f_pos += count;
	retval = count;
	
out:
  	mutex_unlock(&dev->debug_mutex);
	return retval;
}
                
ssize_t 
debug_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct debug_dev *dev = (struct debug_dev *)filp->private_data;
	ssize_t retval = 0;
	
	if (mutex_lock_killable(&dev->debug_mutex))
		return -EINTR;
	
	if (*f_pos >= dev->buffer_size) {
    /* Writing beyond the end of the buffer is not allowed. */
		retval = -EINVAL;
        goto out;
    }
	
	if (*f_pos + count > dev->buffer_size)
		count = dev->buffer_size - *f_pos;
	
	if (count > dev->block_size)
		count = dev->block_size;
	
	if (copy_from_user(&(dev->data[*f_pos]), buf, count) != 0)
	{
		retval = -EFAULT;
		goto out;
	}
	
	*f_pos += count;
	retval = count;
	
out:
  	mutex_unlock(&dev->debug_mutex);
	return retval;
}

loff_t 
debug_llseek(struct file *filp, loff_t off, int whence)
{
	struct debug_dev *dev = (struct debug_dev *)filp->private_data;
	loff_t newpos = 0;
	
	switch(whence) {
	  case 0: /* SEEK_SET */
		newpos = off;
		break;

	  case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	  case 2: /* SEEK_END */
		newpos = dev->buffer_size + off;
		break;

	  default: /* can't happen */
		return -EINVAL;
	}
	if (newpos < 0 || newpos > dev->buffer_size) 
        return -EINVAL;
    
	filp->f_pos = newpos;
	return newpos;
}

struct file_operations debug_fops = {
	.owner =    THIS_MODULE,
	.read =     debug_read,
	.write =    debug_write,
	.open =     debug_open,
	.release =  debug_release,
    .llseek =   debug_llseek,
};


static int debug_construct_device(struct debug_dev *dev, int minor, struct class *class)
{
	int err = 0;
	dev_t devno = MKDEV(debug_major, minor);
    struct device *device = NULL;
    
    BUG_ON(dev == NULL || class == NULL);

	/* Memory is to be allocated when the device is opened the first time */
	dev->data = NULL;     
    dev->buffer_size = debug_buffer_size;
	dev->block_size = debug_block_size;
	mutex_init(&dev->debug_mutex);
    
	cdev_init(&dev->cdev, &debug_fops);
	dev->cdev.owner = THIS_MODULE;
    
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
	{
		printk(KERN_WARNING "[DEBUG ERROR] Error %d while trying to add %s%d",
			err, DEBUG_DEVICE_NAME, minor);
        return err;
	}

    device = device_create(class, NULL, devno, NULL, DEBUG_DEVICE_NAME "%d", minor);

    if (IS_ERR(device)) {
        err = PTR_ERR(device);
        printk(KERN_WARNING "[DEBUG ERROR] Error %d while trying to create %s%d",
			err, DEBUG_DEVICE_NAME, minor);
        cdev_del(&dev->cdev);
        return err;
    }
	return 0;
}

/* Destroy the device and free its buffer */
static void debug_destroy_device(struct debug_dev *dev, int minor,
    struct class *class)
{
    BUG_ON(dev == NULL || class == NULL);
    device_destroy(class, MKDEV(debug_major, minor));
    cdev_del(&dev->cdev);
    kfree(dev->data);
    return;
}

static void debug_cleanup_module(int devices_to_destroy)
{
	int i;
	
	/* Get rid of character devices (if any exist) */
	if (debug_devices) {
		for (i = 0; i < devices_to_destroy; ++i) {
		    debug_destroy_device(&debug_devices[i], i, debug_class);
        }
		kfree(debug_devices);
	}
    
    if (debug_class)
        class_destroy(debug_class);

	unregister_chrdev_region(MKDEV(debug_major, 0), debug_ndevices);
	return;
}

static int __init debug_init_module(void)
{
	int err = 0;
	int i = 0;
    int devices_to_destroy = 0;
	dev_t dev = 0;
	
	if (debug_ndevices <= 0)
	{
		printk(KERN_WARNING "[DEBUG ERROR] Invalid value of debug_ndevices: %d\n", 
			debug_ndevices);
		err = -EINVAL;
		return err;
	}
	
	/* Get a range of minor numbers (starting with 0) to work with */
	err = alloc_chrdev_region(&dev, 0, debug_ndevices, DEBUG_DEVICE_NAME);
	if (err < 0) {
		printk(KERN_WARNING "[DEBUG ERROR] alloc_chrdev_region() failed\n");
		return err;
	}
    debug_major = MAJOR(dev);

    /* Create device class (before allocation of the array of devices) */
    debug_class = class_create(THIS_MODULE, DEBUG_DEVICE_NAME);
    if (IS_ERR(debug_class)) {
        err = PTR_ERR(debug_class);
        goto fail;
    }
	
	/* Allocate the array of devices */
	debug_devices = (struct debug_dev *)kzalloc(
		debug_ndevices * sizeof(struct debug_dev), 
		GFP_KERNEL);
	if (debug_devices == NULL) {
		err = -ENOMEM;
		goto fail;
	}
	
	/* Construct devices */
	for (i = 0; i < debug_ndevices; ++i) {
		err = debug_construct_device(&debug_devices[i], i, debug_class);
        if (err) {
            devices_to_destroy = i;
            goto fail;
        }
	}
	return 0; /* success */

fail:
	debug_cleanup_module(devices_to_destroy);
	return err;
}

static void __exit
debug_exit_module(void)
{
	debug_cleanup_module(debug_ndevices);
	return;
}

module_init(debug_init_module);
module_exit(debug_exit_module);
