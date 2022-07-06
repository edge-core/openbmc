#!/bin/sh

eeprom="/sys/bus/i2c/devices/7-0050/eeprom"

cd /tmp/gpionames/
cd SWITCH_EEPROM1_WRT
echo out > direction
echo 1 > value
cat direction value

i2cset -f -y 7 0x70 4

if [ -e $eeprom ]; then
    cat $eeprom | hexdump -C > /tmp/old
	dd if=/home/root/$1 of=$eeprom 
	cat $eeprom | hexdump -C > /tmp/new
	
	echo "write eeprom success"

else
	echo "eeprom driver not probe"
fi

