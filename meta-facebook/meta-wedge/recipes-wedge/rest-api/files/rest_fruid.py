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


from ctypes import *

def get_fruid():
    assembly_number = ""
    
    import subprocess
    val = subprocess.Popen(['/usr/sbin/i2cset', '-f', '-y', '6', '0x51', '0x00', '0x17', 'i'], stdout=subprocess.PIPE).communicate()

    for num in range(0,11):
        val = subprocess.Popen(['/usr/sbin/i2cget', '-f', '-y', '6', '0x51'], stdout=subprocess.PIPE).communicate()
        temp = val[0][2], val[0][3]
        res = str(int(''.join(temp)) - 30)
        assembly_number += res
    return assembly_number 
