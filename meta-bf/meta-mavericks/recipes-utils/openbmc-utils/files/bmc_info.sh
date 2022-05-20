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

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin

usage() {
    program=$(basename "$0")
    echo "Usage:"
    echo "  $program <bmc|bios> reset <master|slave>"
    echo ""
    echo "Examples:"
    echo "  $program bmc"
    echo "  $program bmc reset master"
    echo "  $program bios"
    echo "  $program bios reset master"
}

check_boot_source_ast2400(){
    # Please refer to reg WDT1/WDT2 Control Register definition to
    # understand this code block, WDT1 is on page 522 of ast2400v14.pdf
    # and WDT2 is on page 523 of ast2400v14.pdf
    # get watch dog1 timeout status register
    wdt1=$(devmem "0x1e785010")

    # get watch dog2 timeout status register
    wdt2=$(devmem "0x1e785030")

    wdt1_timeout_cnt=$(( ((wdt1 & 0xff00) >> 8) ))
    wdt2_timeout_cnt=$(( ((wdt2 & 0xff00) >> 8) ))
    wdt1_boot_code_source=$(( ((wdt1 & 0x2) >> 1) ))
    wdt2_boot_code_source=$(( ((wdt2 & 0x2) >> 1) ))
    boot_code_source=0

    # Check both WDT1 and WDT2 to indicate the boot source
    if [ $wdt1_timeout_cnt -ge 1 ] && [ $wdt1_boot_code_source -eq 1 ]; then
        boot_code_source=1
    elif [ $wdt2_timeout_cnt -ge 1 ] && [ $wdt2_boot_code_source -eq 1 ]; then
        boot_code_source=1
    fi

    echo $boot_code_source
}

bmc_boot_info_ast2400() {
    # get watch dog1 timeout status register
    wdt1=$(devmem "0x1e785010")

    # get watch dog2 timeout status register
    wdt2=$(devmem "0x1e785030")

    wdt1_timeout_cnt=$(( ((wdt1 & 0xff00) >> 8) ))
    wdt2_timeout_cnt=$(( ((wdt2 & 0xff00) >> 8) ))

    boot_code_source=$(check_boot_source_ast2400)

    boot_source="Master Flash"
    if [ $((boot_code_source)) -eq 1 ]
    then
      boot_source="Slave Flash"
    fi

    echo "WDT1 Timeout Count: " $wdt1_timeout_cnt
    echo "WDT2 Timeout Count: " $wdt2_timeout_cnt
    echo "Current Boot Code Source: $boot_source"
}

bmc_boot_from_ast2400() {
    # Please refer to reg WDT1/WDT2 Control Register definition to
    # understand this code block, WDT1 is on page 521 of ast2400v14.pdf
    # and WDT2 is on page 523 of ast2400v14.pdf
    # Enable watchdog reset_system_after_timeout bit and WDT_enable_signal bit.
    # Refer to ast2400v14.pdf page 524th.
    boot_source=0x00000013
    boot_code_source=$(check_boot_source_ast2400)
    if [ "$1" = "master" ]; then
        if [ $((boot_code_source)) -eq 0 ]; then
            echo "Current boot source is master, no need to switch."
            return 0
        fi
        # Set bit_7 to 0 : Use default boot code whenever WDT reset.
        boot_source=0x00000033
    elif [ "$1" = "slave" ]; then
        if [ $((boot_code_source)) -eq 1 ]; then
            echo "Current boot source is slave, no need to switch."
            return 0
        fi
        # No matter BMC boot from any one of master and slave.
        # Set bit_7 to 1 : Use second boot code whenever WDT reset.
        # And the sencond boot code stands for the other boot source.
        boot_source=0x000000b3
    fi

    echo "BMC will switch to $1 after 10 seconds..."

    # Clear WDT1 counter and boot code source status
    devmem "0x1e785014" w 0x77
    # Clear WDT2 counter and boot code source status
    devmem "0x1e785034" w 0x77
    # Set WDT time out 10s, 0x00989680 = 10,000,000 us
    devmem "0x1e785024" 32 0x00989680
    # WDT magic number to restart WDT counter to decrease.
    devmem "0x1e785028" 32 0x4755
    devmem "0x1e78502c" 32 $boot_source
}

