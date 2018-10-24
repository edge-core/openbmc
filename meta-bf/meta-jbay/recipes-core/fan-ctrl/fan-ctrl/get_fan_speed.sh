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
usage() {
    echo "Usage: $0 [Fan Unit (1..6)] [board type (Jbay)]" >&2
}

FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0066

# Convert the percentage to our 1/16th unit (0-15).
show_pwm()
{
    pwm="${FAN_DIR}/fantray_pwm"
    val=$(cat $pwm | head -n 1)
# According to Fan_board_CPLD_Specification, the fan module PWM is the same if register value is between 1 and 4.
# 0000: 0x6.25% = 0% duty cycle      1000: 9x6.25% = 56.25% duty cycle
# 0001: 5x6.25% = 31.25% duty cycle  1001: 10x6.25% = 62.50% duty cycle
# 0010: 5x6.25% = 31.25% duty cycle  1010: 11x6.25% = 68.75% duty cycle
# 0011: 5x6.25% = 31.25% duty cycle  1011: 12x6.25% = 75.00% duty cycle
# 0100: 5x6.25% = 31.25% duty cycle  1100: 13x6.25% = 81.25% duty cycle
# 0101: 6x6.25% = 37.50% duty cycle  1101: 14x6.25% = 87.50% duty cycle
# 0110: 7x6.25% = 43.75% duty cycle  1110: 15x6.25% = 93.75% duty cycle
# 0111: 8x6.25% = 50.00% duty cycle  1111: 16x6.25% = 100.0% duty cycle

    if [[ $val -gt 0 ]]; then
        if [[ $val -lt 4 ]]; then
            val=4
        fi
        val=$(( val + 1 ))
    fi
    echo "$((val * 100 / 16))%"
}

show_rpm()
{
    front_rpm="${FAN_DIR}/fan$((($1 * 2 - 1)))_input"
    rear_rpm="${FAN_DIR}/fan$((($1 * 2)))_input"
    echo "$(cat $front_rpm), $(cat $rear_rpm)"
}

set -e

# refer to the comments in init_pwn.sh regarding
# the fan unit and tacho mapping
# fixing function to handle jbay number of fans
# while keeping backward command line compatibility with original FB script
if [ "$#" -eq 0 ]; then
        FANS="1 2 3 4 5 6"
elif [ "$#" -eq 1 ]; then
    if [ $1 = "Jbay" ]; then
        FANS="1 2 3 4 5 6"
    else
        if [ $1 -gt 6 ]; then
            usage
            exit 1
        fi
        FANS="$1"
    fi
elif [ "$#" -eq 2 ]; then
    if [ $2 != "Jbay" ]; then
        usage
        exit 1
    fi
    if [ $1 -gt 6 ]; then
        usage
        exit 1
    fi
    FANS="$1"
else
    usage
    exit 1
fi

for fan in $FANS; do
    echo "Fan $fan RPMs: $(show_rpm $fan), ($show_pwm)"
done
