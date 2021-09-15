#!/bin/sh
#
#
. /usr/local/bin/openbmc-utils.sh

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin

board_type=$(wedge_board_type)
board_subtype=$(wedge_board_subtype)

if [ "$board_subtype" == "Mavericks" ]; then
    maxnfans=10
    #FANS="1 2 3 4 5 6 7 8 9 10"
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0033
    FAN_DIR_UPPER=/sys/class/i2c-adapter/i2c-9/9-0033
elif [ "$board_subtype" == "Montara" ]; then
    maxnfans=5
    #FANS="1 2 3 4 5"
    FAN_DIR=/sys/class/i2c-adapter/i2c-8/8-0033
fi

usage() {
    echo "Usage:" 
}

# Convert the percentage to our 1/32th unit (0-31).
show_led_ctrl()
{
    if [ $1 -gt 5 ]; then
        fan=$(( $1 - 5 ))
        led="${FAN_DIR_UPPER}/fantray${fan}_led_ctrl"
    else
        led="${FAN_DIR}/fantray${1}_led_ctrl"
    fi
    val=$(cat $led | head -n 1)
    echo "$val"
}

show_led_blink()
{
    if [ $1 -gt 5 ]; then
        fan=$(( $1 - 5 ))
        led="${FAN_DIR_UPPER}/fantray${fan}_led_blink"
    else
        led="${FAN_DIR}/fantray${1}_led_blink"
    fi
    val=$(cat $led | head -n 1)
    echo "$val"
}

if [ "$#" -gt 2 ]; then
    usage
    exit 1
fi

if [ "$board_type" == "MAVERICKS" ]; then
# get_fan_led.sh 3 Mavericks
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
      else
          usage
          exit 1
      fi

      if [ $1 -gt 0 ] 2>/dev/null ; then
          if [ $1 -gt $maxnfans ]; then
              echo "Error: The max of fan unit is $maxnfans"
              exit 1
          fi
          FANS="$1"
          echo "Fan $FANS led: $(show_led_ctrl $FANS), $(show_led_blink $FANS)"
      else
          usage
          exit 1
      fi
  fi
fi
