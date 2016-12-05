#!/bin/bash
I2C_BUS=$1
SYS_ASSM_PART_NUM_LEN=11
if test "$#" -ne 2; then
    echo "Illegal number of arguments"
    echo "Usage : bash bf_pltfm_eeprom_write.sh <i2c_bus> <11 digit assm num>"
    exit 1
fi

digits=$2
subscript=0
while [ "$subscript" -lt $SYS_ASSM_PART_NUM_LEN ]
    do
        bd_id[${subscript}]=${digits:${subscript}:1 }
        ((subscript +=1))
    done

EEP_ADDR=0X51
REG_ADDR=23

echo "Writing to the EEPROM"
for ((idx=0; idx<SYS_ASSM_PART_NUM_LEN; idx++))
do
  cur_addr=$REG_ADDR+idx
  /usr/sbin/i2cset -f -y $I2C_BUS $EEP_ADDR $((($cur_addr&0xFF00)>>8)) $(($cur_addr&0x00FF)) $((${bd_id[$idx]}|0x30)) i
done
echo "Writing Done"
