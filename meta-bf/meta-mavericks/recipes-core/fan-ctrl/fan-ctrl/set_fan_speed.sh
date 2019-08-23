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
        echo "Usage: $0 <PERCENT (0..100)>" >&2
    fi
}

set -e

if [ "$board_type" == "MAVERICKS" ]; then
    if [ "$#" -gt 3 ] || [ "$#" -lt 1 ]; then
        usage
        exit 1
    fi

    if [ $1 -ge 0 ] 2>/dev/null ; then
        if [ $1 -gt 100 ]; then
            usage
            exit 1
        fi
    else
        usage
        exit 1
    fi

    # Adding fix for Mavericks and Montara while keeping backward compatibility with FB command line.
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
    if [ "$#" -gt 1 ] || [ "$#" -lt 1 ]; then
        usage
        exit 1
    fi

    if [ $1 -ge 0 ] 2>/dev/null ; then
        if [ $1 -gt 100 ]; then
            usage
            exit 1
        fi
    else
        usage
        exit 1
    fi

    #For Newport, Convert the percentage to our 1/16th unit (0-15).
    # According to Fan_board_CPLD_Specification,
    # 0000(0): 1x6.25% = 06.25% duty cycle  1000(8):   9x6.25% = 56.25% duty cycle
    # 0001(1): 2x6.25% = 12.50% duty cycle  1001(9):  10x6.25% = 62.50% duty cycle
    # 0010(2): 3x6.25% = 18.75% duty cycle  1010(10): 11x6.25% = 68.75% duty cycle
    # 0011(3): 4x6.25% = 25.00% duty cycle  1011(11): 12x6.25% = 75.00% duty cycle
    # 0100(4): 5x6.25% = 31.25% duty cycle  1100(12): 13x6.25% = 81.25% duty cycle
    # 0101(5): 6x6.25% = 37.50% duty cycle  1101(13): 14x6.25% = 87.50% duty cycle
    # 0110(6): 7x6.25% = 43.75% duty cycle  1110(14): 15x6.25% = 93.75% duty cycle
    # 0111(7): 8x6.25% = 50.00% duty cycle  1111(15): 16x6.25% = 100.0% duty cycle
    # For example,
    # the PWM of  0.00% <= fan <=   6.25% will be assigned to 0000(0).
    # the PWM of  6.25% <  fan <=  12.50% will be assigned to 0001(1).
    # the PWM of 12.50% <  fan <=  18.75% will be assigned to 0010(2).
    # the PWM of 18.75% <  fan <=  25.00% will be assigned to 0011(3).
    # ...
    # the PWM of 93.75% <  fan <= 100.00% will be assigned to 1111(15).
    unit=$(( ( $1 * 16 ) / 100 ))
    if [ $1 -eq 25 ] || [ $1 -eq 50 ] || [ $1 -eq 75 ] || [ $1 -eq 100 ]; then
        unit=$(($unit - 1))
    fi

    pwm="${FAN_DIR}/fantray_pwm"
    echo "$unit" > $pwm
    echo "Successfully set fan speed to $1%"
fi
