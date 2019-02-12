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
        echo "Usage: $0 [Fan Unit (1..5)(upper: 6..10)] [board type (Mavericks Montara)]" >&2
    elif [ "$board_type" == "NEWPORT" ]; then
        echo "Usage: $0 [Fan Unit (1..6)] [board type (Newport)]" >&2
    fi
}

# Convert the percentage to our 1/32th unit (0-31).
show_pwm()
{
    if [ $1 -gt 5 ]; then
        fan=$(( $1 - 5 ))
        pwm="${FAN_DIR_UPPER}/fantray${fan}_pwm"
    else
        pwm="${FAN_DIR}/fantray${1}_pwm"
    fi
    val=$(cat $pwm | head -n 1)
    echo "$((val * 100 / 31))%"
}

show_rpm()
{
    if [ $1 -gt 5 ]; then
        front_rpm="${FAN_DIR_UPPER}/fan$(((($1 - 5) * 2 - 1)))_input"
        rear_rpm="${FAN_DIR_UPPER}/fan$(((($1 - 5) * 2)))_input"
    else
        front_rpm="${FAN_DIR}/fan$((($1 * 2 - 1)))_input"
        rear_rpm="${FAN_DIR}/fan$((($1 * 2)))_input"
    fi
    echo "$(cat $front_rpm), $(cat $rear_rpm)"
}

#For Newport, Convert the percentage to our 1/16th unit (0-15).
show_pwm_newport()
{
    pwm="${FAN_DIR}/fantray_pwm"
    val=$(cat $pwm | head -n 1)

    # According to Fan_board_CPLD_Specification,
    # 0000(0): 0x6.25% = 0% duty cycle      1000(8): 9x6.25% = 56.25% duty cycle
    # 0001(1): 5x6.25% = 31.25% duty cycle  1001(9): 10x6.25% = 62.50% duty cycle
    # 0010(2): 5x6.25% = 31.25% duty cycle  1010(10): 11x6.25% = 68.75% duty cycle
    # 0011(3): 5x6.25% = 31.25% duty cycle  1011(11): 12x6.25% = 75.00% duty cycle
    # 0100(4): 5x6.25% = 31.25% duty cycle  1100(12): 13x6.25% = 81.25% duty cycle
    # 0101(5): 6x6.25% = 37.50% duty cycle  1101(13): 14x6.25% = 87.50% duty cycle
    # 0110(6): 7x6.25% = 43.75% duty cycle  1110(14): 15x6.25% = 93.75% duty cycle
    # 0111(7): 8x6.25% = 50.00% duty cycle  1111(15): 16x6.25% = 100.0% duty cycle
    # The fan module PWM is the same if register value is between 1 and 4.
    if [[ $val -gt 0 ]]; then
        if [[ $val -lt 4 ]]; then
            val=4
        fi
        val=$(( val + 1 ))
    fi

    echo "$((val * 100 / 16))%"
}

show_rpm_newport()
{
    front_rpm="${FAN_DIR}/fan$((($1 * 2 - 1)))_input"
    rear_rpm="${FAN_DIR}/fan$((($1 * 2)))_input"
    echo "$(cat $front_rpm), $(cat $rear_rpm)"
}

set -e

if [ "$#" -gt 2 ]; then
    usage
    exit 1
fi

# refer to the comments in init_pwn.sh regarding
# the fan unit and tacho mapping
# fixing function to handle mavericks, montara and newport number of fans
# while keeping backward command line compatibility with original FB script
if [ "$#" -eq 1 ]; then
    if [ $1 = "Mavericks" ]; then
        if [ "$board_subtype" != "Mavericks" ]; then
            echo "Error: This is $board_subtype"
            exit 1
        fi
    elif [ $1 = "Montara" ]; then
        if [ "$board_subtype" != "Montara" ]; then
            echo "Error: This is $board_subtype"
            exit 1
        fi
    elif [ $1 = "Newport" ]; then
        if [ "$board_subtype" != "Newport" ]; then
            echo "Error: This is $board_subtype"
            exit 1
        fi
    elif [ $1 -gt 0 ] 2>/dev/null ; then
        if [ $1 -gt $maxnfans ]; then
            echo "Error: The max of fan unit is $maxnfans"
            exit 1
        fi
        FANS="$1"
    else
        usage
        exit 1
    fi
elif [ "$#" -eq 2 ]; then
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
    else
        usgae
        exit 1
    fi

    if [ $1 -gt 0 ] 2>/dev/null ; then
        if [ $1 -gt $maxnfans ]; then
            echo "Error: The max of fan unit is $maxnfans"
            exit 1
        fi
        FANS="$1"
    else
        usage
        exit 1
    fi
fi

for fan in $FANS; do
    if [ "$board_type" == "MAVERICKS" ]; then
        echo "Fan $fan RPMs: $(show_rpm $fan), ($(show_pwm $fan))"
    elif [ "$board_type" == "NEWPORT" ]; then
        echo "Fan $fan RPMs: $(show_rpm_newport $fan), ($show_pwm_newport)"
    fi
done
