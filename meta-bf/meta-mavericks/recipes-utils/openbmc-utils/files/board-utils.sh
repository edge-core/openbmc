# Copyright 2015-present Facebook. All Rights Reserved.

SYSCPLD_SYSFS_DIR="/sys/class/i2c-adapter/i2c-12/12-0031"
PWR_MAIN_SYSFS="${SYSCPLD_SYSFS_DIR}/pwr_main_n"
PWR_USRV_SYSFS="${SYSCPLD_SYSFS_DIR}/pwr_usrv_en"
PWR_USRV_BTN_SYSFS="${SYSCPLD_SYSFS_DIR}/pwr_usrv_btn_en"
SLOTID_SYSFS="${SYSCPLD_SYSFS_DIR}/slotid"

wedge_iso_buf_enable() {
    # TODO, no isolation buffer
    return 0
}

wedge_iso_buf_disable() {
    # TODO, no isolation buffer
    return 0
}

wedge_is_us_on() {
    local val n retries prog
    if [ $# -gt 0 ]; then
        retries="$1"
    else
        retries=1
    fi
    if [ $# -gt 1 ]; then
        prog="$2"
    else
        prog=""
    fi
    if [ $# -gt 2 ]; then
        default=$3              # value 0 means defaul is 'ON'
    else
        default=1
    fi
    val=$(cat $PWR_MAIN_SYSFS 2> /dev/null | head -n 1)
    if [ -z "$val" ]; then
        return $default
    elif [ "$val" == "0x1" ]; then
        return 0            # powered on
    else
        return 1
    fi
}

TMP_EEPROM_BOARD_TYPE="/tmp/eeprom_board_type"
TMP_EEPROM_SYS_PART_NO="/tmp/eeprom_sys_assembly_pn"

wedge_board_type() {
    local pn sapn
    if [ -e "$TMP_EEPROM_BOARD_TYPE" ]; then
        pn=$(cat $TMP_EEPROM_BOARD_TYPE 2> /dev/null)
    else
        pn=$(/usr/bin/weutil 2> /dev/null | grep -i '^Location on Fabric:')
        echo "$pn" > $TMP_EEPROM_BOARD_TYPE
        sapn=$(/usr/bin/weutil 2> /dev/null | grep -i '^System Assembly Part Number:')
        echo "$sapn" > $TMP_EEPROM_SYS_PART_NO
    fi
    case "$pn" in
        *Montara*)
            echo 'MAVERICKS'
            ;;
        *Maverick*)
            echo 'MAVERICKS'
            ;;
        *Mavericks*)
            echo 'MAVERICKS'
            ;;
        *Newport*)
            echo 'NEWPORT'
            ;;
        *Newports*)
            echo 'NEWPORT'
            ;;
        *)                  # Follow bf_board_type_get() of fand.cpp, defaulting to Montara
            echo 'MAVERICKS'
            ;;
    esac
}

wedge_board_subtype() {
    local pn
    if [ -e "$TMP_EEPROM_BOARD_TYPE" ]; then
        pn=$(cat $TMP_EEPROM_BOARD_TYPE 2> /dev/null)
    else
        pn=$(/usr/bin/weutil 2> /dev/null | grep -i '^Location on Fabric:')
        echo "$pn" > $TMP_EEPROM_BOARD_TYPE
    fi
    case "$pn" in
        *Montara*)
            echo 'Montara'
            ;;
        *Maverick*)
            echo 'Mavericks'
            ;;
        *Mavericks*)
            echo 'Mavericks'
            ;;
        *Newport*)
            echo 'Newport'
            ;;
        *Newports*)
            echo 'Newport'
            ;;
        *)                  # Follow bf_board_type_get() of fand.cpp, defaulting to Montara
            echo 'Montara'
            ;;
    esac
}

wedge_board_sys_assembly_pn() {
    local sapn
    if [ -e "$TMP_EEPROM_SYS_PART_NO" ]; then
        sapn=$(cat $TMP_EEPROM_SYS_PART_NO 2> /dev/null)
    else
        sapn=$(/usr/bin/weutil 2> /dev/null | grep -i '^System Assembly Part Number:')
        echo "$sapn" > $TMP_EEPROM_BOARD_TYPE
    fi
    echo $sapn
}

wedge_slot_id() {
    printf "%d\n" $(cat $SLOTID_SYSFS)
}

wedge_board_rev() {
    local val0 val1 val2
    val0=$(gpio_get BOARD_REV_ID0)
    val1=$(gpio_get BOARD_REV_ID1)
    val2=$(gpio_get BOARD_REV_ID2)
    echo $((val0 | (val1 << 1) | (val2 << 2)))
}

# Should we enable OOB interface or not
wedge_should_enable_oob() {
    # wedge100 uses BMC MAC since beginning
    return -1
}

#sets UCD device GPIO pin; call with care
#param-0 (string) gpio "Pin Id" (not the GPIO number)
#param-1 (string) 0 for low and 1 for high
wedge_ucd_gpio_set() {
    if [ $# -ne 2 ]; then
        return
    fi
    if [ $1 -gt 23 ]; then
        return
    fi
    #select the gpio
    i2cset -f -y 2 0x34 0xfa $1
    #set the gpio
    if [ $2 == "0" ]; then
        i2cset -f -y 2 0x34 0xfb 0x3
        usleep 10
        i2cset -f -y 2 0x34 0xfb 0x0
    else
        i2cset -f -y 2 0x34 0xfb 0x7
        usleep 10
        i2cset -f -y 2 0x34 0xfb 0x4
    fi
}

#gets UCD device GPIO pin; call with care
#param-0 (string) gpio "Pin Id" (not the GPIO number)
wedge_ucd_gpio_get() {
    if [ $# -ne 1 ]; then
        return
    fi
    if [ $1 -gt 23 ]; then
        return
    fi
    #select the gpio
    i2cset -f -y 2 0x34 0xfa $1
    i2cget -f -y 2 0x34 0xfb
}

wedge_power_on_board() {
    local val isolbuf
    board_subtype=$(wedge_board_subtype)

    # enable rails that might have been turned off during previous shutdown
    # sequence is important
    wedge_ucd_gpio_set 22 1
    wedge_ucd_gpio_set 12 1
    wedge_ucd_gpio_set 13 1
    wedge_ucd_gpio_set 20 1
    wedge_ucd_gpio_set 21 1

    # power on main power, uServer power, and enable power button
    val=$(cat $PWR_MAIN_SYSFS | head -n 1)
    if [ "$val" != "0x1" ]; then
        if [ "$board_subtype" == "Newport" ] ; then
            echo 1 > $PWR_USRV_SYSFS
            echo "setting pwr_usrv_en also for $board_subtype"
        fi
        echo 1 > $PWR_MAIN_SYSFS
        sleep 2
    fi
    val=$(cat $PWR_USRV_BTN_SYSFS | head -n 1)
    if [ "$val" != "0x1" ]; then
        echo 1 > $PWR_USRV_BTN_SYSFS
        sleep 1
    fi

    # Enable the COME I2C isolation buffer
    val=$(i2cget -f -y 12 0x31 0x28) 
    isolbuf=$(($val|0x30))
    i2cset -f -y 12 0x31 0x28 $isolbuf
}
