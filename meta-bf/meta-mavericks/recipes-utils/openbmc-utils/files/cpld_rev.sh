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

board_type=$(wedge_board_type)
board_subtype=$(wedge_board_subtype)

if [ "$board_subtype" == "Mavericks" ]; then
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0033
    FAN_DIR_UPPER=/sys/class/i2c-adapter/i2c-9/9-0033
    SYS_DIR=/sys/class/i2c-adapter/i2c-12/12-0031
    SYS_DIR_UPPER=/sys/class/i2c-adapter/i2c-12/12-0030
elif [ "$board_subtype" == "Montara" ]; then
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0033
    SYS_DIR=/sys/class/i2c-adapter/i2c-12/12-0031
elif [ "$board_subtype" == "Newport" ]; then
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0066
    SYS_DIR=/sys/class/i2c-adapter/i2c-12/12-0031
elif [ "$board_subtype" == "Stinson" ]; then           
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0066        
    SYS_DIR=/sys/class/i2c-adapter/i2c-12/12-0031
elif [ "$board_subtype" == "Davenport" ]; then   
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0066  
    SYS_DIR=/sys/class/i2c-adapter/i2c-12/12-0031
elif [ "$board_subtype" == "Pescadero" ]; then   
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0066  
    SYS_DIR=/sys/class/i2c-adapter/i2c-12/12-0031
fi

usage() {
    echo "Usage: ${0} <target board (upper lower)> <target cpld (sys fan)>" >&2
}

# Check the number of arguments provided
if [ $# -ne 2 ]; then
    usage
    exit 1
fi

if [ $1 != "upper" ] && [ $1 != "lower" ]; then
    usage
    exit 1
fi
if [ $2 != "sys" ] && [ $2 != "fan" ]; then
    usage
    exit 1
fi

# 2U: Mavericks, 1U: Montara, Newport
if [ $1 == "upper" ] && [ "$board_subtype" != "Mavericks" ]; then
    echo "upper board does not exist"
    exit 1
fi

# Check the file name and upgrade accordingly
if [ $1 == "upper" ]; then
    if [ $2 == "sys" ]; then
        rev="${SYS_DIR_UPPER}/cpld_rev"
        sub_rev="${SYS_DIR_UPPER}/cpld_sub_rev"
    else
        rev="${FAN_DIR_UPPER}/cpld_rev"
        sub_rev="${FAN_DIR_UPPER}/cpld_sub_rev"
    fi
elif [ $1 == "lower" ]; then
    if [ $2 == "sys" ]; then
        rev="${SYS_DIR}/cpld_rev"
        sub_rev="${SYS_DIR}/cpld_sub_rev"
    else
        rev="${FAN_DIR}/cpld_rev"
        sub_rev="${FAN_DIR}/cpld_sub_rev"
    fi
else
  usage
  exit 1
fi

# According to fancpld.c definition
if [ "$board_type" == "NEWPORT" ] && [ $2 == "fan" ]; then
    rev_hex2dec=$(cat $rev)
    echo $(($rev_hex2dec))
elif [ "$board_type" == "STINSON" ] && [ $2 == "fan" ]; then
    rev_hex2dec=$(cat $rev)
    echo $(($rev_hex2dec))
elif [ "$board_type" == "DAVENPORT" ] && [ $2 == "fan" ]; then
    rev_hex2dec=$(cat $rev)
    echo $(($rev_hex2dec))
elif [ "$board_type" == "PESCADERO" ] && [ $2 == "fan" ]; then
    rev_hex2dec=$(cat $rev)
    echo $(($rev_hex2dec))
else
    rev_hex2dec=$(cat $rev)
    sub_rev_hex2dec=$(cat $sub_rev)
    echo $(($rev_hex2dec)).$(($sub_rev_hex2dec))
fi

