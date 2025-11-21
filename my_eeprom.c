#include<linux/module.h>        // micro MODULE_LICENSE
#include<linux/init.h>          // module_init/exit
#include<linux/i2c.h>           // struct i2c_client, i2c_driver
#include<linux/kernel.h>        // pr_info
#include<linux/sysfs.h>         // sysfs_create_file
#include<linux/device.h>        // device_create_file 

/* * Sysfs callback function
 * When user read the sysfs file, this function will be called.
 */
static ssize_t my_test_write_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {

    struct i2c_client *client = to_i2c_client(dev);
    int ret;
    
    /* * Prepare Transaction Buffer
     * 24c64 (8KB) is 2 bytes (16-bit) address
     * send sequence high byte -> low byte -> data
     */
    u8 tx_buf[3];
    
    tx_buf[0] = 0x00; // Memory Address High
    tx_buf[1] = 0x00; // Memory Address Low  -> address 0x0000
    tx_buf[2] = 0xAB; // Data (0xAB)

    pr_info("MY_EEPROM: [Write Test] User triggered write!\n");
    pr_info("MY_EEPROM: Writing Data(0xAB) to Address(0x0000)...\n");

    /* * Core I2C send function
     * client: I2C device instance
     * tx_buf: transaction buffer
     * 3: length (2 byte addr + 1 byte data)
     */
    ret = i2c_master_send(client, tx_buf, 3);
    
    if (ret < 0) {
        pr_err("MY_EEPROM: I2C Write failed! Error code: %d\n", ret);
        return ret; // return errpr
    }

    pr_info("MY_EEPROM: Write success! Sent %d bytes to hardware.\n", ret);
    
    // because this function is trigger by echo, echo need a return value
    return count;
}

/* * Define Sysfs file attribute 
 * file name: my_test_write (/sys/bus/i2c/device/xxxx/"file name")
 * Authority: 0200 (S_IWUSR - only owner can write)
 * Show function: NULL (no read access)
 * Store function: my_test_write_store (callback function when write is trigger)
 */
static DEVICE_ATTR(my_test_write, 0200, NULL, my_test_write_store);

/* * Probe function 
 * When a device address is added through new_device and it's name is the same as
 * name in id_table. This function will be called by kernel.
 */
static int my_eeprom_probe(struct i2c_client *client){
    int ret;
    pr_info("MY_EEPROM: Probe called! Found device at 0x%02x\n", client->addr);

    /* * create our write method under sysfs
     * device_create_file will put our my_test_write structure under sysfs
     * The path will be like： /sys/bus/i2c/devices/1-0050/my_test_write
     */
    ret = device_create_file(&client->dev, &dev_attr_my_test_write);
    if (ret) {
        pr_err("MY_EEPROM: Failed to create sysfs file\n");
        return ret;
    }
    
    pr_info("MY_EEPROM: Sysfs interface 'my_test_write' created successfully.\n");
    return 0;
}

/* * Remove function 
 * When rmmod or delete_device called, this function trigger.
 */
static void my_eeprom_remove(struct i2c_client *client) {     
    // remove file under sysfs to avoid crash
    device_remove_file(&client->dev, &dev_attr_my_test_write);
    pr_info("MY_EEPROM: Sysfs file removed. Goodbye!\n");
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
        .probe = my_eeprom_probe,
        .remove = my_eeprom_remove,
        .id_table = my_eeprom_id,
        };

/* * Regist micro
 */
module_i2c_driver(my_eeprom_driver);
 
 /* * Module information
  */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Brian Yeh");
MODULE_DESCRIPTION("A simple hello world i2c driver kernel 6.x+)");
