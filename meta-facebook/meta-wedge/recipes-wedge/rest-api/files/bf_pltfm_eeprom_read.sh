#!/bin/bash
I2C_BUS=$1
EEP_ADDR=0X51
SYS_ASSM_PART_NUM_LEN=11
REG_ADDR=23

echo "Reading from the EEPROM"
echo "The read bytes are :"
/usr/sbin/i2cset -f -y $I2C_BUS $EEP_ADDR $((($REG_ADDR&0xFF00)>>8)) $(($REG_ADDR&0x00FF))
for ((idx=0; idx<SYS_ASSM_PART_NUM_LEN; idx++))
do
  /usr/sbin/i2cget -f -y $I2C_BUS $EEP_ADDR
done
echo "Reading Done"
