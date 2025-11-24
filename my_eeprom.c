#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/fs.h>           // File operations (fops)
#include <linux/cdev.h>         // Character device structure
#include <linux/uaccess.h>      // copy_to/from_user
#include <linux/slab.h>         // kzalloc/kfree

#define DEVICE_NAME "my_eeprom"
#define CLASS_NAME  "my_eeprom_class"

/* * Custom Driver Structure
 * This structure holds all context data needed for our device.
 * It links the I2C client (hardware) with the cdev (software interface).
 */
struct my_eeprom_dev {
    struct i2c_client *client;  // Pointer to the I2C hardware client
    dev_t dev_num;              // Holds Major and Minor numbers
    struct class *class;        // Device Class (for sysfs/udev)
    struct cdev cdev;           // The Character Device structure
    struct device *device;      // The Device Node (/dev/xxx)
};

/* Global pointer to our device data (for simple single-device handling) */
static struct my_eeprom_dev *my_dev;

/* * Open Function
 * Called when user runs: open("/dev/my_eeprom", ...)
 * Goal: Link the file session to our specific device data.
 */
static int my_open(struct inode *inode, struct file *file)
{
    /* * Use container_of to retrieve our custom struct 'my_eeprom_dev' 
     * from the generic 'cdev' pointer stored in the inode.
     */
    struct my_eeprom_dev *dev = container_of(inode->i_cdev, struct my_eeprom_dev, cdev);
    
    /* * Save this device pointer into 'file->private_data'.
     * This ensures that read() and write() functions can access 
     * the correct I2C client later.
     */
    file->private_data = dev;
    
    pr_info("MY_DRIVER: Device opened\n");
    return 0;
}

/* * Read Function
 * Called when user runs: cat /dev/my_eeprom or read()
 * Logic: Perform a Random Read operation on the EEPROM using the file offset (*ppos).
 */
static ssize_t my_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
    /* Retrieve our device data from private_data */
    struct my_eeprom_dev *dev = file->private_data;
    struct i2c_client *client = dev->client;
    
    int ret;
    char *kbuf;
    struct i2c_msg msgs[2]; // We need 2 messages for Random Read protocol
    u8 addr_buf[2];         // Buffer for 16-bit address (24c64)

    /* Allocate temporary kernel buffer to store data read from hardware */
    kbuf = kzalloc(count, GFP_KERNEL);
    if (!kbuf) return -ENOMEM;

    /* * Prepare I2C Transaction
     * Step 1: Dummy Write. We need to tell the EEPROM which address to read from.
     * We use the current file position (*ppos) as the EEPROM memory address.
     */
    addr_buf[0] = (*ppos >> 8) & 0xFF; // High Byte of address
    addr_buf[1] = *ppos & 0xFF;        // Low Byte of address

    msgs[0].addr = client->addr;
    msgs[0].flags = 0;          // Write flag (0)
    msgs[0].len = 2;            // Sending 2 bytes (Address)
    msgs[0].buf = addr_buf;

    /* * Step 2: Read Data. After setting the address, we read the data.
     */
    msgs[1].addr = client->addr;
    msgs[1].flags = I2C_M_RD;   // Read flag
    msgs[1].len = count;        // Read 'count' bytes requested by user
    msgs[1].buf = kbuf;

    /* Execute the combined I2C transfer */
    ret = i2c_transfer(client->adapter, msgs, 2);
    if (ret < 0) {
        pr_err("MY_DRIVER: I2C Read failed\n");
        kfree(kbuf);
        return ret;
    }

    /* Copy data from Kernel Space to User Space */
    if (copy_to_user(user_buf, kbuf, count) != 0) {
        kfree(kbuf);
        return -EFAULT;
    }

    /* Update file position marker so the next read continues from here */
    *ppos += count;
    
    /* Free kernel buffer */
    kfree(kbuf);
    
    pr_info("MY_DRIVER: Read %zu bytes from offset 0x%04llx\n", count, *ppos - count);
    return count; // Return actual number of bytes read
}

/* * Write Function
 * Called when user runs: echo "..." > /dev/my_eeprom or write()
 * Logic: Write raw bytes directly to I2C. 
 * Note: The user MUST provide the address in the first 2 bytes: [AddrH][AddrL][Data...]
 */
