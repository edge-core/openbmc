#!/bin/sh
PROGRAM_TITLE=`basename $0`

usage() {
    echo "Usage: $PROGRAM_TITLE  <bios firmware name>"
    echo "This script is only used to update external BIOS"
    echo "Examples:"
    echo "      $PROGRAM_TITLE PCOM-B634VG-BAREFOOT_R100E8.BIN"
}

if [[ $# -ne 1 ]]; then
    usage
    exit -1
fi

boot_area=$(bmc_info.sh bios | grep Boot | awk '{print $5}' )
echo "Current Boot Code Source: $boot_area"

if [ "x$boot_area" != "xmaster" ];then
	echo "Fail: COMe uses external BIOS to boot, can't upgrade external BIOS"
	exit -1
fi

# Make sure GPIO SWITCH_EEPROM1_WRT is low. In the case, those SPI pins of BMC are not connected to the SWITCH EEPROM.
cat /tmp/gpionames/SWITCH_EEPROM1_WRT/direction
cat /tmp/gpionames/SWITCH_EEPROM1_WRT/value

# Make sure SPI pins are configured as SPI instead of GPIO.
source /usr/local/bin/openbmc-utils.sh
devmem_set_bit $(scu_addr 70) 12
openbmc_gpio_util.py dump | grep SPI

# Make sure external BIOS flash connects to BMC instead of COMe
gpio_set COM_SPI_SEL 1
gpio_set COM6_BUF_EN 0

# Create dev node for spidev
[ -c /dev/spidev5.0 ] || mknod /dev/spidev5.0 c 153 0

# Load spidev module
modprobe spidev
flashrom -p linux_spi:dev=/dev/spidev5.0

# read bios flash
flashrom -p linux_spi:dev=/dev/spidev5.0 -r bios

#write bios flash
flashrom -V -p linux_spi:dev=/dev/spidev5.0 -w $1

echo "bios upgrade success"