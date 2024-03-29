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
import threading

from cStringIO import StringIO
import sys

import subprocess
import bmc_command

lock = threading.Lock()

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
    ver = re.search(r'.* (\w+\.\w+\.\w+).*', data)
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
    err = 0
    for item in data.split('\''):
        if err_status in item:
            try:
                err = int(item.strip()[-3:-2])
                return err
            except Exception as e:
                #default error if i2c failure 2
                err = 2
                return 2

    return err

def get_bmc_tmp(param1):

    lock.acquire()
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
        except Exception as e:
            l.append(float(0))
            pass

    output.append(err)

    if platform == "mavericks" or platform == "mavericks-p0c":
        try:
            for i in range(0, 9):
                output.append(int(l[2*i + 1] * 10))
            #Max device temperature
            output.append(int(l[19] * 10))
        except Exception as e:
            #fill all 0 when error
            output = []
            if len(l) != 20:
               err = 3
            output.append(err)
            for i in range(0, 10):
                output.append(int(0))
            pass

    if platform == "montara" or platform == "newport" or platform == "stinson" or platform == "davenport":
        try:
            for i in range(0, 5):
                output.append(int(l[2*i + 1] * 10))
            #Max device temperature
            output.append(int(l[11] * 10))
        except Exception as e:
            #fill all 0 when error
            output = []
            if len(l) != 12:
                err = 3
            output.append(err)
            for i in range(0, 6):
                output.append(int(0))
            pass

    result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }

    lock.release()
    return result;

def get_bmc_ucd():

    lock.acquire()
    l = []
    output = []
    err = 0
    err_status = "exit status"
    platform = btools.get_project()
    valid_range = 12

    arg = ['btools.py', '--UCD', 'sh', 'v']

    if platform == "mavericks-p0c" or platform == "newport" or platform == "stinson" or platform == "davenport":
        valid_range = 16
    if platform == "mavericks":
        valid_range = 15
    if platform == "montara":
        valid_range = 12

    with Capturing() as screen_op:
        btools.main(arg)
    data = str(screen_op)

    # if error while data collection
    if err_status in data:
        err = find_err_status(data)
    output.append(err)

    try:
        t = re.findall('\d+\.\d+', data)
        for i in range (0, valid_range):
            l.append(float(t[i]))
        for i in range(0, valid_range):
            output.append(int(l[i] * 1000))
    except Exception as e:
        #fill all 0 when error
        output = []
        if len(t) != valid_range:
            #If the information is incomplete, err = 3
            err = 3
        output.append(err)
        for i in range(0, valid_range):
            output.append(int(0))
        pass

    result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }

    lock.release()
    return result;

def run_btools(p_len, param1, param2, param3, param4):

    lock.acquire()
    output = []

    #Parsing input parameters
    target="--%s" % (str(param1).upper())
    arg1=str(param2)
    arg2=str(param3)
    arg3=str(param4)

    if p_len == 0:
        arg = ['btools.py', target]
        btools_cmd="btools.py %s help" % (target)
    elif p_len == 1:
        arg = ['btools.py', target, arg1]
        btools_cmd="btools.py %s %s" % (target, arg1)
    elif p_len == 2:
        arg = ['btools.py', target, arg1, arg2]
        btools_cmd="btools.py %s %s %s" % (target, arg1, arg2)
    elif p_len == 3:
        arg = ['btools.py', target, arg1, arg2, arg3]
        btools_cmd="btools.py %s %s %s %s" % (target, arg1, arg2, arg3)

    #Main
    with Capturing() as screen_op:
        btools.main(arg)
    data = str(screen_op)

    #Output
    output.append(data)
    result = {
                "Information": {"Command": btools_cmd, "Result": output},
                "Actions": [],
                "Resources": [],
             }

    lock.release()
    return result;

def get_bmc_ps_feature(param1, param2):

    lock.acquire()
    output = []
    if param2 == "presence":
        try:
            r = btools.psu_check_pwr_presence(int(param1))
            output.append(int(r))
        except Exception as e:
            output.append("Check pwr presence error")
            pass

    result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }

    lock.release()
    return result;


