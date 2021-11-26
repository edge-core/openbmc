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
import bottle
import rest_usb2i2c_reset
import rest_i2cflush
import rest_cpld

boardApp = bottle.Bottle()

# Disable the endpoint in BMC until we root cause cp2112 issues.
# Handler to reset usb-to-i2c
#@boardApp.route('/api/sys/usb2i2c_reset')
#def rest_usb2i2c_reset_hdl():
#    return rest_usb2i2c_reset.set_usb2i2c()

@boardApp.route('/api/sys/i2cflush')
def rest_i2cflush_hdl():
    return rest_i2cflush.i2cflush()

@boardApp.route('/api/sys/cpldget/<param1>/<param2>')
def rest_cpld_get(param1, param2):
    return rest_cpld.get_reg(param1, param2)

@boardApp.route('/api/sys/cpldset/<param1>/<param2>/<param3>')
def rest_cpld_set(param1, param2, param3):
    return rest_cpld.set_reg(param1, param2, param3)

@boardApp.route('/api/sys/fancpldget/<param1>/<param2>/<param3>')
def rest_cpld_get(param1, param2, param3):
    return rest_cpld.get_fan_reg(param1, param2, param3)

@boardApp.route('/api/sys/fancpldset/<param1>/<param2>/<param3>/<param4>')
def rest_cpld_set(param1, param2, param3, param4):
    return rest_cpld.set_fan_reg(param1, param2, param3, param4)