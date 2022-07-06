#!/bin/sh

eeprom="/sys/bus/i2c/devices/6-0050/eeprom"

cd /tmp/gpionames/
cd SWITCH_EEPROM1_WRT
echo out > direction
echo 1 > value
cat direction value

if [ -e $eeprom ]; then
    cat $eeprom | hexdump -C > /tmp/old_eeprom
	dd if=/home/root/$1 of=$eeprom 
	cat $eeprom | hexdump -C > /tmp/new_eeprom
	
    echo "write eeprom success"

else
	echo "eeprom driver not probe"
fi