def get_bmc_ps_old(param1):

    lock.acquire()
    l = []
    j = []
    output = []
    load_sharing = []
    err = [0] * 11
    err_status = "exit status"
    not_present = "not_present"
    try:
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
            lock.release()
            return result;
    except Exception as e:
        print("get presence error:")
        print(e)
        # 1. error status
        output.append("1")
        output.append("read present fail")
        result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }
        lock.release()
        return result;

    # input voltage data
    arg = ['btools.py', '--PSU', '1', 'r', 'v']
    arg[2] = str(param1)

    with Capturing() as screen_op:
        btools.main(arg)
    data = str(screen_op)

    # if error while data collection
    if err_status in data:
        err[0] = find_err_status(data)

    try:
        t = re.findall('\d+\.\d+', data)
        l.append(float(t[0]))
    except Exception as e:
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
    try:
        t = re.findall('\d+\.\d+', data)
        l.append(float(t[0]))
    except Exception as e:
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

    try:
        t = re.findall('\d+\.\d+', data)
        l.append(float(t[0]))
    except Exception as e:
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

    try:
        t = re.findall('\d+\.\d+', data)
        l.append(float(t[0]))
    except Exception as e:
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

    try:
        t = re.findall('\d+', data)
        l.append(float(t[0]))
    except Exception as e:
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

    try:
        t = re.findall('\d+', data)
        l.append(float(t[0]))
    except Exception as e:
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

    # if current is shared between supplies then load sharing
    # is true
    try :
        t = re.findall('\d+\.\d+', data)
        if float(t[0]) > 0.0 and float(t[1]) > 0.0 :
            l.append(float(1))
        else :
            l.append(float(0))
    except Exception as e:
        l.append(float(0))


    # ps model
    arg[4] = 'psmodel'
    with Capturing() as screen_op:
        btools.main(arg)
    data = str(screen_op)

    # if error while data collection
    if err_status in data:
        err[8] = find_err_status(data)

    try:
        t = re.findall('[\w\.-]+', data)
        j.append(t[0])
    except Exception as e:
        j.append("Error")
        pass

    # ps serial
    arg[4] = 'psserial'
    with Capturing() as screen_op:
        btools.main(arg)
    data = str(screen_op)

    # if error while data collection
    if err_status in data:
        err[9] = find_err_status(data)

    try:
        t = re.findall('[\w\.-]+', data)
        j.append(t[0])
    except Exception as e:
        j.append("Error")
        pass

    # ps verion
    arg[4] = 'psrev'
    with Capturing() as screen_op:
        btools.main(arg)
    data = str(screen_op)

    # if error while data collection
    if err_status in data:
      err[10] = find_err_status(data)

    try:
        t = re.findall('[\w\.-]+', data)
        j.append(t[0])
    except Exception as e:
        j.append("Error")
        pass

    #if err is present append it to output
    a = 0
    for x in err:
        if x != 0:
            a = x
            break

    output.append(a)

    for x in l:
        output.append(int(x))
    for x in j:
        output.append(x)

    result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }

    lock.release()
    return result;

