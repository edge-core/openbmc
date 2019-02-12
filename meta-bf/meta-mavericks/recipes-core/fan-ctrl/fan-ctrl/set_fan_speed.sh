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
    maxnfans=10
    FANS="1 2 3 4 5 6 7 8 9 10"
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0033
    FAN_DIR_UPPER=/sys/class/i2c-adapter/i2c-9/9-0033
elif [ "$board_subtype" == "Montara" ]; then
    maxnfans=5
    FANS="1 2 3 4 5"
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0033
elif [ "$board_subtype" == "Newport" ]; then
    maxnfans=6
    FANS="1 2 3 4 5 6"
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0066
fi

usage() {
    if [ "$board_type" == "MAVERICKS" ]; then
        echo "Usage: $0 <PERCENT (0..100)> [Fan Unit (1..5)(upper: 6..10)] [board type (Montara Mavericks)]" >&2
    elif [ "$board_type" == "NEWPORT" ]; then
        echo "Usage: $0 <PERCENT (0..100)> [Fan Unit (1..6)] [board type (Newport)]" >&2
    fi
}

set -e

if [ "$#" -gt 3 ] || [ "$#" -lt 1 ]; then
    usage
    exit 1
fi

if [ $1 -gt 0 ] 2>/dev/null ; then
    if [ $1 -gt 100 ]; then
        usage
        exit 1
    fi
else
    usage
    exit 1
fi

# Adding fix for Mavericks, Montara and Newport while keeping backward compatibility with FB command line.
if [ "$#" -eq 2 ]; then
    if [ $2 = "Mavericks" ]; then
        if [ "$board_subtype" != "Mavericks" ]; then
            echo "Error: This is $board_subtype"
            exit 1
        fi
    elif [ $2 = "Montara" ]; then
        if [ "$board_subtype" != "Montara" ]; then
            echo "Error: This is $board_subtype"
            exit 1
        fi
    elif [ $2 = "Newport" ]; then
        if [ "$board_subtype" != "Newport" ]; then
            echo "Error: This is $board_subtype"
            exit 1
        fi
    elif [ $2 -gt 0 ] 2>/dev/null ; then
        if [ $2 -gt $maxnfans ]; then
            echo "Error: The max of fan unit is $maxnfans"
            exit 1
        fi
        FANS="$2"
    else
        usage
        exit 1
    fi
elif [ "$#" -eq 3 ]; then
    if [ $3 = "Mavericks" ]; then
        if [ "$board_subtype" != "Mavericks" ]; then
            echo "Error: This is $board_subtype"
            exit 1
        fi
    elif [ $3 = "Montara" ]; then
        if [ "$board_subtype" != "Montara" ]; then
            echo "Error: This is $board_subtype"
            exit 1
        fi
    elif [ $3 = "Newport" ]; then
        if [ "$board_subtype" != "Newport" ]; then
            echo "Error: This is $board_subtype"
            exit 1
        fi
    else
        usage
        exit 1
    fi

    if [ $2 -gt 0 ] 2>/dev/null ; then
        if [ $2 -gt $maxnfans ]; then
            echo "Error: The max of fan unit is $maxnfans"
            exit 1
        fi
        FANS="$2"
    else
        usage
        exit 1
    fi
fi

if [ "$board_type" == "MAVERICKS" ]; then
    # Convert the percentage to our 1/32th unit (0-31).
    unit=$(( ( $1 * 31 ) / 100 ))

    for fan in $FANS; do
        if [ $fan -gt 5 ]; then
            fan_idx=$(( $fan - 5))
            pwm="${FAN_DIR_UPPER}/fantray${fan_idx}_pwm"
        else
            pwm="${FAN_DIR}/fantray${fan}_pwm"
        fi
        echo "$unit" > $pwm
        echo "Successfully set fan ${fan} speed to $1%"
    done
elif [ "$board_type" == "NEWPORT" ]; then
    #For Newport, Convert the percentage to our 1/16th unit (0-15).
    unit=$(( ( $1 * 16 ) / 100 - 1 ))

    pwm="${FAN_DIR}/fantray_pwm"
    echo "$unit" > $pwm
    echo "Successfully set fan speed to $1%"
fi
