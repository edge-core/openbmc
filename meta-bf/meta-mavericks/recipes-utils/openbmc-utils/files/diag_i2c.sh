#!/bin/bash

#SCU70[9:8]= 01 360MHz    SCU08[25:23]= 011  PCLK = H-PLL/8
APB_clock=45000

val=`busybox devmem 0x1e78a044 32`
val=$((${val} & 0xfffff))
Basecyc=$((2**($val & 0xf)))
CK_low=$(((($val & 0xf000)>>12)+1))
CK_high=$(((($val & 0xf0000)>>16)+1))

Freq_SCL=$(($APB_clock/$(($Basecyc * ($CK_low+$CK_high)))))
echo "BMC i2c speed: $Freq_SCL kHz"


