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
import btools

from cStringIO import StringIO
import sys

import subprocess
import bmc_command

class Capturing(list):
    def __enter__(self):
        self._stdout = sys.stdout
        sys.stdout = self._stringio = StringIO()
        return self
    def __exit__(self, *args):
        self.extend(self._stringio.getvalue().splitlines())
        del self._stringio    # free up some memory
        sys.stdout = self._stdout


# Handler for FRUID resource endpoint
def get_bmc():
    # Get BMC Reset Reason
    (wdt_counter, _) = Popen('devmem 0x1e785010', \
                             shell=True, stdout=PIPE).communicate()
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
    (uptime, _) = Popen('uptime', \
                        shell=True, stdout=PIPE).communicate()

    # Get Usage information
    (data, _) = Popen('top -b n1', \
                        shell=True, stdout=PIPE).communicate()
    adata = data.split('\n')
    mem_usage = adata[0]
    cpu_usage = adata[1]

    # Get OpenBMC version
    version = ""
    (data, _) = Popen('cat /etc/issue', \
                        shell=True, stdout=PIPE).communicate()
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
    platform = btools.get_project()

    arg = ['btools.py', '--TMP', 'sh']

    # ignore the input of project name field
    print "Auto detection, ignore %s" % str(param1)

    with Capturing() as screen_op:
         btools.main(arg)
    data = str(screen_op)

    # if error while data collection
    if err_status in data:
      err = find_err_status(data)

    for t in data.split():
        try:
             l.append(float(t))
        except ValueError:
             pass

    output.append(err)

    if platform == "mavericks" or platform == "mavericks-p0c":
      for i in range(0, 9):
        output.append(int(l[2*i + 1] * 10))

      #Max device temperature
      output.append(int(l[19] * 10))

    if platform == "montara" or platform == "newport":
      for i in range(0, 5):
        output.append(int(l[2*i + 1] * 10))

      #Max device temperature
      output.append(int(l[11] * 10))

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
    platform = btools.get_project()
    valid_range = 12

    arg = ['btools.py', '--UCD', 'sh', 'v']

    if platform == "mavericks-p0c":
         valid_range = 16
    if platform == "mavericks":
         valid_range = 15
    if platform == "montara" or platform == "newport":
         valid_range = 12

    with Capturing() as screen_op:
         btools.main(arg)

    data = str(screen_op)

    # if error while data collection
    if err_status in data:
      err = find_err_status(data)

    t = re.findall('\d+\.\d+', data)
    for i in range (0, valid_range):
        try:
             l.append(float(t[i]))
        except ValueError:
             pass

    output.append(err)

    for i in range(0, valid_range):
       output.append(int(l[i] * 1000))

    result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }

    return result;

