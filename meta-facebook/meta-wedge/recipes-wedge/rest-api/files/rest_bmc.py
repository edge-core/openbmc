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

def get_bmc_ucd():
    
    l = []
    output = []
    err = 0
    err_status = "exit status"

    cmd = "btools.py --UCD sh v"
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

    for i in range(0, 12):	
	output.append(int(l[2*i + 1] * 1000))
    
    result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }

    return result;

def get_bmc_ps(param1):

    l = []
    output = []
    load_sharing = []
    err = [0] * 8
    err_status = "exit status"

    # input voltage data
    cmd = "btools.py --PSU %s r v" % param1
    data = Popen(cmd, \
                       shell=True, stdout=PIPE).stdout.read()

    # if error while data collection
    if err_status in data:
	    err[0] = find_err_status(data)

    t = re.findall('\d+\.\d+', data)
    try:
    	l.append(float(t[0]))
    except ValueError:
        l.append(float(0))
        pass

    # output voltage
    cmd = "btools.py --PSU %s r vo" % param1
    data = Popen(cmd, \
                       shell=True, stdout=PIPE).stdout.read()

    # if error while data collection
    if err_status in data:
	    err[1] = find_err_status(data)

    # ouput voltage data
    t = re.findall('\d+\.\d+', data)
    try:
    	l.append(float(t[0]))
    except ValueError:
        l.append(float(0))
        pass

    # input current
    cmd = "btools.py --PSU %s r i" % param1
    data = Popen(cmd, \
                       shell=True, stdout=PIPE).stdout.read()

    # if error while data collection
    if err_status in data:
	    err[2] = find_err_status(data)

    t = re.findall('\d+\.\d+', data)
    try:
    	l.append(float(t[0]))
    except ValueError:
        l.append(float(0))
        pass

    #  power supply
    cmd = "btools.py --PSU %s r p" % param1
    data = Popen(cmd, \
                       shell=True, stdout=PIPE).stdout.read()

    # if error while data collection
    if err_status in data:
	    err[3] = find_err_status(data)

    t = re.findall('\d+\.\d+', data)
    try:
    	l.append(float(t[0]))
    except ValueError:
        l.append(float(0))
        pass

    # fan spped
    cmd = "btools.py --PSU %s r fspeed" % param1
    data = Popen(cmd, \
                       shell=True, stdout=PIPE).stdout.read()

    # if error while data collection
    if err_status in data:
	    err[4] = find_err_status(data)

    t = re.findall('\d+', data)
    try:
    	l.append(float(t[0]))
    except ValueError:
        l.append(float(0))
        pass
	
    # fan fault
    cmd = "btools.py --PSU %s r ffault" % param1
    data = Popen(cmd, \
                       shell=True, stdout=PIPE).stdout.read()

    # if error while data collection
    if err_status in data:
	    err[5] = find_err_status(data)

    t = re.findall('\d+', data)
    try:
    	l.append(float(t[0]))
    except ValueError:
        l.append(float(0))
        pass

    # presence
    cmd = "btools.py --PSU %s r presence" % param1
    data = Popen(cmd, \
                       shell=True, stdout=PIPE).stdout.read()

    # if error while data collection
    if err_status in data:
	    err[6] = find_err_status(data)

    if "not present" in data:
        l.append(float(0))
    else :
        l.append(float(1))

    #  load sharing
    cmd = "btools.py --PSU %s r ld" % param1
    data = Popen(cmd, \
                       shell=True, stdout=PIPE).stdout.read()

    # if error while data collection
    if err_status in data:
	    err[7] = find_err_status(data)

    t = re.findall('\d+\.\d+', data)

    # if current is shared between supplies then load sharing
    # is true
    if float(t[0]) > 0.0 and float(t[1]) > 0.0 :
        l.append(float(1))
    else :
        l.append(float(0))

    # if err is present append it to output
    a = 0
    for x in err:
	if x != 0:
	    a = x
	    break

    output.append(a)

    for x in l:
	    output.append(int(x))
	
    result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }

    return result;

def get_bmc_fan_get(param1):

    cmd = "/usr/local/bin/get_fan_speed.sh %s" % param1
    data = Popen(cmd, \
                       shell=True, stdout=PIPE).stdout.read()

    print data

#if __name__ == '__main__':
#   get_bmc_ps(2)
