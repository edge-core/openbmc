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

import json
import re
import subprocess
import bmc_command
import syslog

sensor_cache_data = "com_e_driver-i2c-4-33\nAdapter: ast_i2c.4\nCPU Vcore:      +1.80 V  (min =  +1.71 V, max =  +1.89 V)\n+3V Voltage:    +3.28 V  (min =  +3.00 V, max =  +3.60 V)\n+5V Voltage:    +5.07 V  (min =  +4.50 V, max =  +5.50 V)\n+12V Voltage:  +12.08 V  (min = +10.80 V, max = +13.20 V)\nVDIMM Voltage:  +1.21 V  (min =  +1.14 V, max =  +1.26 V)\nCPU Temp:       +40.0 C  (low  =  +0.0 C, high = +85.0 C)\nMemory Temp:    +29.3 C  (low  =  +0.0 C, high = +85.0 C)\n\nfancpld-i2c-8-33\nAdapter: ast_i2c.8\nFan 1 front: 6150 RPM  (min =    0 RPM)\nFan 1 rear:  3750 RPM  (min =    0 RPM)\nFan 2 front: 6150 RPM  (min =    0 RPM)\nFan 2 rear:  3900 RPM  (min =    0 RPM)\nFan 3 front: 6150 RPM  (min =    0 RPM)\nFan 3 rear:  3900 RPM  (min =    0 RPM)\nFan 4 front: 6150 RPM  (min =    0 RPM)\nFan 4 rear:  3900 RPM  (min =    0 RPM)\nFan 5 front: 6150 RPM  (min =    0 RPM)\nFan 5 rear:  3900 RPM  (min =    0 RPM)\n\nfancpld-i2c-9-33\nAdapter: ast_i2c.9\nFan 6 front:  7500 RPM  (min =    0 RPM)\nFan 6 rear:   4950 RPM  (min =    0 RPM)\nFan 7 front:  7500 RPM  (min =    0 RPM)\nFan 7 rear:   4950 RPM  (min =    0 RPM)\nFan 8 front:  7500 RPM  (min =    0 RPM)\nFan 8 rear:   4950 RPM  (min =    0 RPM)\nFan 9 front:  7500 RPM  (min =    0 RPM)\nFan 9 rear:   4950 RPM  (min =    0 RPM)\nFan 10 front: 7500 RPM  (min =    0 RPM)\nFan 10 rear:  4950 RPM  (min =    0 RPM)\n\nltc4151-i2c-7-6f\nAdapter: ast_i2c.7\nvout1:            N/A  \niout1:            N/A  \n\npsu_driver-i2c-7-59\nAdapter: ast_i2c.7\nPSU2 Input Voltage:         +239.00 V  \nPSU2 12V Output Voltage:    +12.11 V  \nPSU2 5/3.3V Output Voltage:  +3.32 V  \nPSU2 FAN:                   6000 RPM\nPSU2 Temp1:                  +30.0 C  \nPSU2 Temp2:                  +31.0 C  \nPSU2 Temp3:                  +37.0 C  \nPSU2 Input Power:           121.38 W  \nPSU2 12V Output Power:      108.62 W  \nPSU2 5/3.3V Output Power:     0.00 W  \nPSU2 Input Current:          +0.51 A  \nPSU2 12V Output Current:     +8.84 A  \nPSU2 5/3.3V Output Current:  +0.00 A  \n\npsu_driver-i2c-7-5a\nAdapter: ast_i2c.7\nPSU1 Input Voltage:         +239.25 V  \nPSU1 12V Output Voltage:    +12.11 V  \nPSU1 5/3.3V Output Voltage:  +3.30 V  \nPSU1 FAN:                   5968 RPM\nPSU1 Temp1:                  +32.0 C  \nPSU1 Temp2:                  +31.0 C  \nPSU1 Temp3:                  +37.0 C  \nPSU1 Input Power:           130.75 W  \nPSU1 12V Output Power:      116.38 W  \nPSU1 5/3.3V Output Power:     0.00 W  \nPSU1 Input Current:          +0.54 A  \nPSU1 12V Output Current:     +9.75 A  \nPSU1 5/3.3V Output Current:  +0.00 A  \n\ntmp75-i2c-3-48\nAdapter: ast_i2c.3\nChip Temp:    +38.0 C  (high = +80.0 C, hyst = +75.0 C)\n\ntmp75-i2c-3-49\nAdapter: ast_i2c.3\nExhaust2 Temp:  +28.5 C  (high = +80.0 C, hyst = +75.0 C)\n\ntmp75-i2c-3-4a\nAdapter: ast_i2c.3\nExhaust Temp:  +25.5 C  (high = +80.0 C, hyst = +75.0 C)\n\ntmp75-i2c-3-4b\nAdapter: ast_i2c.3\nIntake Temp:  +33.0 C  (high = +80.0 C, hyst = +75.0 C)\n\ntmp75-i2c-3-4c\nAdapter: ast_i2c.3\nIntake2 Temp:  +25.5 C  (high = +80.0 C, hyst = +75.0 C)\n\ntmp75-i2c-8-48\nAdapter: ast_i2c.8\nFan Board Outlet Right Temp:  +24.0 C  (high = +80.0 C, hyst = +75.0 C)\n\ntmp75-i2c-8-49\nAdapter: ast_i2c.8\nFan Board Outlet Left Temp:  +25.0 C  (high = +80.0 C, hyst = +75.0 C)\n\ntmp75-i2c-9-4a\nAdapter: ast_i2c.9\nUpper board Intake Temp:  +23.0 C  (high = +80.0 C, hyst = +75.0 C)\n\ntmp75-i2c-9-4b\nAdapter: ast_i2c.9\nUpper board Tofino Temp:  +31.0 C  (high = +80.0 C, hyst = +75.0 C)\n\nmax6658-i2c-9-4c\nAdapter: ast_i2c.9\nMax6658 Chip temp:  +32.1 C  (low  = -55.0 C, high = +70.0 C)\n                             (crit = +85.0 C, hyst = +75.0 C)\nCOMe Board temp:    +44.2 C  (low  = -55.0 C, high = +70.0 C)\n                             (crit = +85.0 C, hyst = +75.0 C)\n\n"
# Handler for sensors resource endpoint
def get_sensors():
    global sensor_cache_data
    result = []
    proc = subprocess.Popen(['sensors'],
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    for i in range(3):
        try:
            data, err = bmc_command.timed_communicate(proc)
            if len(data) != 0:
                sensor_cache_data = data
                break
        except bmc_command.TimeoutError as ex:
            data = ex.output
            err = ex.error

    if len(data) == 0:
        syslog.syslog(syslog.LOG_WARNING,"use sensor cache data")
        data = sensor_cache_data

    data = re.sub(r'\(.+?\)', '', data)
    for edata in data.split('\n\n'):
        adata = edata.split('\n', 1)
        sresult = {}
        if (len(adata) < 2):
            break;
        sresult['name'] = adata[0]
        for sdata in adata[1].split('\n'):
            tdata = sdata.split(':')
            if (len(tdata) < 2):
                continue
            sresult[tdata[0].strip()] = tdata[1].strip()
        result.append(sresult)

    fresult = {
                "Information": result,
                "Actions": [],
                "Resources": [],
              }
    return fresult
