#!/bin/bash
#
# Copyright 2015-present Facebook. All Rights Reserved.
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
#

. /usr/local/bin/openbmc-utils.sh

PWR_TF_RST_SYSFS="${SYSCPLD_SYSFS_DIR}/tofino_pwr_on_rst_n"
echo "POR to Tofino...."

# gracefully reduce Tofino-2 power current before toggling reset
board_subtype=$(wedge_board_subtype)
if [ "$board_subtype" == "Newport" ] ; then
# write 0x88ff1800 to pps_pll_ctrl0_address = 0x80018
    i2c_set_get 11 0x58 9 0 0x80 0x18 0x00 0x08 0x00 0x00 0x18 0xff 0x88
    usleep 50
# write 0x88d71800 to core_pll_ctrl0_address = 0x80020
    i2c_set_get 11 0x58 9 0 0x80 0x20 0x00 0x08 0x00 0x00 0x18 0xd7 0x88
# write 0x891f1801 to mac0_pll_ctrl0_address = 0x80028
    i2c_set_get 11 0x58 9 0 0x80 0x28 0x00 0x08 0x00 0x01 0x18 0x1f 0x89
# write 0x891f1801 to mac1_pll_ctrl0_address = 0x80030
    i2c_set_get 11 0x58 9 0 0x80 0x30 0x00 0x08 0x00 0x01 0x18 0x1f 0x89
    usleep 10
fi
echo 0 > $PWR_TF_RST_SYSFS
usleep 10000
echo 1 > $PWR_TF_RST_SYSFS

logger "Reset Tofino"
