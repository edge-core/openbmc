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
. /usr/local/bin/openbmc-utils.sh

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin

platform=$(btools.py -F 2> /dev/null | head -n 1)

if [ "$platform" == "mavericks" ] || [ "$platform" == "montara" ] || [ "$platform" == "newport" ] || [ "$platform" == "stinson" ]; then
    if [ -d /sys/bus/i2c/drivers/lm75/9-004a ]; then
        echo 9-004a > /sys/bus/i2c/drivers/lm75/unbind
    fi
    if [ -d /sys/bus/i2c/drivers/lm75/9-004b ]; then
        echo 9-004b > /sys/bus/i2c/drivers/lm75/unbind
    fi
fi
