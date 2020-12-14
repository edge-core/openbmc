#!/usr/bin/env python
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import urllib2
import subprocess
import syslog
import time
import ssl
import os
import os.path
import signal
from rest_config import RestConfig

listen_ssl = RestConfig.getboolean('listen', 'ssl')
port = RestConfig.getint('listen', 'port')

# it's on localhost if i can't trust it we have bigger problems.
sslctx = ssl.create_default_context()
sslctx.check_hostname = False
sslctx.verify_mode = ssl.CERT_NONE

checkurl = "http{}://localhost:{}/api".format(
    's' if listen_ssl else '', port)
interval = 5
timeout = 5
service = "restapi"
# intervals before forced kill
grace = 3
graceleft = grace

output_okmsg_in_retry_or_rcv = 0

def killall(killcmd=b'python\x00/usr/local/bin/rest.py\x00'):
    try:
        pids = [pid for pid in os.listdir('/proc') if pid.isdigit()]
    except Exception as e:
        syslog.syslog(syslog.LOG_ERR,
                      "Error scanning /proc: {}".format(str(e)))
    for pid in pids:
        try:
            with open(os.path.join('/proc', pid, 'cmdline'), 'rb') as cf:
                cmdline = cf.read()
                if cmdline == killcmd:
                    syslog.syslog(syslog.LOG_ERR, 'Killing leftover rest '
                            'process: pid={} ({})...'
                            .format(pid, cmdline.replace('\x00', ' ')))
                    # would sigint but at this point we should be touching only
                    # procs that escaped runit, they're probably broken badly
                    # already anyway
                    os.kill(int(pid), signal.SIGKILL)
        except IOError:
            syslog.syslog(syslog.LOG_ERR, 'IOError')
            continue


def runit_status(svc):
    return subprocess.call(["/usr/bin/sv", "status", svc])


def runit_kill_restart(svc):
    # Distinguish which one makes REST restart.
    # If kill by watchdog, recover by watchdog.
    file = open("/tmp/wdt_rcv_rest", "w+")
    file.write('1')
    file.close()
    syslog.syslog(syslog.LOG_INFO, 'REST API WDT recovery enable(1)')
    # force-stop is SIGTERM, wait 7s, if still alive, SIGKILL
    devnull = open(os.devnull, 'w')
    try:
    #subprocess.call(["/usr/bin/sv", "force-stop", svc])
        subprocess.call(["/usr/bin/sv", "force-stop", svc], stdout=devnull,stderr=devnull)
    finally:
        devnull.close()
    # kill any lingering hung forks
    killall()
    # bring it back
    devnull = open(os.devnull, 'w')
    try:
    #subprocess.call(["/usr/bin/sv", "start", svc])
        subprocess.call(["/usr/bin/sv", "start", svc], stdout=devnull,stderr=devnull)
    finally:
        devnull.close()
    file = open("/tmp/wdt_rcv_rest", "w+")
    file.write('0')
    file.close()
    syslog.syslog(syslog.LOG_INFO, 'REST API WDT recovery disable(0)')


def main():
    global graceleft
    global output_okmsg_in_retry_or_rcv
    syslog.openlog(b"restwatchdog")
    syslog.syslog(syslog.LOG_INFO,
                  "REST API watchdog checking %s, every %d seconds"
                  % (checkurl, interval))
    while True:
        time.sleep(interval)
        status = runit_status(service)
        if status != 0:
            syslog.syslog(syslog.LOG_WARNING,
                          "REST API not running (sv status: %d)" % (status, ))
            continue
        else:
            try:
                f = urllib2.urlopen(checkurl, timeout=timeout, context=sslctx)
                f.read()
                graceleft = grace
                if output_okmsg_in_retry_or_rcv != 0:
                    syslog.syslog(syslog.LOG_INFO, 'Successful!')
                    output_okmsg_in_retry_or_rcv = 0
            except Exception as e:
                syslog.syslog(syslog.LOG_WARNING, "REST API not responding due to {}".format(str(e)))
                if graceleft <= 0:
                    syslog.syslog(syslog.LOG_ERR,
                                  "Killing unresponsive REST service")
                    runit_kill_restart(service)
                    graceleft = grace
                else:
                    output_okmsg_in_retry_or_rcv = 1
                    syslog.syslog(syslog.LOG_WARNING, 'Retry remaining times: %d' % (graceleft))
                    graceleft -= 1

if __name__ == "__main__":
    main()
