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

FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0066

show_present()
{
    fantray_pres="${FAN_DIR}/fantray${1}_present"
    echo "$(cat $fantray_pres)"
}

set -e

FANS="1 2 3 4 5 6"

for fan in $FANS; do
    echo "Fantray present: $(show_present $fan)"
done
