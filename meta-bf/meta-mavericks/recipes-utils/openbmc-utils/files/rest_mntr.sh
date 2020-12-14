#!/bin/sh

SERVICE="rest.py"
FILE_WDT_RCV_REST="/tmp/wdt_rcv_rest"

check_8080_port(){
    for ((i=0; i<=12; i++)); do
        netstat -tanu | grep 8080 | grep LISTEN > /dev/null
        cmd=$?
        string=`netstat -tanu | grep 8080 | grep LISTEN`
        logger "$string"
        if [ "$cmd" == 0 ]; then
            return
        fi
        sleep 5
    done
}

while [ 1 ]
do
    wdt_rcv_rest=0
    RESULT=`ps | sed -n /${SERVICE}/p`
    if [ "${RESULT:-null}" = null ]; then
        if [ -e "$FILE_WDT_RCV_REST" ]; then
            wdt_rcv_rest=`cat $FILE_WDT_RCV_REST`
            logger "REST MNTR WDT recovery enable(1)/disable(0): $wdt_rcv_rest"
        fi
        if [ $wdt_rcv_rest -eq 0 ]; then
            logger "REST MNTR rest.py not running"
            /usr/local/bin/setup-rest-api.sh restart > /dev/null
        fi
        check_8080_port
        if [ -e "/tmp/mav_9548_10_lock" ]; then
          rm -f "/tmp/mav_9548_10_lock"
          echo "Stale mav_9548_10_lock exists. Removing it"
        fi
    fi
    sleep 5
done
