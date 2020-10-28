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

echo 0 > $PWR_TF_RST_SYSFS
usleep 10000
echo 1 > $PWR_TF_RST_SYSFS

if [ "$board_subtype" == "Newport" ] ; then
    # sequence is important
    wedge_ucd_gpio_set 21 0
    wedge_ucd_gpio_set 20 0
    wedge_ucd_gpio_set 13 0
    wedge_ucd_gpio_set 12 0
    wedge_ucd_gpio_set 22 0
fi
logger "Reset Tofino"
