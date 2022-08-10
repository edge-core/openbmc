#!/bin/bash

APB_clock=24750

val=`busybox devmem 0x1e78a044 32`
val=$((${val} & 0xfffff))
Basecyc=$((2**($val & 0xf)))
CK_low=$(((($val & 0xf000)>>12)+1))
CK_high=$(((($val & 0xf0000)>>16)+1))

Freq_SCL=$(($APB_clock/$(($Basecyc * ($CK_low+$CK_high)))))
echo "I2C: Freq_SCL $Freq_SCL kHz"