def get_bmc_ps(param1):
    vlist = [0, 0, 0, 0, 0, 0, 0, 0,  'Error', 'Error', 'Error']
    lock.acquire()
    output = []
    err = [1] * 11
    try:
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
            lock.release()
            return result;
    except Exception as e:
        print("get presence error:")
        print(e)
        # 1. error status
        output.append("1")
        output.append("read present fail")
        result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }
        lock.release()
        return result;

    # input voltage data
    arg = ['btools.py', '--PSU', '1', 'a', 'v']
    arg[2] = str(param1)

    with Capturing() as screen_op:
        btools.main(arg)
    data = str(screen_op)

    for line in data.split(','):
        if line.find('in0_input') != -1:
          try:
            t = re.findall('\d+\.\d+', line[len('in0_input'):])
            vlist[0] = float(t[0])
            err[0] = 0
          except Exception as e:
            pass
        elif line.find('in1_input') != -1:
          try:
            t = re.findall('\d+\.\d+', line[len('in1_input'):])
            vlist[1] = float(t[0])
            err[1] = 0
          except Exception as e:
            pass
        elif line.find('curr1_input') != -1:
          try:
            t = re.findall('\d+\.\d+', line[len('curr1_input'):])
            vlist[2] = float(t[0])
            err[2] = 0
          except Exception as e:
            pass
        elif line.find('power1_input') != -1:
          try:
            t = re.findall('\d+\.\d+', line[len('power1_input'):])
            vlist[3] = float(t[0])
            err[3] = 0
          except Exception as e:
            pass
        elif line.find('fan1_input') != -1:
          try:
            t = re.findall('\d+', line[len('fan1_input'):])
            vlist[4] = int(t[0])
            err[4] = 0
          except Exception as e:
            pass
        elif line.find('fan1_fault') != -1:
          try:
            t = re.findall('\d+', line[len('fan1_fault'):])
            vlist[5] = int(t[0])
            err[5] = 0
          except Exception as e:
            pass
        elif line.find('present') != -1:
          try:
            if line.find('error') != -1:
              vlist[6] = 0
            elif line.find('not') != -1:
              err[6] = 0
            else:
              vlist[6] = 1
              err[6] = 0
          except Exception as e:
            pass
        elif line.find('curr2_input') != -1:
          try:
            if line.find('error') != -1:
              vlist[7] = 0
            else:
              t = re.findall('\d+', line[len('curr2_input'):])
              vlist[7] = int(t[0])
              err[7] = 0
          except Exception as e:
            pass
        elif line.find('mfr_model_label') != -1:
          try:
            aline = line.split(': ', 1)
            t = re.findall('[\w\.-]+', aline[1])
            vlist[8] = str(t[0])
            err[8] = 0
          except Exception as e:
            pass
        elif line.find('mfr_serial_label') != -1:
          try:
            aline = line.split(': ', 1)
            t = re.findall('[\w\.-]+', aline[1])
            vlist[9] = str(t[0])
            err[9] = 0
          except Exception as e:
            pass
        elif line.find('mfr_revision') != -1:
          try:
            aline = line.split(': ', 1)
            t = re.findall('[\w\.-]+', aline[1])
            vlist[10] = str(t[0])
            err[10] = 0
          except Exception as e:
            pass

    #if err is present append it to output
    a = 0
    for x in err:
        if x != 0:
            a = x
            break

    output.append(a)

    for i in range(8):
        output.append(int(vlist[i]))
    for j in range(8, 11):
        output.append(vlist[j])

    result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }

    lock.release()
    return result;

def get_fan_present(param1):
    p1=-1
    cmd = "/usr/local/bin/get_fantray_present.sh"
    data = Popen(cmd, \
                       shell=True, stdout=PIPE).stdout.read()
    try:
      t = re.findall(str(param1)+' present: 0x[0-9A-F]+', data, re.I)
      v = re.findall(r'0x[0-9A-F]+', t[0], re.I)
      p1=int(v[0],base=16)
    except Exception as e:
      return -1

    return p1

