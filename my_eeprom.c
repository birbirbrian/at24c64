#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#define DEVICE_NAME "my_eeprom"
#define CLASS_NAME "my_eeprom_class"

/* * Global variables
 */
static dev_t dev_num;           // major/minor number
static struct class *my_class;  // device class
static struct cdev my_cdev;     // handle for this device

/* * Open function
 */
static int my_open(struct inode *inode, struct file *file) {
    pr_info("MY_DRIVER: Device opened\n");
    return 0;
}

/* * Read function
 * When user use cat, this will reture the hardcore string
 * file: who open me
 * use_buf: buffer that will going to show to user space on (user space address)
 * count: how many data user want to read
 * ppos: more lake a offset, it is record to make the process begin.(current position)
 */
static ssize_t my_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) {
    int ret = 0;
    
    // Hardcoded data
    char *data = "Hello! This is hardcoded data.\n";
    size_t data_len = strlen(data);
    
    // check if is exceed (EOF)
    if(*ppos >= data_len) return 0;  // 【修正】用 >= 比較安全
    
    // check if the length user want to read will exceed the max memory size
    if(count > data_len - *ppos) {
        count = data_len - *ppos;
    }

    // start to copy the data from kernel to user
    if(copy_to_user(user_buf, data + *ppos, count) != 0) {
        ret = -EFAULT;
        return ret;
    }
    
    // update position
    *ppos += count;
    
    ret = count;
    return ret;
}    

/* * Write function
 * When user use write() or echo, this function will be called
 * user_buf: data user want to write
 * count: data length
 */static ssize_t my_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos) {
    // max write size
    char kbuf[128];
    int ret = 0;
    
    // avoid buffer overflow
    // reserve 1 byte for '\0'
    if(count > sizeof(kbuf) - 1) {
        count = sizeof(kbuf) - 1;
    }
    
    // copy data
    if(copy_from_user(kbuf, user_buf, count) != 0) {
        ret = -EFAULT;
        return ret;
    }
    
    kbuf[count] = '\0';
    
    pr_info("MY_DRIVER: User wrote: %s\n", kbuf);
    
    // tell user how many data we save
    return count;
}

// define file operation
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .read = my_read,
    .write = my_write,
};


/* * Probe function 
 * When a device address is added through new_device and it's name is the same as
 * name in id_table. This function will be called by kernel.
 */
static int my_probe(struct i2c_client *client){
    int ret = 0;
    
    pr_info("MY_EEPROM: Probe called! Found device at 0x%02x\n", client->addr);
    
    // request device driver major/minor number
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if(ret < 0) {
        pr_err("MY_EEPROM: Fail to alloc chardev region\n");
        return ret;
    }
    
    // create device class
    // for kernel 6.x 
    my_class = class_create(CLASS_NAME);
    if (IS_ERR(my_class)) {
        pr_err("MY_DRIVER: Failed to create class\n");
        ret = PTR_ERR(my_class);
        goto err_unregister;
    }

    // init c dev
    cdev_init(&my_cdev, &fops);
    
    // add c dev to kernel
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("MY_DRIVER: Failed to add cdev\n");
        goto err_class;
    }

    // create device node in /dev
    if (IS_ERR(device_create(my_class, NULL, dev_num, NULL, DEVICE_NAME))) {
        pr_err("MY_DRIVER: Failed to create device\n");
        ret = -1;
        goto err_cdev;
    }

    pr_info("MY_DRIVER: Success! /dev/%s created.\n", DEVICE_NAME);
    return 0;

// goto error handlers
err_cdev:
    cdev_del(&my_cdev);
err_class:
    class_destroy(my_class);
err_unregister:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

/* * Remove function 
 * When rmmod or delete_device called, this function trigger.
 */
static void my_remove(struct i2c_client *client)
{
    device_destroy(my_class, dev_num);     // delete /dev node
    cdev_del(&my_cdev);                    // delete cdev
    class_destroy(my_class);               // delete class
    unregister_chrdev_region(dev_num, 1);  // uregister major/minor number
    pr_info("MY_DRIVER: Removed!\n");
}

/* * ID Table
 * This struct tells kernel what device name this driver support
 * When echo "xxx" > /sys/xxx/xxx/xxx/new_device, kernel will compare this string
 */
static const struct i2c_device_id my_eeprom_id[] = {
        {"my_24c64", 0},
        {}
};

/* * Regist in i2c subsystem
 */
MODULE_DEVICE_TABLE(i2c, my_eeprom_id);

/* * Driver struct
 * Pack Probe, Remove, ID table in a object
 */
static struct i2c_driver my_eeprom_driver = {
        .driver = {
                .name = "my_eeprom_driver",
                .owner = THIS_MODULE,
        },
        .probe = my_probe,
        .remove = my_remove,
        .id_table = my_eeprom_id,
};

/* * Regist micro
 */
module_i2c_driver(my_eeprom_driver);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Brian Yeh");
MODULE_DESCRIPTION("A simple hello world i2c driver kernel 6.x+");