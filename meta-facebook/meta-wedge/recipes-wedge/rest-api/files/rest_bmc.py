#!/usr/bin/env python
#
# Copyright 2014-present Facebook. All Rights Reserved.
#
# This program file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program in a file named COPYING; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301 USA
#


from subprocess import *
import re

# Handler for FRUID resource endpoint
def get_bmc():
    # Get BMC Reset Reason
    wdt_counter = Popen('devmem 0x1e785010', \
                        shell=True, stdout=PIPE).stdout.read()
    wdt_counter = int(wdt_counter, 0)

    wdt_counter &= 0xff00

    if wdt_counter:
        por_flag = 0
    else:
        por_flag = 1

    if por_flag:
        reset_reason = "Power ON Reset"
    else:
        reset_reason = "User Initiated Reset or WDT Reset"

    # Get BMC's Up Time
    uptime = Popen('uptime', \
                        shell=True, stdout=PIPE).stdout.read()

    # Get Usage information
    data = Popen('top -b n1', \
                        shell=True, stdout=PIPE).stdout.read()
    adata = data.split('\n')
    mem_usage = adata[0]
    cpu_usage = adata[1]

    # Get OpenBMC version
    version = ""
    data = Popen('cat /etc/issue', \
                        shell=True, stdout=PIPE).stdout.read()
    ver = re.search(r'v([\w\d._-]*)\s', data)
    if ver:
        version = ver.group(1)

    result = {
                "Information": {
                    "Description": "Wedge BMC",
                    "Reset Reason": reset_reason,
                    "Uptime": uptime,
                    "Memory Usage": mem_usage,
                    "CPU Usage": cpu_usage,
                    "OpenBMC Version": version,
                },
                "Actions": [],
                "Resources": [],
             }

    return result;

def find_err_status(data):

    err_status = "exit status"

    for item in data.split('\n'):
   	    if err_status in item:
	        try:
	            err = int(item[-1:])
	            return err
	        except ValueError:
		        #default error if i2c failure 2
		        err = 2
		        return 2

    return err
	
def get_bmc_tmp(param1):

    l = []
    output = []
    err = 0
    err_status = "exit status"

    cmd = "btools.py --TMP %s sh" % param1
    data = Popen(cmd, \
                       shell=True, stdout=PIPE).stdout.read()

    # if error while data collection
    if err_status in data:
	    err = find_err_status(data)

    for t in data.split():
        try:
             l.append(float(t))
        except ValueError:
             pass	

    output.append(err)

    if param1 == "Mavericks" :
    	for i in range(0, 9):	
	    output.append(int(l[2*i + 1] * 10))
		
	#Max device temperature
	output.append(int(l[18] * 10))

    if param1 == "Montara" :
	for i in range(0, 5):
	    output.append(int(l[2*i + 1] * 10))

    result = {
		        "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }

    return result;

#if __name__ == '__main__':
#   get_bmc_tmp("Mavericks")
