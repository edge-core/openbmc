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

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin

prog="$0"

PWR_BTN_GPIO="BMC_PWR_BTN_OUT_N"
PWR_SYSTEM_SYSFS="${SYSCPLD_SYSFS_DIR}/pwr_cyc_all_n"
PWR_USRV_RST_SYSFS="${SYSCPLD_SYSFS_DIR}/usrv_rst_n"
PWR_TF_RST_SYSFS="${SYSCPLD_SYSFS_DIR}/tofino_pwr_on_rst_n"
PWR_USRV_SYSFS="${SYSCPLD_SYSFS_DIR}/pwr_main_n"
PWR_USRV_EN_SYSFS="${SYSCPLD_SYSFS_DIR}/pwr_usrv_en"

board_subtype=$(wedge_board_subtype)
echo "board type is $board_subtype"

tofino_set_vdd_core() {
  CODE="$(i2cget -f -y 12 0x31 0xb)"
  CODE_M=$(($CODE & 0x7))
  if [ "$board_subtype" == "Newport" ] ; then
    local apn
    apn=$(wedge_board_sys_assembly_pn)
    case "$apn" in
      *015-000004-03)
          # Offset 0x0B, bit[3] CODE_EN not used. CPLD does not have the necessary information.
          tbl=(0.737 0.768 0.793 0.808 0.823 0.843 0.869 0.899)
          btools.py --IR set_vdd_core ${tbl[$CODE_M]}
          logger "VDD setting: ${tbl[$CODE_M]}"
          echo "setting Newport-P0B SVS $CODE Tofino VDD_CORE to ${tbl[$CODE_M]}..."
          ;;
      *)
          btools.py --IR set_vdd_core 0.825
          echo "setting Newport Tofino VDD_CORE to 0.825V..."
          ;;
    esac
    return 0
  fi
}

usage() {
    echo "Usage: $prog <command> [command options]"
    echo
    echo "Commands:"
    echo "  status: Get the current microserver power status"
    echo
    echo "  on: Power on microserver if not powered on already"
    echo "    options:"
    echo "      -f: Re-do power on sequence no matter if microserver has "
    echo "          been powered on or not."
    echo
    echo "  off: Power off microserver ungracefully"
    echo
    echo "  reset: Power reset microserver ungracefully"
    echo "    options:"
    echo "      -s: Power reset whole wedge system ungracefully"
    echo
}

do_status() {
    echo -n "Microserver power is "
    if wedge_is_us_on; then
        echo "on"
    else
        echo "off"
    fi
    return 0
}

do_on_ucd_gpio_en() {
  # sequence is important
  wedge_ucd_gpio_set 22 1
  wedge_ucd_gpio_set 12 1
  wedge_ucd_gpio_set 13 1
  wedge_ucd_gpio_set 20 1
  wedge_ucd_gpio_set 21 1
}

do_on_com_e() {
    board_subtype=$(wedge_board_subtype)

    if [ "$board_subtype" == "Newport" ] ; then
        # turn ON the power rails that might have been forced down

        echo 1 > $PWR_USRV_EN_SYSFS
        echo "wedge_power setting pwr_usrv_en also for $board_subtype"

        echo 1 > $PWR_USRV_SYSFS
	local svern
	svern=$(wedge_board_sub_version)
	case "$svern" in
	*1)
	logger "no voltage setting with modified HW for board type $board_subtype"
	echo "but, no voltage setting with modified HW for board type $board_subtype"
	sleep 2
	# dont do 1.7V, GPIO_en and vdd_core setting on NEWPORT_MOD
	#credo slow parts need 1.7V instead of 1.5V
	#        btools.py --IR set n VDDA_1.7V
	#        tofino_set_vdd_core
	  ;;
	*)
	sleep 2
	# do_on_ucd_gpio_en
	# credo slow parts need 1.7V instead of 1.5V
	btools.py --IR set n VDDA_1.7V
	tofino_set_vdd_core
	   ;;
	esac

        usleep 100000
        # issue reset to Tofino-2
        i2cset -f -y 12 0x31 0x32 0x9
        usleep 50000
        i2cset -f -y 12 0x31 0x32 0xf

        return 0
    fi
    echo 1 > $PWR_USRV_SYSFS
    return $?
}

do_on() {
    local force opt ret
    force=0
    while getopts "f" opt; do
        case $opt in
            f)
                force=1
                ;;
            *)
                usage
                exit -1
                ;;

        esac
    done
    echo -n "Power on microserver ..."
    if [ $force -eq 0 ]; then
        # need to check if uS is on or not
        if wedge_is_us_on 10 "."; then
            echo " Already on. Skip!"
            return 1
        fi
    fi

    # reset Tofino
#    reset_brcm.sh
    # power on sequence
    do_on_com_e
    ret=$?
    if [ $ret -eq 0 ]; then
        echo " Done"
        logger "Successfully power on micro-server"
    else
        echo " Failed"
        logger "Failed to power on micro-server"
    fi
    return $ret
}

do_off_com_e() {
    board_subtype=$(wedge_board_subtype)

    if [ "$board_subtype" == "Newport" ] ; then
        echo 0 > $PWR_USRV_EN_SYSFS
        echo "wedge_power setting pwr_usrv_en also for $board_subtype"
    fi

    echo 0 > $PWR_USRV_SYSFS
    return $?
}

do_off() {
    local ret
    echo -n "Power off microserver ..."
    do_off_com_e
    ret=$?
    if [ $ret -eq 0 ]; then
        echo " Done"
    else
        echo " Failed"
    fi
    return $ret
}

do_reset() {
    local system opt pulse_us
    system=0
    while getopts "s" opt; do
        case $opt in
            s)
                system=1
                ;;
            *)
                usage
                exit -1
                ;;
        esac
    done
    if [ $system -eq 1 ]; then
        pulse_us=100000             # 100ms
        logger "Power reset the whole system ..."
        echo -n "Power reset the whole system ..."
        sleep 1
        echo 0 > $PWR_SYSTEM_SYSFS
        # Echo 0 above should work already. However, after CPLD upgrade,
        # We need to re-generate the pulse to make this work
        usleep $pulse_us
        echo 1 > $PWR_SYSTEM_SYSFS
        usleep $pulse_us
        echo 0 > $PWR_SYSTEM_SYSFS
        usleep $pulse_us
        echo 1 > $PWR_SYSTEM_SYSFS
    else
        if ! wedge_is_us_on; then
            echo "Power resetting microserver that is powered off has no effect."
            echo "Use '$prog on' to power the microserver on"
            return -1
        fi
        # reset Tofino  first
#        reset_brcm.sh
        echo -n "Power reset microserver ..."
        echo 0 > $PWR_USRV_RST_SYSFS
        sleep 1
        echo 1 > $PWR_USRV_RST_SYSFS
        logger "Successfully power reset micro-server"
    fi
    echo " Done"
    return 0
}

if [ $# -lt 1 ]; then
    usage
    exit -1
fi

command="$1"
shift

case "$command" in
    status)
        do_status $@
        ;;
    on)
        do_on $@
        ;;
    off)
        do_off $@
        ;;
    reset)
        do_reset $@
        ;;
    *)
        usage
        exit -1
        ;;
esac

exit $?
