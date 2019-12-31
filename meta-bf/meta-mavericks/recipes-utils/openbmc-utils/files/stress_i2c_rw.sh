#!/bin/bash
#
# Copyright(c) 2019,  Accton Technology Corporation, All Rights Reserved
#
# Purpose: Stress Test Tool of I2C
# Command Format:
# ./stress_i2c_rw.sh one <times> <bus> <addr> <reg> <val1> <val2>
#
#   Time            <times>
#   -----------------------
#   Run infinitely  0
#   Run 3 mins      4737
#   Run 5 mins      7888
#   Run 4 hours     372865
#   Run 8 hours     745062
#   Run 12 hours    1117202
#   Run 1 days      2233074
#
# For example:
# root@bmc:~# stress_i2c_rw.sh one 4737 12 0x31 0x24 0x00 0x80
#
# The above will do System CPLD (12, 0x31) stress test around 3 minutes. It'd
# iteratively rewrite its register 0x24 with 0x00 and 0x80.
#
# Warning:
# 1. If I2C MUX exists, please carefully run this tool.
# 2. Choose the 'reserved register' to be the test target
#
# Created by Jeremy Chen, 2019.12.16
#
. /usr/local/bin/openbmc-utils.sh

board_type=$(wedge_board_type)
board_subtype=$(wedge_board_subtype)

#User Define
DBG_MODE=0 #1:output all messages
RETRY_TIMES=3 #Max. retry times of i2cget
# Timing for message output
# - First time
# - Every N-times
N_ROUND_TO_PRINT=2000
# - Each error time
# - Last time


#Macro Define
chg_val_1_2=0;
CNT_OK=0;
CNT_NG_W=0;
CNT_NG_R=0;
CNT_RETRY_R_OK=0;
ROUND_TIMES=1
FLAG_E_W=1; #for write error counter
FLAG_E_R=2; #for read error counter
MSG_LV_NOR=1; #Message level Normal
MSG_LV_ERR=2; #Message level Error
MSG_LV_DBG=3; #Message level Debug
remainder=$(($N_ROUND_TO_PRINT-1))


#User input
MODE=$1
TIMES=$2
CFG_BUS=$3
CHIP_SEL_I2C_ADDR=$4
REG=$5
val1=$6
val2=$7


show_help() {
    echo ""
    echo "USAGE:"
    echo "./stress_i2c_rw.sh one <times> <bus> <addr> <reg> <val1> <val2>"
    echo "    <times> 1,2,3,.."
    echo "            0 is infinite"
    echo ""
    echo "EX:"
    echo "root@bmc:~# stress_i2c_rw.sh one 4737 12 0x31 0x24 0x00 0x80"

    exit 1
}

if [[ "$MODE" != "one" && "$MODE" != "syscpld" && "$MODE" != "syscpld_dbg" ]]; then
    show_help
fi

if [[ $TIMES -lt 0 ]]; then
    show_help
fi

PRINT() {
    MSG_LEVEL=$1
    MSG_ROUND=$(($2-1))
    MSG_TXT=$3

    if [ $DBG_MODE -eq 1 ]; then
        MSG_LEVEL=$MSG_LV_DBG
    fi

    if [[ $MSG_LEVEL -eq $MSG_LV_ERR || $MSG_LEVEL -eq $MSG_LV_DBG ]]; then
        echo $MSG_TXT
    elif [[ $MSG_ROUND -eq 0 || $MSG_ROUND -eq $(($TIMES-1)) || $(($MSG_ROUND % $N_ROUND_TO_PRINT)) -eq $remainder ]]; then
        echo $MSG_TXT
    fi
}

read_data=0
i2c_read() {
    read_data=`i2cget -f -y $CFG_BUS $CHIP_SEL_I2C_ADDR $1`
}

i2c_write() {
   i2cset -f -y $CFG_BUS $CHIP_SEL_I2C_ADDR $1 $2
}

convert_datetime=""
convert_num_2_datetime() {
    i=$1
    ((sec=i%60, i/=60, min=i%60, i=i/60, hrs=i%24, day=i/24))
    if [ $day -ne 0 ]; then
        convert_datetime=$(printf "%02dd, %02dh, %02dm, %02ds" $day $hrs $min $sec)
    elif [ $hrs -ne 0 ]; then
        convert_datetime=$(printf "%02dh, %02dm, %02ds" $hrs $min $sec)
    elif [ $min -ne 0 ]; then
        convert_datetime=$(printf "%02dm, %02ds" $min $sec)
    else
        convert_datetime=$(printf "%02ds" $sec)
    fi
}


#
#Main
#
Start_time_show=$(date +"%Y-%m-%d %H:%M:%S")
Start_time=$(date +%s)

