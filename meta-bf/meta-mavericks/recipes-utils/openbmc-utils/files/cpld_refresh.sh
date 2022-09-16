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

board_subtype=$(wedge_board_subtype)
product_sub_version=$(cat /tmp/eeprom_product_sub_version | awk '{print $3}')
wedge32x_fancpld="Wedge32X_FANCPLD_refresh.vme"
wedge32x_syscpld="Wedge32X_SYSCPLD_refresh.vme"

wedge65x_fancpld="WEDGE_65X_FanCPLD_Refresh.vme"
wedge65x_lowersyscpld="WEDGE_65X_LowerCPLD_Refresh.vme"
wedge65x_uppersyscpld="WEDGE_65X_UpperCPLD_Refresh.vme"

usage() {
    echo "Usage: ${0} <target board (upper lower)> <target cpld (sys fan)>" >&2
}

refresh_upper_syscpld() {
    echo "Started Upper SYSCPLD refresh .."
    # Select BMC channel
    echo out > /tmp/gpionames/CPLD_UPPER_JTAG_SEL/direction
    echo 1 > /tmp/gpionames/CPLD_UPPER_JTAG_SEL/value
    #disable heartbeat
    i2cset -y -f 12 0x30 0x2e 0x18

    if [ ! -e $wedge65x_uppersyscpld ]; then
        echo "refresh file not exist"
        rc="Fail"
    else
        rc=$(ispvm syscpld uppersys $wedge65x_uppersyscpld | grep PASS)
    fi
    if [ "$rc" == "| PASS! |" ]; then
        echo "Finished Upper FANCPLD refresh: Pass"
    else
        echo "Finished Upper FANCPLD refresh: Fail"
    fi
}

refresh_lower_syscpld() {
    echo "Started Lower SYSCPLD refresh .."
    # Select BMC channel
    echo out > /tmp/gpionames/CPLD_JTAG_SEL/direction
    echo 1 > /tmp/gpionames/CPLD_JTAG_SEL/value
    #disable heartbeat
    i2cset -y -f 12 0x31 0x2e 0x18

    if [ "$board_subtype" == "Mavericks" ] && [ $product_sub_version -ge 5 ]; then
        if [ ! -e $wedge65x_lowersyscpld ]; then
            echo "refresh file not exist"
            rc="Fail"
        else
            rc=$(ispvm syscpld lowersys $wedge65x_lowersyscpld | grep PASS)
        fi
    elif [ "$board_subtype" == "Montara" ] && [ $product_sub_version -ge 4 ]; then
        if [ ! -e $wedge32x_syscpld ]; then
            echo "refresh file not exist"
            rc="Fail"
        else
            rc=$(ispvm syscpld lowersys $wedge32x_syscpld | grep PASS)
        fi
    else
        echo "refresh cpld Fail"
        exit 1
    fi

    if [ "$rc" == "| PASS! |" ]; then
        echo "Finished Upper FANCPLD refresh: Pass"
    else
        echo "Finished Upper FANCPLD refresh: Fail"
    fi
}

refresh_upper_fancpld() {
    echo "Started Upper FANCPLD refresh .."
    # Enable CPLD update (UPD)
    echo out > /tmp/gpionames/UPPER_FANCARD_CPLD_UPD_EN/direction
    echo 0 > /tmp/gpionames/UPPER_FANCARD_CPLD_UPD_EN/value
    # Select upper channel
    echo out > /tmp/gpionames/BMC_FANCARD_CPLD_JTAG__SEL/direction
    echo 1 > /tmp/gpionames/BMC_FANCARD_CPLD_JTAG__SEL/value

    if [ ! -e $wedge65x_fancpld ]; then
        echo "refresh file not exist"
        rc="Fail"
    else
        rc=$(ispvm syscpld fan $wedge65x_fancpld | grep PASS)
    fi
    if [ "$rc" == "| PASS! |" ]; then
        echo "Finished Upper FANCPLD refresh: Pass"
    else
        echo "Finished Upper FANCPLD refresh: Fail"
    fi
}

refresh_lower_fancpld() {
    echo "Started Lower FANCPLD refresh .."

    if [ "$board_subtype" == "Montara" ] || [ "$board_subtype" == "Mavericks" ] ; then
        # Enable CPLD update (UPD)
        echo out > /tmp/gpionames/CPLD_UPD_EN/direction
        echo 0 > /tmp/gpionames/CPLD_UPD_EN/value
    fi

    if [ "$board_subtype" == "Mavericks" ] ; then
        # Select lower channel
        echo out > /tmp/gpionames/BMC_FANCARD_CPLD_JTAG__SEL/direction
        echo 0 > /tmp/gpionames/BMC_FANCARD_CPLD_JTAG__SEL/value
    fi

    if [ "$board_subtype" == "Mavericks" ] && [ $product_sub_version -ge 5 ]; then
        if [ ! -e $wedge65x_fancpld ]; then
            echo "refresh file not exist"
            rc="Fail"
        else
            rc=$(ispvm syscpld fan $wedge65x_fancpld | grep PASS)
        fi
    elif [ "$board_subtype" == "Montara" ] && [ $product_sub_version -ge 4 ]; then
        if [ ! -e $wedge32x_fancpld ]; then
            echo "refresh file not exist"
            rc="Fail"
        else
            rc=$(ispvm syscpld fan $wedge32x_fancpld | grep PASS)
        fi
    else
        echo "refresh cpld Fail"
        exit 1
    fi
    
    if [ "$rc" == "| PASS! |" ]; then
        echo "Finished Upper FANCPLD refresh: Pass"
    else
        echo "Finished Upper FANCPLD refresh: Fail"
    fi
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

if [ "$board_subtype" == "Mavericks" ] && [ $product_sub_version -le 4 ]; then
    echo "this machine use altera, no need to refresh"
    exit 1
elif [ "$board_subtype" == "Montara" ] && [ $product_sub_version -le 3 ]; then
    echo "this machine use altera, no need to refresh"
    exit 1
else
    echo " "
fi


# 2U: Mavericks, 1U: Montara, Newport
if [ $1 == "upper" ] && [ "$board_subtype" != "Mavericks" ]; then
    echo "upper board does not exist"
    exit 1
fi

# Check the file name and refresh accordingly
if [ $1 == "upper" ]; then
    if [ $2 == "sys" ]; then
        refresh_upper_syscpld
    else
        refresh_upper_fancpld
    fi
elif [ $1 == "lower" ]; then
    if [ $2 == "sys" ]; then
        refresh_lower_syscpld
    else
        refresh_lower_fancpld
    fi
else
  usage
  exit 1
fi