def get_bmc_fan(param1):

    output = []
    err = 0
    errnum = 0
    error = ["error", "Error", "ERROR"]
    fantray_present=1

    platform = btools.get_project()
    cmd = "/usr/local/bin/get_fan_speed.sh %s" % param1
    data = Popen(cmd, \
                       shell=True, stdout=PIPE).stdout.read()

    # if error while data collection
    if any(x in data for x in error):
        err = 1

    if param1 == "Mavericks" or param1 == "Montara" or param1 == "Newport" or param1 == "Stinson" or param1 == "Davenport":
      num = 0
    else:
      try:
        num = int(param1)
      except Exception as e:
        print(e)
        # 1. error status
        output.append(1)
        output.append(param1)
        for i in range(3):
          output.append(int(0))
        result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }
        output.append(int(fantray_present))
        return result;

    if platform == "newport" or platform == "stinson" or platform == "davenport":
        data1 = data
        cmd = "/usr/local/bin/get_fantray_present.sh"
        data = Popen(cmd, \
                       shell=True, stdout=PIPE).stdout.read()
        t = re.findall('\d+', data)
        length=len(t)
        i=1
        while i < length:
            fantray_present= fantray_present & int(t[i])
            i += 2
        data = data1
    elif platform == "mavericks" or platform == "mavericks-p0c":
        if param1 == "Mavericks":
          value2 = get_fan_present("Fantray_upper")
          value1 = get_fan_present("Fantray")
          if value1 == -1 or value2 == -1:
            err = 1
            errnum = 11
          elif value1 == 0 and value2 == 0:
            fantray_present=0
        elif (num <= 10) and (num > 5):
          value = get_fan_present("Fantray_upper")
          if value == -1:
            err = 1
          elif (value & (1 << (num - 6))) == 0:
            fantray_present=0
        elif (num <= 5) and (num > 0):
          value = get_fan_present("Fantray")
          if value == -1:
            err = 1
          elif (value & (1 << (num - 1))) == 0:
            fantray_present=0
        else:
          err = 1
    elif platform == "montara":
        value = get_fan_present("Fantray")
        if param1 == "Montara":
          if value == -1:
            err = 1
            errnum = 6
          elif value == 0:
            fantray_present=0
        elif (num > 0) and (num <= 5):
          if value == -1:
            err = 1
          elif (value & (1 << (num -1))) == 0:
            fantray_present=0
        else:
          err = 1
    else:
        err = 1

    output.append(err)

    if errnum > 0:
      for num in range(1, errnum):
        output.append(num)
        for i in range(3):
          output.append(int(0))
    elif err == 1:
      output.append(param1)
      for i in range(3):
        output.append(int(0))
    else:
      t = re.findall('\d+', data)
      for x in t:
        output.append(int(x))

    output.append(int(fantray_present))

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

    platform = btools.get_project()
    if platform == "newport" or platform == "stinson" or platform == "davenport":
        cmd = "/usr/local/bin/set_fan_speed.sh %s" % (param3)
    else:
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


def get_bmc_fan_led(param1):
    output = []
    err = 0
    error = ["error", "Error", "ERROR"]

    platform = btools.get_project()
    if platform == "mavericks-p0c" or platform == "mavericks":
        platform = "Mavericks"
    try:
      cmd = "/usr/local/bin/get_fan_led.sh %s %s" % (param1, platform.capitalize())
      data = Popen(cmd, shell=True, stdout=PIPE).stdout.read()
      t = re.findall('0x\d+', data)
      if len(t) == 2:
        output.append(err)
        for x in t:
          output.append(x)
      else:
        err = 1
        output.append(err)
    except Exception as e:
        err = 1
        output.append(err)

    result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }

    return result;

def set_bmc_fan_led(param1, param2, param3):

    output = []
    error = ["error", "Error", "ERROR"]
    err = 0

    platform = btools.get_project()
    if platform == "mavericks-p0c" or platform == "mavericks":
        platform = "Mavericks"
    try:
      cmd = "/usr/local/bin/set_fan_led.sh %s %s %s %s" % (param1, param2, param3, platform.capitalize())
      data = Popen(cmd, shell=True, stdout=PIPE).stdout.read()
      t = re.findall('0x0', data)
      if len(t) == 0:
        err = 1
    except Exception as e:
        err = 1
    output.append(err)
    result = {
                "Information": {"Description": output},
                "Actions": [],
                "Resources": [],
             }

    return result;

def set_bmc_all_fan(param1):

    output = []
    error = ["error", "Error", "ERROR"]
    err = 0

    cmd = "/usr/local/bin/set_fan_speed.sh %s" % (param1)

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

