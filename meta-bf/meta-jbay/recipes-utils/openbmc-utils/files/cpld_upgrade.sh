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
usage() {
    echo "Usage: ${0} <target board (lower)> <target cpld (sys fan)> <cpld_image.jbc>" >&2
}

upgrade_lower_syscpld() {
    echo "Started Lower SYSCPLD upgrade .."
    echo out > /tmp/gpionames/CPLD_JTAG_SEL/direction
    echo 1 > /tmp/gpionames/CPLD_JTAG_SEL/value

    #program syscpld
    rc=$(jbi -r -aPROGRAM -gc102 -gi101 -go103 -gs100 $1 | grep -i "Success")
    if [[ $rc == *"Success"* ]]; then
        echo "Finished Lower SYSCPLD upgrade: Pass"
    else
        echo "Finished Lower SYSCPLD upgrade: Fail (Program failed)"
    fi
}

upgrade_lower_fancpld() {
    echo "Started Lower FANCPLD upgrade .."
    echo out > /tmp/gpionames/CPLD_UPD_EN/direction
    echo 0 > /tmp/gpionames/CPLD_UPD_EN/value

    echo out > /tmp/gpionames/BMC_FANCARD_CPLD_JTAG__SEL/direction
    echo 0 > /tmp/gpionames/BMC_FANCARD_CPLD_JTAG__SEL/value

    #program fancpld
    rc=$(jbi -aPROGRAM -gc77 -gi78 -go79 -gs76 $1 | grep -i "Success")
    if [[ $rc == *"Success"* ]]; then
        echo "Finished Lower FANCPLD upgrade: Pass"
    else
        echo "Finished Lower FANCPLD upgrade: Fail (Program failed)"
    fi
}

# Check the number of arguments provided
if [ $# -ne 3 ]; then
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

# Check the file path provided is a valid one.
jbcfile="$3"
if [ ! -f $jbcfile ]; then
    echo "$jbcfile does not exist"
    exit 1
fi

# Check the file path extension is .jbc
filename="$(basename $jbcfile)"
if [ ${filename: -4} != ".jbc" ] && [ ${filename: -4} != ".JBC" ]; then
    echo "Must pass in a .jbc file"
    exit 1
fi

# Check the file name and upgrade accordingly
if [ $1 == "lower" ] && [ $2 == "sys" ]; then
    upgrade_lower_syscpld $jbcfile
elif [ $1 == "lower" ] && [ $2 == "fan" ]; then
    upgrade_lower_fancpld $jbcfile
else
  usage
  exit 1
fi
