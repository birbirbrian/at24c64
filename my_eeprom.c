#include<linux/module.h>        // micro MODULE_LICENSE
#include<linux/init.h>          // module_init/exit
#include<linux/i2c.h>           // struct i2c_client, i2c_driver
#include<linux/kernel.h>        // pr_info

/* Probe function 
 * When a device address is added through new_device and it's name is the same as
 * name in id_table. This function will be called by kernel.
 */
 static int my_eeprom_probe(struct i2c_client *client) {
         
        pr_info("MY_EEPROM: Porbe function called!!!\n");
        pr_info("MY_EEPROM: Device found at addr: 0x%02x\n", client->addr);
        pr_info("MY_EEPROM: Match ID name: %s\n", client->name);
        return 0;
        }

/* Remove function 
 * When rmmod or delete_device called, this function trigger.
 */
 static void my_eeprom_remove(struct i2c_client *client) {
         
        pr_info("MY_EEPROM: Remove function called for addr: 0x%02x\n", client->addr);
        pr_info("MY_EEPROM: Goodbye\n");
        }

/* ID Table
 * This struct tells kernel what device name this driver support
 */
static const struct i2c_device_id my_eeprom_id[] = {
        {"my_24c64", 0},
        {}
        };

/* Regist in i2c subsystem
 */
MODULE_DEVICE_TABLE(i2c, my_eeprom_id);

/* Driver struct
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

/* Regist micro
 */
 module_i2c_driver(my_eeprom_driver);
 
 /* Module information
  */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Brian Yeh");
MODULE_DESCRIPTION("A simple hello world i2c driver kernel 6.x+)");
