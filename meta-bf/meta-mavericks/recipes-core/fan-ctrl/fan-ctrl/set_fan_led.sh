#!/bin/sh
#
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
fi

usage() {
    echo "Usage:"
}

set_led()
{
    if [ $1 -gt 5 ]; then
        fan=$(( $1 - 5 ))
        ledctrl="${FAN_DIR_UPPER}/fantray${fan}_led_ctrl"
        ledblink="${FAN_DIR_UPPER}/fantray${fan}_led_blink"
    else
        ledctrl="${FAN_DIR}/fantray${1}_led_ctrl"
        ledblink="${FAN_DIR}/fantray${1}_led_blink"
    fi
    echo $2 > $ledctrl
    ret=$?
    if [ $ret -ne 0 ]; then
        echo "0x1"
        return
    fi

    echo $3 > $ledblink
    ret=$?
    if [ $ret -ne 0 ]; then
        echo "0x1"
    else
        echo "0x0"
    fi
}

if [ "$#" -gt 4 ]; then
    usage
    exit 1
fi

# set_fan_led.sh 3 0x1 0x0 Mavericks
if [ "$board_type" == "MAVERICKS" ]; then
    if [ "$#" -eq 4 ]; then
        if [ $4 = "Mavericks" ]; then
            if [ "$board_subtype" != "Mavericks" ]; then
                echo "Error: This is $board_subtype"
                exit 1
            fi
        elif [ $4 = "Montara" ]; then
            if [ "$board_subtype" != "Montara" ]; then
                echo "Error: This is $board_subtype"
                exit 1
            fi
        else
            usage
            exit 1
        fi
        if [ $1 -lt 1 ] || [ $2 -lt 0 ] || [ $3 -lt 0 ]; then
            usage
            exit 1
        fi
        if [ $1 -gt $maxnfans  ] || [ $2 -gt 3 ] || [ $3 -gt 1 ]; then
            usage
            exit 1
        fi
        echo "Fan $1 led: $(set_led  $1 $2 $3)"
    else
        usage
        exit 1
    fi
fi
