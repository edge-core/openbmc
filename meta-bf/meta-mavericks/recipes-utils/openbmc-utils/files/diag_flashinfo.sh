#!/bin/bash


usage() {
    echo "Usage: $0 type/size"
    echo "  type: check BMC FLASH type"
    echo "  size: check BMC FLASH size"
    echo
}

check_type(){
    BMC_FLASH_MAIN_TYPE="mx25l25635e"
	BMC_FLASH_REPLACE_TYPE="w25q256"
    spi_path="/tmp/spi.txt"
    has_error=0

    dmesg | grep "spi0.0" > $spi_path
    if [ ! -s "$spi_path" ];then
        echo "spi0.0 message isn't be recorded"
        has_error=-1
    fi

    flash_main_type_num=`cat $spi_path | grep $BMC_FLASH_MAIN_TYPE | wc -l`
	flash_replace_type_num=`cat $spi_path | grep $BMC_FLASH_REPLACE_TYPE | wc -l`
    if [ "$flash_main_type_num" == "3" ]; then
        echo "BMC FLASH type is ${BMC_FLASH_MAIN_TYPE}"
        echo "check BMC FLASH type: pass"
    else 
		if [ "$flash_replace_type_num" == "3" ]; then
			echo "BMC FLASH type is ${BMC_FLASH_REPLACE_TYPE}"
			echo "check BMC FLASH type: pass"
		else
			echo "check BMC FLASH type: fail"
			echo "LOG: "
			cat $spi_path
			has_error=-1
		fi
	fi
    rm $spi_path
    exit $has_error
}

check_size(){
    FLASH_SIZE=(00060000 00020000 00400000 01780000 00400000 02000000)
	i=0
	
    for name  in u-boot env kernel rootfs data0 flash0
    do		
        flash=`cat /proc/mtd | grep $name`
        if [ ! -n "$flash" ];then
            echo "flash $name doesn't exist"
            exit -1
        fi

        flash_size=`echo $flash | awk '{print $2}'`
        echo "BMC FLASH $name size is ${flash_size}"
        if [ "$flash_size" != "${FLASH_SIZE[$i]}" ]; then
            echo "check BMC FLASH $name size: fail"
            echo "LOG: $flash"
            exit -1
        fi
		let i=i+1		
    done
    echo "check FLASH size: pass"
}


if [ $# -ne 1 ]; then
    usage
    exit -1
fi

object="$1"

case "$object" in
    type)
        check_type
        ;;
    size)
        check_size
        ;;
    *)
        usage
        exit -1
        ;;
esac
