#!/bin/bash
I2C_BUS=$1
EEP_ADDR=0X51
SYS_ASSM_PART_NUM=(0X31 0x33 0x35 0x30 0x30 0x30 0x30 0x31 0x31 0x30 0x31)
SYS_ASSM_PART_NUM_LEN=${#SYS_ASSM_PART_NUM[@]}
REG_ADDR=0X0100

echo "Reading from the EEPROM"
echo "The read bytes are :"
/usr/sbin/i2cset -f -y $I2C_BUS $EEP_ADDR $((($REG_ADDR&0xFF00)>>8)) $(($REG_ADDR&0x00FF))
for ((idx=0; idx<SYS_ASSM_PART_NUM_LEN; idx++))
do
  /usr/sbin/i2cget -f -y $I2C_BUS $EEP_ADDR
done
echo "Reading Done"
