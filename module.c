#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>

#include "lfring.h"

MODULE_LICENSE("GPL");

#define DEVICE_NAME "KLFRING"

static dev_t dev = -1;
static struct cdev klfring_cdev;
static struct class *klfring_class = NULL;
static lfring_t *rb;

#define EXPECT(exp)       \
    {                     \
        if (!(exp))       \
            return false; \
    }

static bool test_ringbuffer(uint32_t flags)
{
    void *vec[4];
    uint32_t ret;
    uint32_t idx;

    lfring_t *rb = lfring_alloc(2, flags);
    EXPECT(rb != NULL);

    ret = lfring_dequeue(rb, vec, 1, &idx);
    EXPECT(ret == 0);
    ret = lfring_enqueue(rb, (void *[]){(void *) 1}, 1);
    EXPECT(ret == 1);

    ret = lfring_dequeue(rb, vec, 1, &idx);
    EXPECT(ret == 1);
    EXPECT(idx == 0);
    EXPECT(vec[0] == (void *) 1);

    ret = lfring_dequeue(rb, vec, 1, &idx);
    EXPECT(ret == 0);

    ret = lfring_enqueue(rb, (void *[]){(void *) 2, (void *) 3, (void *) 4}, 3);
    EXPECT(ret == 2);

    ret = lfring_dequeue(rb, vec, 1, &idx);
    EXPECT(ret == 1);
    EXPECT(idx == 1);
    EXPECT(vec[0] == (void *) 2);

    ret = lfring_dequeue(rb, vec, 4, &idx);
    EXPECT(ret == 1);
    EXPECT(idx == 2);
    EXPECT(vec[0] == (void *) 3);

    lfring_free(rb);
    return true;
}


static int klfring_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int klfring_release(struct inode *inode, struct file *file)
{
    return 0;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = klfring_open,
    .release = klfring_release,
};

static int __init klfring_init(void)
{
    int ret = -1;
    struct device *device;

    if ((ret = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME)) < 0)
        return ret;

    klfring_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(klfring_class)) {
        ret = PTR_ERR(klfring_class);
        goto error_unregister_chrdev_region;
    }

    device = device_create(klfring_class, NULL, dev, NULL, DEVICE_NAME);
    if (IS_ERR(device)) {
        ret = PTR_ERR(device);
        goto error_class_destroy;
    }

    cdev_init(&klfring_cdev, &fops);
    if ((ret = cdev_add(&klfring_cdev, dev, 1)) < 0)
        goto error_device_destroy;

    rb = lfring_alloc(2, LFRING_FLAG_MP | LFRING_FLAG_MC);
    if (rb == NULL)
        goto error_cdev_del;

    printk(KERN_INFO DEVICE_NAME ": loaded\n");

    printk("testing SPMC lock-free ring\n");
    if (!test_ringbuffer(LFRING_FLAG_SP | LFRING_FLAG_MC)) {
        printk(KERN_INFO "Failed to pass the SPMC buffer test!");
        return -EIO;
    }

    printk("testing SPSC lock-free ring\n");
    if (!test_ringbuffer(LFRING_FLAG_SP | LFRING_FLAG_SC)) {
        printk(KERN_INFO "Failed to pass the SPSC buffer test!");
        return -EIO;
    }

    printk(KERN_INFO "Success to pass the buffer test!");
    return 0;

error_cdev_del:
    cdev_del(&klfring_cdev);
error_device_destroy:
    device_destroy(klfring_class, dev);
error_class_destroy:
    class_destroy(klfring_class);
error_unregister_chrdev_region:
    unregister_chrdev_region(dev, 1);
    return ret;
}

static void __exit klfring_exit(void)
{
    lfring_free(rb);
    device_destroy(klfring_class, dev);
    cdev_del(&klfring_cdev);
    class_destroy(klfring_class);
    unregister_chrdev_region(dev, 1);
    printk(KERN_INFO DEVICE_NAME ": unloaded\n");
}

module_init(klfring_init);
module_exit(klfring_exit);