if [ "$MODE" == "one" ]; then
  # Store the original value
  i2c_read $REG
  ori=$read_data

  for ((i=0; i<=$TIMES; i++)); do
    if [[ $TIMES -gt 0 && $i -eq $TIMES ]]; then
      break
    fi

    if [ $chg_val_1_2 -eq 0 ]; then
      val=$val1
      chg_val_1_2=1
    else
      val=$val2
      chg_val_1_2=0
    fi

    flag_NG=0

    i2c_write $REG $val
    rtn=$?
    if [ $rtn -ne 0 ]; then
        PRINT $MSG_LV_ERR 0 "Test Count $ROUND_TIMES:"
        flag_NG=$FLAG_E_W
        PRINT $MSG_LV_ERR 0 "WRITE Error: i2cset -f -y $CFG_BUS $CHIP_SEL_I2C_ADDR $REG $val"
    else
        for ((j=0; j<$RETRY_TIMES; j++)); do
            i2c_read $REG
            rtn=$?
            # To run 'sync' could decreases the error frequency.
            sync
            if [ $rtn -ne 0 ]; then
                if [ $j -eq 0 ]; then
                    PRINT $MSG_LV_ERR 0 "Test Count $ROUND_TIMES:"
                    flag_NG=$FLAG_E_R
                fi
                PRINT $MSG_LV_ERR 0 "READ Error: i2cget -f -y $CFG_BUS $CHIP_SEL_I2C_ADDR $REG"
                continue
            fi

            if [ "$read_data" != "$val" ]; then
                if [ $j -eq 0 ]; then
                    PRINT $MSG_LV_ERR 0 "Test Count $ROUND_TIMES:"
                    flag_NG=$FLAG_E_R
                fi
                PRINT $MSG_LV_ERR 0 "DATA Error: Expect $val, Return $read_data"
                continue
            else
                if [ $j -gt 0 ]; then
                    CNT_RETRY_R_OK=$(($CNT_RETRY_R_OK+1))
                fi
                break
            fi
        done
    fi

    if [ $flag_NG -eq $FLAG_E_W ]; then
        CNT_NG_W=$(($CNT_NG_W+1))
        PRINT $MSG_LV_ERR 0 "Test Result: Total:$ROUND_TIMES, OK:$CNT_OK, NG_Write:$CNT_NG_W, NG_Read:$CNT_NG_R, RETRY_READ_OK:$CNT_RETRY_R_OK"
    elif [ $flag_NG -eq $FLAG_E_R ]; then
        CNT_NG_R=$(($CNT_NG_R+1))
        PRINT $MSG_LV_ERR 0 "Test Result: Total:$ROUND_TIMES, OK:$CNT_OK, NG_Write:$CNT_NG_W, NG_Read:$CNT_NG_R, RETRY_READ_OK:$CNT_RETRY_R_OK"
    else
        PRINT $MSG_LV_NOR $ROUND_TIMES "Test Count $ROUND_TIMES:"
        CNT_OK=$(($CNT_OK+1))
        PRINT $MSG_LV_NOR $ROUND_TIMES "Test Result: Total:$ROUND_TIMES, OK:$CNT_OK, NG_Write:$CNT_NG_W, NG_Read:$CNT_NG_R, RETRY_READ_OK:$CNT_RETRY_R_OK"
    fi

    ROUND_TIMES=$(($ROUND_TIMES+1))

    #<times>=0 is infinite.
    if [ $TIMES -eq 0 ]; then
      i=$(($i-1))
    fi
  done

  # Recover the original value
  i2c_write $REG $ori

else
  echo "Error!"
  exit 1

fi

End_time_show=$(date +"%Y-%m-%d %H:%M:%S")
End_time=$(date +%s)


#
#Output Test Result
#
echo -e "\r\nI2C Stress Test Summary"
echo "Bus                     : $CFG_BUS"
echo "Address                 : $CHIP_SEL_I2C_ADDR"
if [[ $CFG_BUS -eq 12 && "$CHIP_SEL_I2C_ADDR" = "0x31" ]]; then
  printf "  %s%s\r\n" "CPLD version          : " $(cpld_rev.sh lower sys)
fi
echo "Register                : $REG"
echo "Executed Command        : stress_i2c_rw.sh $MODE $TIMES $CFG_BUS $CHIP_SEL_I2C_ADDR $REG $val1 $val2"
echo "Start Time              : $Start_time_show"
echo "End Time                : $End_time_show"
convert_num_2_datetime $(($End_time-$Start_time))
echo "Executed Time           : $convert_datetime"
echo "Total Test Counts       : $(($ROUND_TIMES-1))"
echo "# of OK Counts          : $CNT_OK"
echo "# of Write NG Counts    : $CNT_NG_W"
echo "# of Read NG Counts     : $CNT_NG_R"
echo "  # of $RETRY_TIMES RETRY Read OK Counts : $CNT_RETRY_R_OK"
echo "  # of $RETRY_TIMES RETRY Read NG Counts : $(($CNT_NG_R-$CNT_RETRY_R_OK))"
echo ""

