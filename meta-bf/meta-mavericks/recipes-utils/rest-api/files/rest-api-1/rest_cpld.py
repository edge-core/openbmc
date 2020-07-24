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

import subprocess

# Endpoint for performing cpld register on wedge100


def get_reg(param1, param2):
    cmd = "/usr/sbin/i2cget -y -f 12 %s %s" % (param1, param2)
    register = ''
    p = subprocess.Popen(cmd,
                         shell=True,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    out, err = p.communicate()
    rc = p.returncode

    if rc < 0:
        status = 'failed with returncode = ' + str(rc) + ' and error ' + err
    else:
        status = 'done'
        register = out.split()[0]
    result = {
                "Information": {"status" : status, "register" : register},
                "Actions": [],
                "Resources": [],
             }
    return result

def set_reg(param1, param2, param3):
    cmd = "/usr/sbin/i2cset -y -f 12 %s %s %s" % (param1, param2, param3)
    p = subprocess.Popen(cmd,
                         shell=True,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    out, err = p.communicate()
    rc = p.returncode

    if rc < 0:
        status = 'failed with returncode = ' + str(rc) + ' and error ' + err
    else:
        status = 'done'
    result = {
                "Information": {"status" : status},
                "Actions": [],
                "Resources": [],
             }
    return result