def get_bmc_ps_feature(param1, param2):

  output = []
  if param2 == "presence":
     r = btools.psu_check_pwr_presence(int(param1))
     output.append(int(r))

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
    not_present = "not_present"

    r = btools.psu_check_pwr_presence(int(param1))
    if r != 0 :
        # 0. error status
        output.append("0")
        output.append("absent")
        result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }
        return result;


    arg = ['btools.py', '--PSU', '1', 'r', 'v']
    arg[2] = str(param1)

    # input voltage data
    with Capturing() as screen_op:
         btools.main(arg)
    data = str(screen_op)

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

    arg[4] = 'vo'
    with Capturing() as screen_op:
         btools.main(arg)
    data = str(screen_op)

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
    arg[4] = 'i'
    with Capturing() as screen_op:
         btools.main(arg)
    data = str(screen_op)

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
    arg[4] = 'p'
    with Capturing() as screen_op:
         btools.main(arg)
    data = str(screen_op)

    # if error while data collection
    if err_status in data:
      err[3] = find_err_status(data)

    t = re.findall('\d+\.\d+', data)
    try:
      l.append(float(t[0]))
    except ValueError:
        l.append(float(0))
        pass

    # fan speed
    arg[4] = 'fspeed'
    with Capturing() as screen_op:
         btools.main(arg)
    data = str(screen_op)

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
    arg[4] = 'ffault'
    with Capturing() as screen_op:
         btools.main(arg)
    data = str(screen_op)

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
    arg[4] = 'presence'
    with Capturing() as screen_op:
         btools.main(arg)
    data = str(screen_op)

    # if error while data collection
    if err_status in data:
      err[6] = find_err_status(data)

    if not_present in data:
        l.append(float(0))
    else :
        l.append(float(1))

    #  load sharing
    arg[4] = 'ld'
    with Capturing() as screen_op:
         btools.main(arg)
    data = str(screen_op)

    # if error while data collection
    if err_status in data:
      err[7] = find_err_status(data)

    t = re.findall('\d+\.\d+', data)

    # if current is shared between supplies then load sharing
    # is true
    try :
        if float(t[0]) > 0.0 and float(t[1]) > 0.0 :
            l.append(float(1))
        else :
            l.append(float(0))
    except Exception :
        l.append(float(0))
        err[7] = 0

    #if err is present append it to output
    a = 0
    for x in err:
        if x != 0:
            a = x
            break

    output.append(a)

    for x in l:
      output.append(int(x))

    # ps model
    arg[4] = 'psmodel'
    with Capturing() as screen_op:
         btools.main(arg)
    data = str(screen_op)

    # if error while data collection
    if err_status in data:
      err[8] = find_err_status(data)

    t = re.findall('[\w\.-]+', data)
    try:
      output.append(t[0])
    except ValueError:
        output.append("None")
        pass

    # ps serial
    arg[4] = 'psserial'
    with Capturing() as screen_op:
         btools.main(arg)
    data = str(screen_op)

    # if error while data collection
    if err_status in data:
      err[9] = find_err_status(data)
         
    t = re.findall('[\w\.-]+', data)
    try:
      output.append(t[0])
    except ValueError:
        output.append("None")
        pass

    # ps ver
    arg[4] = 'psrev'
    with Capturing() as screen_op:
         btools.main(arg)
    data = str(screen_op)

    # if error while data collection
    if err_status in data:
      err[10] = find_err_status(data)

    t = re.findall('[\w\.-]+', data)
    try:
      output.append(t[0])
    except ValueError:
        output.append("None")
        pass

    result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }

    return result;

def get_bmc_fan(param1):

    output = []
    err = 0
    error = ["error", "Error", "ERROR"]

    cmd = "/usr/local/bin/get_fan_speed.sh %s" % param1
    data = Popen(cmd, \
                       shell=True, stdout=PIPE).stdout.read()

    # if error while data collection
    if any(x in data for x in error):
        err = 1

    output.append(err)

    t = re.findall('\d+', data)

    for x in t:
      output.append(int(x))

    cmd = "/usr/local/bin/get_fantray_present.sh"
    data = Popen(cmd, \
                       shell=True, stdout=PIPE).stdout.read()


    t = re.findall('\d+', data)
    output.append(int(t[0]))

    result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }

    return result;

def set_bmc_fan(param1, param2, param3):

    output = []
    error = ["error", "Error", "ERROR"]
    err = 0
    cmd = "/usr/local/bin/set_fan_speed.sh %s %s %s" % (param3, param2, param1)
    data = Popen(cmd, \
                      shell=True, stdout=PIPE).stdout.read()

    # if error while data collection
    if any(x in data for x in error):
        err = 1

    output.append(err)
    result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }

    return result;

def get_bmc_sensors(args):
    output = []
    error = ["error", "Error", "ERROR"]
    err = 0
    cmd = "/usr/bin/sensors %s" %(args)
    data = Popen(cmd, \
                      shell=True, stdout=PIPE).stdout.read()

    output.append(data)

    # if error while data collection
    if any(x in data for x in error):
        err = 1

    output.append(err)

    result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }

    return result;

