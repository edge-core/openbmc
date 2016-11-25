#!/bin/bash
I2C_BUS=$1
EEP_ADDR=0X51
SYS_ASSM_PART_NUM=(0X31 0x33 0x35 0x30 0x30 0x30 0x30 0x31 0x31 0x30 0x31)
SYS_ASSM_PART_NUM_LEN=${#SYS_ASSM_PART_NUM[@]}
REG_ADDR=23

echo "Writing to the EEPROM"
for ((idx=0; idx<SYS_ASSM_PART_NUM_LEN; idx++))
do
  cur_addr=$REG_ADDR+idx
  /usr/sbin/i2cset -f -y $I2C_BUS $EEP_ADDR $((($cur_addr&0xFF00)>>8)) $(($cur_addr&0x00FF)) ${SYS_ASSM_PART_NUM[$idx]} i
done
echo "Writing Done"
