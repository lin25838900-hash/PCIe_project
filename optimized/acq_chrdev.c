#include "pcie_acq_optimized.h"

static int acq_open(struct inode *inode, struct file *filp)
{
    filp->private_data = g_acq_dev;
    return 0;
}

static ssize_t acq_read(struct file *filp, char __user *buf,
                        size_t count, loff_t *ppos)
{
    struct acq_device *dev = filp->private_data;
    size_t total = NUM_CHANNELS * CHANNEL_SIZE;
    int i;

    if (count < total)
        return -EINVAL;

    if (wait_event_interruptible(dev->wait_queue, dev->data_available))
        return -ERESTARTSYS;

    spin_lock(&dev->lock);
    dev->data_available = false;
    spin_unlock(&dev->lock);

    for (i = 0; i < NUM_CHANNELS; i++) {
        if (copy_to_user(buf + i * CHANNEL_SIZE,
                         dev->channel[i].buffer, CHANNEL_SIZE))
            return -EFAULT;
    }

    return total;
}

#define ACQ_IOC_MAGIC   'A'
#define ACQ_IOC_START   _IO(ACQ_IOC_MAGIC, 1)
#define ACQ_IOC_STOP    _IO(ACQ_IOC_MAGIC, 2)

static long acq_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct acq_device *dev = filp->private_data;

    switch (cmd) {
    case ACQ_IOC_START:
        iowrite32(CTRL_ENABLE, dev->regs + REG_CTRL);
        break;
    case ACQ_IOC_STOP:
        iowrite32(0, dev->regs + REG_CTRL);
        break;
    default:
        return -ENOTTY;
    }

    return 0;
}

static const struct file_operations acq_fops = {
    .owner = THIS_MODULE,
    .open = acq_open,
    .read = acq_read,
    .unlocked_ioctl = acq_ioctl,
};

int acq_chrdev_init(struct acq_device *dev)
{
    int ret;

    ret = alloc_chrdev_region(&dev->devno, 0, 1, DEVICE_NAME);
    if (ret)
        return ret;

    cdev_init(&dev->cdev, &acq_fops);
    ret = cdev_add(&dev->cdev, dev->devno, 1);
    if (ret)
        goto err_unreg;

    dev->class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(dev->class)) {
        ret = PTR_ERR(dev->class);
        dev->class = NULL;
        goto err_cdev;
    }

    dev->device = device_create(dev->class, NULL, dev->devno, NULL,
                                DEVICE_NAME);
    if (IS_ERR(dev->device)) {
        ret = PTR_ERR(dev->device);
        dev->device = NULL;
        goto err_class;
    }

    return 0;

err_class:
    if (dev->class)
        class_destroy(dev->class);
    dev->class = NULL;
err_cdev:
    cdev_del(&dev->cdev);
err_unreg:
    unregister_chrdev_region(dev->devno, 1);
    return ret;
}

void acq_chrdev_cleanup(struct acq_device *dev)
{
    if (dev->device)
        device_destroy(dev->class, dev->devno);
    if (dev->class)
        class_destroy(dev->class);

    cdev_del(&dev->cdev);
    unregister_chrdev_region(dev->devno, 1);

    dev->device = NULL;
    dev->class = NULL;
}
