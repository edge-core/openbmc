#!/bin/sh
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

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin

board_type=$(wedge_board_type)
board_subtype=$(wedge_board_subtype)

if [ "$board_subtype" == "Mavericks" ]; then
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0033
    FAN_DIR_UPPER=/sys/class/i2c-adapter/i2c-9/9-0033
elif [ "$board_subtype" == "Montara" ]; then
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0033
elif [ "$board_type" == "NEWPORT" ]; then
    FANS="1 2 3 4 5 6"
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0066
elif [ "$board_type" == "DAVENPORT" ]; then
    FANS="1 2 3 4 5 6"
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0066
elif [ "$board_type" == "STINSON" ]; then
    FANS="1 2 3 4 5 6 7"
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0066
fi

show_present()
{
    if [ "$board_subtype" == "Mavericks" ]; then
        fantray_pres="${1}/fantray_present"
    elif [ "$board_subtype" == "Montara" ]; then
        fantray_pres="${FAN_DIR}/fantray_present"
    elif [ "$board_type" == "NEWPORT" ] || [ "$board_type" == "STINSON" ] || [ "$board_type" == "DAVENPORT" ] ; then
        fantray_pres="${FAN_DIR}/fantray${1}_present"
    fi
    echo "$(cat $fantray_pres)"
}

set -e

if [ "$board_subtype" == "Mavericks" ]; then
    # refer to the comments in init_pwn.sh regarding
    echo "Fantray present: $(show_present ${FAN_DIR})"
    echo "Fantray_upper present: $(show_present ${FAN_DIR_UPPER})"
elif [ "$board_subtype" == "Montara" ]; then
    echo "Fantray present: $(show_present)"
elif [ "$board_type" == "NEWPORT" ] || [ "$board_type" == "STINSON" ] || [ "$board_type" == "DAVENPORT" ] ; then
    for fan in $FANS; do
        echo "Fan $fan present: $(show_present $fan)"
    done
fi