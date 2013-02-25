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
#include "vialab_device.h"

MODULE_LICENSE("GPL");

#define VIALAB_DEVICE_NAME "vialab"

/* parameters */
static int vialab_ndevices = VIALAB_NDEVICES;
static unsigned long vialab_buffer_size = VIALAB_BUFFER_SIZE;
static unsigned long vialab_block_size = VIALAB_BLOCK_SIZE;

module_param(vialab_ndevices, int, S_IRUGO);
module_param(vialab_buffer_size, ulong, S_IRUGO);
module_param(vialab_block_size, ulong, S_IRUGO);

static unsigned int vialab_major = 0;
static struct vialab_dev *vialab_devices = NULL;
static struct class *vialab_class = NULL;

int vialab_open(struct inode *inode, struct file *filp)
{
	unsigned int mj = imajor(inode);
	unsigned int mn = iminor(inode);
	
	struct vialab_dev *dev = NULL;
	
	if (mj != vialab_major || mn < 0 || mn >= vialab_ndevices)
	{
		printk(KERN_WARNING "[VIALAB ERROR] No device found with minor=%d and major=%d\n", mj, mn);
		return -ENODEV; /* No such device */
	}
	
	/* store a pointer to struct vialab_dev here for other methods */
	dev = &vialab_devices[mn];
	filp->private_data = dev; 
	
	if (inode->i_cdev != &dev->cdev)
	{
		printk(KERN_WARNING "[VIALAB ERROR] open: internal error\n");
		return -ENODEV; /* No such device */
	}
	
	/* if opened the 1st time, allocate the buffer */
	if (dev->data == NULL)
	{
		dev->data = (unsigned char*)kzalloc(dev->buffer_size, GFP_KERNEL);
		if (dev->data == NULL)
		{
			printk(KERN_WARNING "[VIALAB ERROR] open(): out of memory\n");
			return -ENOMEM;
		}
	}
	return 0;
}

int 
vialab_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t 
vialab_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct vialab_dev *dev = (struct vialab_dev *)filp->private_data;
	ssize_t retval = 0;
	
	if (mutex_lock_killable(&dev->vialab_mutex))
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
  	mutex_unlock(&dev->vialab_mutex);
	return retval;
}
                
ssize_t 
vialab_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct vialab_dev *dev = (struct vialab_dev *)filp->private_data;
	ssize_t retval = 0;
	
	if (mutex_lock_killable(&dev->vialab_mutex))
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
  	mutex_unlock(&dev->vialab_mutex);
	return retval;
}

loff_t 
vialab_llseek(struct file *filp, loff_t off, int whence)
{
	struct vialab_dev *dev = (struct vialab_dev *)filp->private_data;
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

struct file_operations vialab_fops = {
	.owner =    THIS_MODULE,
	.read =     vialab_read,
	.write =    vialab_write,
	.open =     vialab_open,
	.release =  vialab_release,
    .llseek =   vialab_llseek,
};


static int vialab_construct_device(struct vialab_dev *dev, int minor, struct class *class)
{
	int err = 0;
	dev_t devno = MKDEV(vialab_major, minor);
    struct device *device = NULL;
    
    BUG_ON(dev == NULL || class == NULL);

	/* Memory is to be allocated when the device is opened the first time */
	dev->data = NULL;     
    dev->buffer_size = vialab_buffer_size;
	dev->block_size = vialab_block_size;
	mutex_init(&dev->vialab_mutex);
    
	cdev_init(&dev->cdev, &vialab_fops);
	dev->cdev.owner = THIS_MODULE;
    
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
	{
		printk(KERN_WARNING "[VIALAB ERROR] Error %d while trying to add %s%d",
			err, VIALAB_DEVICE_NAME, minor);
        return err;
	}

    device = device_create(class, NULL, devno, NULL, VIALAB_DEVICE_NAME "%d", minor);

    if (IS_ERR(device)) {
        err = PTR_ERR(device);
        printk(KERN_WARNING "[VIALAB ERROR] Error %d while trying to create %s%d",
			err, VIALAB_DEVICE_NAME, minor);
        cdev_del(&dev->cdev);
        return err;
    }
	return 0;
}

/* Destroy the device and free its buffer */
static void vialab_destroy_device(struct vialab_dev *dev, int minor,
    struct class *class)
{
    BUG_ON(dev == NULL || class == NULL);
    device_destroy(class, MKDEV(vialab_major, minor));
    cdev_del(&dev->cdev);
    kfree(dev->data);
    return;
}

static void vialab_cleanup_module(int devices_to_destroy)
{
	int i;
	
	/* Get rid of character devices (if any exist) */
	if (vialab_devices) {
		for (i = 0; i < devices_to_destroy; ++i) {
		    vialab_destroy_device(&vialab_devices[i], i, vialab_class);
        }
		kfree(vialab_devices);
	}
    
    if (vialab_class)
        class_destroy(vialab_class);

	unregister_chrdev_region(MKDEV(vialab_major, 0), vialab_ndevices);
	return;
}

static int __init vialab_init_module(void)
{
	int err = 0;
	int i = 0;
    int devices_to_destroy = 0;
	dev_t dev = 0;
	
	if (vialab_ndevices <= 0)
	{
		printk(KERN_WARNING "[VIALAB ERROR] Invalid value of vialab_ndevices: %d\n", 
			vialab_ndevices);
		err = -EINVAL;
		return err;
	}
	
	/* Get a range of minor numbers (starting with 0) to work with */
	err = alloc_chrdev_region(&dev, 0, vialab_ndevices, VIALAB_DEVICE_NAME);
	if (err < 0) {
		printk(KERN_WARNING "[VIALAB ERROR] alloc_chrdev_region() failed\n");
		return err;
	}
    vialab_major = MAJOR(dev);

    /* Create device class (before allocation of the array of devices) */
    vialab_class = class_create(THIS_MODULE, VIALAB_DEVICE_NAME);
    if (IS_ERR(vialab_class)) {
        err = PTR_ERR(vialab_class);
        goto fail;
    }
	
	/* Allocate the array of devices */
	vialab_devices = (struct vialab_dev *)kzalloc(
		vialab_ndevices * sizeof(struct vialab_dev), 
		GFP_KERNEL);
	if (vialab_devices == NULL) {
		err = -ENOMEM;
		goto fail;
	}
	
	/* Construct devices */
	for (i = 0; i < vialab_ndevices; ++i) {
		err = vialab_construct_device(&vialab_devices[i], i, vialab_class);
        if (err) {
            devices_to_destroy = i;
            goto fail;
        }
	}
	return 0; /* success */

fail:
	vialab_cleanup_module(devices_to_destroy);
	return err;
}

static void __exit
vialab_exit_module(void)
{
	vialab_cleanup_module(vialab_ndevices);
	return;
}

module_init(vialab_init_module);
module_exit(vialab_exit_module);
