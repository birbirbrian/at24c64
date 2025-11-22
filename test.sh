#!/bin/bash

export I2C_PATH="/sys/bus/i2c/devices/i2c-1"

echo "#####build driver#####"
make
echo "#####build driver#####"
echo ""

echo "#####clean dmesg#####"
echo "0x50" | sudo tee $I2C_PATH/delete_device 2>/dev/null
sudo rmmod at24 2>/dev/null
sudo rmmod my_eeprom 2>/dev/null
sudo dmesg -C
echo "#####clean dmesg#####"
echo ""

echo "#####insmod driver#####"
sudo insmod my_eeprom.ko
echo "my_24c64 0x50" | sudo tee $I2C_PATH/new_device
echo "#####insmod driver#####"
echo ""

echo "#####check dmesg#####"
dmesg
i2cdetect -y 1
echo "#####check dmesg#####"
echo ""

echo "#####Trigger write#####"
echo "1" | sudo tee /sys/bus/i2c/devices/1-0050/my_test_write
echo "#####Trigger write#####"
echo ""

echo "#####check write#####"
dmesg | tail
echo "check by i2ctransfer"
sudo i2ctransfer -y -f 1 w2@0x50 0x00 0x00 r1
echo "#####check write#####"
echo ""

#####remove test#####
echo "0x50" | sudo tee $I2C_PATH/delete_device
sudo rmmod my_eeprom

