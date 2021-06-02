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

### BEGIN INIT INFO
# Provides:          power-on
# Required-Start:
# Required-Stop:
# Default-Start:     S
# Default-Stop:
# Short-Description: Power on micro-server
### END INIT INFO
. /usr/local/bin/openbmc-utils.sh

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin

board_subtype=$(wedge_board_subtype)
echo "board type is $board_subtype"

tofino_set_vdd_core() {
  CODE="$(i2cget -f -y 12 0x31 0xb)"
  CODE_M=$(($CODE & 0x7))
  CODE_EN=$(($CODE & 0x8))
  if [ "$board_subtype" == "Newport" ] ; then
    local apn
    apn=$(wedge_board_sys_assembly_pn)
    case "$apn" in
      *015-000004-03)
        if [ $CODE_EN == 0 ]; then
          btools.py --IR set_vdd_core 0.780
          echo "setting Newport-P0B SVS-disabled Tofino VDD_CORE to 0.780V..."
        else
          tbl=(0.726 0.756 0.781 0.796 0.811 0.831 0.856 0.886)
          btools.py --IR set_vdd_core ${tbl[$CODE_M]}
          logger "VDD setting: ${tbl[$CODE_M]}"
          echo "setting Newport-P0B SVS $CODE Tofino VDD_CORE to ${tbl[$CODE_M]}..."
        fi
        ;;
      *)
        btools.py --IR set_vdd_core 0.825
        echo "setting Newport Tofino VDD_CORE to 0.825V..."
        ;;
    esac
    return 0
  fi
# set the Tofino VDD voltage here before powering-ON COMe
  if [ $CODE_M != 0 ]; then
    tbl=(0 0.83 0.78 0.88 0.755 0.855 0.805 0.905)
    btools.py --IR set_vdd_core ${tbl[$CODE_M]}
    logger "VDD setting: ${tbl[$CODE_M]}"
    echo "setting Tofino VDD_CORE to ${tbl[$CODE_M]}..."
  fi
}

# make power button high to prepare for power on sequence
gpio_set BMC_PWR_BTN_OUT_N 1

#switch COMe tty to dbg port
mav_tty_switch_delay.sh 1

# First power on TH, and if Panther+ is used,
# provide standby power to Panther+.

val=$(cat $PWR_MAIN_SYSFS 2> /dev/null | head -n 1)
#preserve $val as COMe would be in a powered-ON state after the following command
wedge_power_on_board

if [ "$val" != "0x1" ]; then
  if [ "$board_subtype" == "Newport" ] ; then
    # credo slow parts need 1.7V instead of 1.5V
    btools.py --IR set 1.7v VDDA_1.5V
  fi
  tofino_set_vdd_core
  usleep 100000
  i2cset -f -y 12 0x31 0x32 0x9
  usleep 50000
  i2cset -f -y 12 0x31 0x32 0xf
fi

echo -n "Checking microserver power status ... "
if wedge_is_us_on 10 "."; then
    echo "on"
    on=1
else
    echo "off"
    on=0
fi

if [ $on -eq 0 ]; then
    # Power on now
    wedge_power.sh on -f
fi

#switch COMe tty to BMC UART after 45 seconds
echo "wait for 45 seconds before connecting to COMe..."
mav_tty_switch_delay.sh 0 45 &