static ssize_t my_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
    struct my_eeprom_dev *dev = file->private_data;
    struct i2c_client *client = dev->client;
    
    char *kbuf;
    int ret;

    /* Basic check: Need at least 2 bytes for address + 1 byte for data */
    if (count < 3) {
        pr_err("MY_DRIVER: Write data too short (need addr+data)\n");
        return -EINVAL;
    }
    
    /* Limit write size (Simple protection, max 32 bytes + 2 address bytes) */
    if (count > 34) count = 34; 

    /* Allocate kernel buffer */
    kbuf = kzalloc(count, GFP_KERNEL);
    if (!kbuf) return -ENOMEM;

    /* Copy data from User Space to Kernel Space */
    if (copy_from_user(kbuf, user_buf, count) != 0) {
        kfree(kbuf);
        return -EFAULT;
    }

    /* Send I2C data directly using master_send */
    ret = i2c_master_send(client, kbuf, count);
    if (ret < 0) {
        pr_err("MY_DRIVER: I2C Write failed\n");
        kfree(kbuf);
        return ret;
    }

    pr_info("MY_DRIVER: Wrote %d bytes to EEPROM\n", ret);
    
    kfree(kbuf);
    return count; // Return number of bytes written
}

/* File Operations Structure: Map system calls to our functions */
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .read = my_read,
    .write = my_write,
};

/* * Probe Function
 * Called when the driver matches a device (e.g., via new_device).
 * This function registers the Character Device interface.
 */
static int my_probe(struct i2c_client *client)
{
    int ret;
    pr_info("MY_DRIVER: Probe 0x%02x\n", client->addr);

    /* 1. Allocate memory for our custom structure */
    my_dev = kzalloc(sizeof(struct my_eeprom_dev), GFP_KERNEL);
    if (!my_dev) return -ENOMEM;

    my_dev->client = client; // Save the I2C client pointer

    /* 2. Request Major/Minor Number dynamically */
    ret = alloc_chrdev_region(&my_dev->dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("MY_DRIVER: Failed to alloc chrdev region\n");
        goto err_free;
    }

    /* 3. Create Device Class (creates /sys/class/my_eeprom_class) */
    my_dev->class = class_create(CLASS_NAME);
    if (IS_ERR(my_dev->class)) {
        pr_err("MY_DRIVER: Failed to create class\n");
        ret = PTR_ERR(my_dev->class);
        goto err_unregister;
    }

    /* 4. Initialize Character Device (Connect fops to cdev) */
    cdev_init(&my_dev->cdev, &fops);
    my_dev->cdev.owner = THIS_MODULE;
    
    /* 5. Add Character Device to the system */
    ret = cdev_add(&my_dev->cdev, my_dev->dev_num, 1);
    if (ret < 0) {
        pr_err("MY_DRIVER: Failed to add cdev\n");
        goto err_class;
    }

    /* 6. Create Device Node (creates /dev/my_eeprom automatically) */
    my_dev->device = device_create(my_dev->class, NULL, my_dev->dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(my_dev->device)) {
        pr_err("MY_DRIVER: Failed to create device\n");
        ret = PTR_ERR(my_dev->device);
        goto err_cdev;
    }

    pr_info("MY_DRIVER: /dev/%s created successfully\n", DEVICE_NAME);
    return 0;

/* Error Handling (Clean up in reverse order) */
err_cdev:
    cdev_del(&my_dev->cdev);
err_class:
    class_destroy(my_dev->class);
err_unregister:
    unregister_chrdev_region(my_dev->dev_num, 1);
err_free:
    kfree(my_dev);
    return ret;
}

/* * Remove Function
 * Called when rmmod or delete_device is executed
 */
static void my_remove(struct i2c_client *client)
{
    if (my_dev) {
        device_destroy(my_dev->class, my_dev->dev_num); // Remove /dev node
        cdev_del(&my_dev->cdev);                        // Remove cdev
        class_destroy(my_dev->class);                   // Remove class
        unregister_chrdev_region(my_dev->dev_num, 1);   // Release numbers
        kfree(my_dev);                                  // Free memory
    }
    pr_info("MY_DRIVER: Removed\n");
}

/* ID Table: Device names supported by this driver */
static const struct i2c_device_id my_id[] = { { "my_24c64", 0 }, { } };
MODULE_DEVICE_TABLE(i2c, my_id);

/* Driver Registration Structure */
static struct i2c_driver my_driver = {
    .driver = { .name = "my_eeprom_cdev_driver", .owner = THIS_MODULE },
    .probe = my_probe,
    .remove = my_remove,
    .id_table = my_id,
};

module_i2c_driver(my_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Brian");
MODULE_DESCRIPTION("Character Device Driver for I2C EEPROM");