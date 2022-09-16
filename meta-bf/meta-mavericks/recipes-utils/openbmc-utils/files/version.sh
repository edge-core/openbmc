#!/bin/bash


###############################################################################
# Function.
. /usr/local/bin/openbmc-utils.sh

usage() {
    echo "Usage: check version"  
    echo ""
    echo "Examples:"
    echo "      version.sh -a                           # Check All version."
    echo "      version.sh -b                           # Check BMC version."
    echo "      version.sh -e                           # Check EC_Code version."
    echo "      version.sh -s                           # Check SYS_CPLD version"
    echo "      version.sh -f                           # Check FAN_CPLD version"
}

board_subtype=$(wedge_board_subtype)

check_all() {   
    issue=`cat /etc/issue`
    version=`cat /etc/version`
    printf "%-13s%-s %02s %02s" "BMC" : $issue \($version\) 
    echo ""
    cat /sys/class/i2c-adapter/i2c-4/4-0033/version | awk '{print "EC Version  ",":",$3,$4,$5}'
    if [ "$board_subtype" == "Mavericks" ]; then
        sys_upper_cpld_ver=`cpld_rev.sh upper sys`
        sys_lower_cpld_ver=`cpld_rev.sh lower sys`
        printf "%-13s%-s %02s\n" "SYS Upper CPLD" : $sys_upper_cpld_ver
        printf "%-13s%-s %02s\n" "SYS Lower CPLD" : $sys_lower_cpld_ver
    else
        sys_cpld_ver=`cpld_rev.sh lower sys`
        printf "%-13s%-s %02s\n" "SYS CPLD" : $sys_cpld_ver
    fi  
    if [ "$board_subtype" == "Mavericks" ]; then
        fan_upper_cpld_ver=`cpld_rev.sh upper fan`
        fan_lower_cpld_ver=`cpld_rev.sh lower fan`
        printf "%-13s%-s %02s\n" "FAN Upper CPLD" : $fan_upper_cpld_ver
        printf "%-13s%-s %02s\n" "FAN Lower CPLD" : $fan_lower_cpld_ver
    else
        fan_cpld_ver=`cpld_rev.sh lower fan`
        printf "%-13s%-s %02s\n" "FAN CPLD" : $fan_cpld_ver
    fi
}
    
###############################################################################
# Start of the script.

if [[ $# -ne 1 ]]; then
    usage
    exit -1
fi

check_module=$1


case "$check_module" in
    "-a")
        echo "Print All Version Info"
        check_all
        ;;
    "-b")
        echo "Print BMC Version Info"
        issue=`cat /etc/issue`
        version=`cat /etc/version`
        printf "%-13s%-s %02s %02s" "BMC" : $issue \($version\)
        echo ""
        ;;
    "-e")
        echo "Print EC_Code Version Info"
        cat /sys/class/i2c-adapter/i2c-4/4-0033/version | awk '{print "EC Version  ",":",$3,$4,$5}'
        ;;
    "-s")
        if [ "$board_subtype" == "Mavericks" ]; then
            sys_upper_cpld_ver=`cpld_rev.sh upper sys`
            sys_lower_cpld_ver=`cpld_rev.sh lower sys`
            printf "%-13s%-s %02s\n" "SYS Upper CPLD" : $sys_upper_cpld_ver
            printf "%-13s%-s %02s\n" "SYS Lower CPLD" : $sys_lower_cpld_ver
        else
            sys_cpld_ver=`cpld_rev.sh lower sys`
            printf "%-13s%-s %02s\n" "SYS CPLD" : $sys_cpld_ver
        fi
        ;;
    "-f")
        if [ "$board_subtype" == "Mavericks" ]; then
            fan_upper_cpld_ver=`cpld_rev.sh upper fan`
            fan_lower_cpld_ver=`cpld_rev.sh lower fan`
            printf "%-13s%-s %02s\n" "FAN Upper CPLD" : $fan_upper_cpld_ver
            printf "%-13s%-s %02s\n" "FAN Lower CPLD" : $fan_lower_cpld_ver
        else
            fan_cpld_ver=`cpld_rev.sh lower fan`
            printf "%-13s%-s %02s\n" "FAN CPLD" : $fan_cpld_ver
        fi
        ;;
    *)
        usage
        exit -1
        ;;
esac
