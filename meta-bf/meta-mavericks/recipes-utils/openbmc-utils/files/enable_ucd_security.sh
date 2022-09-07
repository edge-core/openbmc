#!/bin/bash

#echo 2-0034 > /sys/bus/i2c/drivers/ucd9000/unbind

sleep 1

value=$(i2ctransfer -f -y 2 w1@0x34 0xf1 r7 | awk '{print $7}')

sleep 1

if [ "$value" = "0x00" ]
then
    i2ctransfer -f -y 2 w8@0x34 0xf1 0x6 0x41 0x43 0x43 0x54 0x4F 0x4E
    sleep 1
    value=$(i2ctransfer -f -y 2 w1@0x34 0xf1 r7 | awk '{print $7}')
    sleep 1
    i2cset -f -y 2 0x34 0x11

    if [ "$value" = "0x01" ]
    then
        echo "Enable UCD Security and save successful."
    else
        echo "Enable UCD Security and save failed."
    fi

else
    echo "UCD Security is already enabled."
fi

sleep 1

#echo 2-0034 > /sys/bus/i2c/drivers/ucd9000/bind