bios_boot_info_ast2400() {
    # get BIOS SPI0 gpio pins status register
    master_disable=$(( $(head -n 1 < "/tmp/gpionames/BRG_COM_BIOS_DIS0_N/value") ))

    # get BIOS SPI1 gpio pins status register
    slave_disable=$(( $(head -n 1 < "/tmp/gpionames/BRG_COM_BIOS_DIS1_N/value") ))

    # Both BIOS SPI0 & SPI1 gpio pins status register are 1, set init value 0
    if [ $master_disable -eq 1 ] && [ $slave_disable -eq 1 ]; then
        source /usr/local/bin/openbmc-utils.sh
        # gpio_set BRG_COM_BIOS_DIS0_N 0
        # gpio_set BRG_COM_BIOS_DIS1_N 0
        boot_source="master"
    fi

    if [ $master_disable -eq 0 ]; then
        boot_source="master"
    elif [ $slave_disable -eq 0 ]; then
        boot_source="slave"
    fi

    echo "BRG_COM_BIOS_DIS0_N: $master_disable"
    echo "BRG_COM_BIOS_DIS1_N: $slave_disable"
    echo "Current Boot Code Source: $boot_source"
}

bios_boot_from_ast2400() {
    # get BIOS SPI0 gpio pins status register
    master_disable=$(( $(head -n 1 < "/tmp/gpionames/BRG_COM_BIOS_DIS0_N/value") ))

    # get BIOS SPI1 gpio pins status register
    slave_disable=$(( $(head -n 1 < "/tmp/gpionames/BRG_COM_BIOS_DIS1_N/value") ))

    # Use external BIOS flash chip to boot COMe
    source /usr/local/bin/openbmc-utils.sh

    if [ "$1" = "master" ]; then
        if [ $master_disable -eq 0 ] && [ $slave_disable -eq 0 ]; then
            echo "Current boot source is master, no need to switch."
            return 0
        fi

        if [ $master_disable -eq 1 ] && [ $slave_disable -eq 1 ]; then
            echo "Current boot source is master, no need to switch."
            return 0
        fi
        # echo 0 > "/tmp/gpionames/BRG_COM_BIOS_DIS0_N/value"
        # echo 0 > "/tmp/gpionames/BRG_COM_BIOS_DIS1_N/value"
        gpio_set BRG_COM_BIOS_DIS0_N 0
        gpio_set BRG_COM_BIOS_DIS1_N 0

        echo "uServer will boot from $1 bios at next reboot!!!!"
    elif [ "$1" = "slave" ]; then
        if [ $master_disable -eq 1 ] && [ $slave_disable -eq 0 ]; then
            echo "Current boot source is slave, no need to switch."
            return 0
        fi
        # echo 1 > "/tmp/gpionames/BRG_COM_BIOS_DIS0_N/value"
        # echo 0 > "/tmp/gpionames/BRG_COM_BIOS_DIS1_N/value"
        gpio_set BRG_COM_BIOS_DIS0_N 1
        gpio_set BRG_COM_BIOS_DIS1_N 0

        echo "uServer will boot from $1 bios at next reboot!!!!"
    else
        echo "Unknown uServer boot source, should be either master or slave!"
    fi

    # Make sure external BIOS connects to COMe instead of BMC
    gpio_set COM6_BUF_EN 0
    gpio_set COM_SPI_SEL 0
}

if [ $# -lt 1 ]; then
    usage
    exit 1
fi

case $1 in
    "bmc")
        if [ $# -eq 1 ]; then
            bmc_boot_info_ast2400
            exit 0
        else
            if [ "$2" = "reset" ] && [ $# -ge 3 ]; then
                case $3 in
                    "master" | "slave")
                        bmc_boot_from_ast2400 "$3"
                        exit 0
                    ;;
                    *)
                        usage
                        exit 1
                    ;;
                esac
            else
                usage
                exit 1
            fi
        fi
    ;;
    "bios")
        if [ $# -eq 1 ]; then
            bios_boot_info_ast2400
            exit 0
        else
            if [ "$2" = "reset" ] && [ $# -ge 3 ]; then
                case $3 in
                    "master" | "slave")
                        bios_boot_from_ast2400 "$3"
                        exit 0
                    ;;
                    *)
                        usage
                        exit 1
                    ;;
                esac
            else
                usage
                exit 1
            fi
        fi
    ;;
    *)
        usage
        exit 1
    ;;
esac