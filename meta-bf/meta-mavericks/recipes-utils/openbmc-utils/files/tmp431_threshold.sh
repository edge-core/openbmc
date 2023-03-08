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

program=$(basename "$0")

usage() {
    echo "Usage: $program <command> [command options]"
    echo
    echo "Commands:"
    echo "  1) status: Get the current TMP431 threshold status"
    echo
    echo "  2) set: Set the current TMP431 threshold value"
    echo "     < TMP431 threshold >"
    echo "      50 | 60 | 70 | 80 | 90 | 105"
    echo
}

do_get() {
    value=$(i2cget -f -y 3 0x4c 0x19)
    echo -n "TMP431 Threshold is $((value)) C"
    echo
    return 0
}

do_set() {
    
    #TMP431 threshold
    value="$1"
    case $1 in
        50)
            value="0x32"
            ;;
        60)
            value="0x3C"
            ;;
        70)
            value="0x46"
            ;;
        80)
            value="0x50"
            ;;
        90)
            value="0x5A"
            ;;
        105)
            value="0x69"
            ;;
        *)
            usage
            exit -1
        ;;
    esac
    i2cset -f -y 3 0x4c 0x19 $value
    echo -n "Set TMP431 Threshold is $((value)) C"
    echo
    return 0
}

if [ $# -lt 1 ]; then
    usage $@
    exit -1
fi

command="$1"
shift

case "$command" in
    status)
        do_get $@
        exit 0
        ;;
    set)
        do_set $@
        exit 0
        ;;
    *)
        usage
        exit -1
        ;;
esac
