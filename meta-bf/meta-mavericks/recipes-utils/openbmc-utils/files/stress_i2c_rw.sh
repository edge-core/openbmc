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
# System CPLD I2C      => BMC# stress_i2c_rw.sh one 4737 12 0x31 0x24 0x00 0x80
# Fan CPLD I2C         => BMC# stress_i2c_rw.sh one 4737 8 0x66 0x06 0x00 0x01
#
# Warning: If I2C MUX exists, please carefully run this tool.
#
# Created by Jeremy Chen, 2019.12.16
#
. /usr/local/bin/openbmc-utils.sh

board_type=$(wedge_board_type)
board_subtype=$(wedge_board_subtype)

#Macro define
chg_val_12=0;
OK=0;
NG=0;

DATETIME_PRINT() {
  datatime=$(date +"%Y-%m-%d %H:%M:%S")
  echo $datatime
}

DBG_PRINT() {
   #echo $1
   printf ""
}

#Project dependency
if [ "$board_subtype" == "Mavericks" ]; then
  sys_cpld_reg=(0x00 0x01 0x02 0x08 0x0B 0x0D 0x0E 0x0F 0x10 0x11 0x12 0x13 0x14 0x1B 0x20 0x21 0x22 0x23 0x24 0x25 0x26 0x28 0x29 0x2E 0x2F 0x30 0x31 0x32 0x33 0x38 0x39 0x3A 0x3B 0x3E 0x3F)
  sys_cpld_val=(0x01 0x0a 0x01 0xf5 0x01 0x00 0x00 0x00 0xeb 0x01 0x2e 0xff 0x1a 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xff 0x00 0x98 0xf2 0xfb 0xff 0x0f 0x06 0xff 0xff 0x00 0x0f 0x09 0x02)
elif [ "$board_subtype" == "Montara" ]; then
#FIX ME: Montara is not ready
  sys_cpld_reg=(0x00 0x01 0x02 0x08 0x0B 0x0D 0x0E 0x0F 0x10 0x11 0x12 0x13 0x14 0x1B 0x20 0x21 0x22 0x23 0x24 0x25 0x26 0x28 0x29 0x2E 0x2F 0x30 0x31 0x32 0x33 0x38 0x39 0x3A 0x3B 0x3E 0x3F)
  sys_cpld_val=(0x01 0x0a 0x01 0xf5 0x01 0x00 0x00 0x00 0xeb 0x01 0x2e 0xff 0x1a 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xff 0x00 0x98 0xf2 0xfb 0xff 0x0f 0x06 0xff 0xff 0x00 0x0f 0x09 0x02)
elif [ "$board_subtype" == "Newport" ]; then
  sys_cpld_reg=(0x00 0x01 0x02 0x08 0x0B 0x0D 0x0E 0x0F 0x10 0x11 0x12 0x13 0x14 0x1B 0x20 0x21 0x22 0x23 0x24 0x25 0x26 0x28 0x29 0x2E 0x2F 0x30 0x31 0x32 0x33 0x38 0x39 0x3A 0x3B 0x3E 0x3F)
  sys_cpld_val=(0x21 0x02 0x04 0xf0 0x07 0x00 0x00 0x00 0xe8 0x03 0x5f 0xff 0x3a 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xff 0x00 0x98 0xf2 0xdf 0xff 0x0f 0x07 0xff 0xff 0x00 0x0f 0x09 0x02)
fi

MODE=$1
TIMES=$2
ROUND_TIMES=1
BUS=$3
ADDR=$4
REG=$5
val1=$6
val2=$7

DATETIME_PRINT
if [ "$MODE" == "one" ]; then
# Store the original value
  ori=$(i2cget -f -y $BUS $ADDR $REG)

  for ((i=0; i<=$TIMES; i++)); do
    if [[ $TIMES -gt 0 && $i -eq $TIMES ]]; then
      break
    fi
    echo "Test Count $ROUND_TIMES:"

    if [ $chg_val_12 -eq 0 ]; then
      i2cset -f -y $BUS $ADDR $REG $val1
      chg_val_12=1
    else
      i2cset -f -y $BUS $ADDR $REG $val2
      chg_val_12=0
    fi
    i2cget -f -y $BUS $ADDR $REG
    ret=$?
    if [ $ret -eq 0 ]; then
      OK=$(($OK+1))
    else
      NG=$(($NG+1))
    fi

    echo -e "Test Result: $ret(T:$ROUND_TIMES, OK:$OK, NG:$NG)\r\n\r\n"

    ROUND_TIMES=$(($ROUND_TIMES+1))

    if [ $TIMES -eq 0 ]; then
      i=$(($i-1))
    fi
  done

# Recover the original value
  i2cset -f -y $BUS $ADDR $REG $ori

elif [ "$MODE" == "syscpld" ]; then
# Show CPLD version
  printf "CPLD version:"
  cpld_rev.sh lower sys

  for ((i=0; i<=$TIMES; i++)); do
    if [[ $TIMES -gt 0 && $i -eq $TIMES ]]; then
      break
    fi
    cnt_no_match=0
    echo "Test Count $ROUND_TIMES:"

# TEST AREA
    for ((r=0; r<${#sys_cpld_reg[@]}; r++)); do
      rtn=$(i2cget -f -y 12 0x31 ${sys_cpld_reg[$r]})
      DBG_PRINT "Read register ${sys_cpld_reg[$r]}, value is $rtn."
      if [[ "$rtn" != "${sys_cpld_val[$r]}" ]]; then
        # In case there are variable values (Two or more) for some registers
        if [ "$board_subtype" == "Mavericks" ]; then
            if [[ "0x08" == "${sys_cpld_reg[$r]}" && "${sys_cpld_val[$r]}"=="0xf1" ]]; then
              echo "Match reg[${sys_cpld_reg[$r]}]{EXP, RTN}={0xf1, $rtn}"
              continue
            elif [[ "0x26" == "${sys_cpld_reg[$r]}" && "${sys_cpld_val[$r]}"=="0x01" ]]; then
              echo "Match reg[${sys_cpld_reg[$r]}]{EXP, RTN}={0xf1, $rtn}"
              continue
            fi
        elif [ "$board_subtype" == "Montara" ]; then
            #FIX ME: Montara is not ready
            print ""
        elif [ "$board_subtype" == "Newport" ]; then
            print ""
        fi

        cnt_no_match=$(($cnt_no_match+1))
        echo "No match reg[${sys_cpld_reg[$r]}]{EXP, RTN}={${sys_cpld_val[$r]}, $rtn}"
      else
        DBG_PRINT "Match reg[${sys_cpld_reg[$r]}]{EXP, RTN}={${sys_cpld_val[$r]}, $rtn}"
      fi
    done
# END

    if [ $cnt_no_match -eq 0 ]; then
      OK=$(($OK+1))
      echo -e "Test Result: Pass(T:$ROUND_TIMES, OK:$OK, NG:$NG)\r\n\r\n"
    else
      NG=$(($NG+1))
      echo -e "Test Result: Fail(T:$ROUND_TIMES, OK:$OK, NG:$NG)\r\n\r\n"
    fi

    ROUND_TIMES=$(($ROUND_TIMES+1))

    if [ $TIMES -eq 0 ]; then
      i=$(($i-1))
    fi
  done

elif [ "$MODE" == "syscpld_dbg" ]; then
  for ((r=0; r<${#sys_cpld_reg[@]}; r++)); do
    rtn=$(i2cget -f -y 12 0x31 ${sys_cpld_reg[$r]})
    echo "$rtn"
  done
else
  echo "Error!"
  exit 1
fi

DATETIME_PRINT
