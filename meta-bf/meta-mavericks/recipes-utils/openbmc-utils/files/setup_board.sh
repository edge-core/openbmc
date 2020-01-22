#!/bin/sh
#
# Copyright 2014-present Facebook. All Rights Reserved.
#
# This program file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program in a file named COPYING; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301 USA

### BEGIN INIT INFO
# Provides:          setup_board.sh
# Required-Start:
# Required-Stop:
# Default-Start:     S
# Default-Stop:
# Short-Description: Setup the board
### END INIT INFO

. /usr/local/bin/openbmc-utils.sh

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin

board_rev=$(wedge_board_rev)
board_type=$(wedge_board_type)

if [ $board_rev -ge 2 ]; then
    # DVT or later
    # Enable the isolation buffer between BMC and COMe i2c bus
    echo 1 > ${SYSCPLD_SYSFS_DIR}/com-e_i2c_isobuf_en
    # Enable the COM6_BUF_EN to open the isolation buffer between COMe BIOS
    # EEPROM with COMe
    gpio_set COM6_BUF_EN 0
    # Make the BIOS EEPROM connect to COMe instead of BMC
    gpio_set COM_SPI_SEL 0
fi

# To enhance role of 'admin/admin' for 'sol.sh' execution.
find /etc -name passwd | xargs -i sed -i 's/1001/0/g' {}

# For AST1250, it should use this pin as LPC reset input. And set hardware
# strapping bit14 to ’1’. Defined on ast2400v13.pdf.
devmem 0x1e6e2070 32 0x0A0845D2

# Reinstall PSU driver to support 'sensors' because PCA9548 is not selected/controlled when booting.
# BMC-bus7-PCA9548(0x70)-channel1&2-PSU1&PSU2
i2cset -f -y 7 0x70 0x3
rmmod psu_driver
modprobe psu_driver

