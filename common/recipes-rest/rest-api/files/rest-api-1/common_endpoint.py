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
import rest_fruid
import rest_server
import rest_sensors
import rest_bmc
import rest_gpios
import rest_modbus
import rest_slotid
import rest_psu_update
import rest_fcpresent
import bottle

commonApp = bottle.Bottle()


# Handler for root resource endpoint
@commonApp.route('/api')
def rest_api():
    result = {
        "Information": {
            "Description": "Wedge RESTful API Entry",
        },
        "Actions": [],
        "Resources": ["sys"],
    }
    return result


# Handler for sys resource endpoint
@commonApp.route('/api/sys')
def rest_sys():
    result = {
        "Information": {
            "Description": "Wedge System",
        },
        "Actions": [],
        "Resources": ["mb", "bmc", "server", "sensors", "gpios",
                      "modbus_registers", "slotid"],
    }
    return result


# Handler for sys/mb resource endpoint
@commonApp.route('/api/sys/mb')
def rest_mb_sys():
    result = {
        "Information": {
            "Description": "System Motherboard",
        },
        "Actions": [],
        "Resources": ["fruid"],
    }
    return result


# Handler for sys/mb/fruid resource endpoint
@commonApp.route('/api/sys/mb/fruid')
def rest_fruid_hdl():
    return rest_fruid.get_fruid()

# Handler for sys/bmc resource endpoint
@bottle.route('/api/sys/bmc')
def rest_bmc_hdl():
    return rest_bmc.get_bmc()

# Handler for sys/bmc/tmp resource endpoint
@bottle.route('/api/sys/bmc/tmp/<param1>')
def rest_bmc_tmp_hdl(param1):
    return rest_bmc.get_bmc_tmp(param1)

# Handler for sys/bmc/ps resource endpoint
@bottle.route('/api/sys/bmc/ps/<param1>')
def rest_bmc_ps_hdl(param1):
    return rest_bmc.get_bmc_ps(param1)

# Handler for sys/bmc/ps_feature resource endpoint
@bottle.route('/api/sys/bmc/ps_feature/<param1>/<param2>')
def rest_bmc_ps_feature_hdl(param1, param2):
    return rest_bmc.get_bmc_ps_feature(param1, param2)

# Handler for sys/bmc/ucd resource endpoint
@bottle.route('/api/sys/bmc/ucd')
def rest_bmc_ucd_hdl():
    return rest_bmc.get_bmc_ucd()

# Handler for sys/bmc/fan/set resource endpoint
@bottle.route('/api/sys/bmc/fan/set/<param1>/<param2>/<param3>')
def rest_bmc_fan_set_hdl(param1, param2, param3):
    return rest_bmc.set_bmc_fan(param1, param2, param3)

# Handler for sys/bmc/fan/get resource endpoint
@bottle.route('/api/sys/bmc/fan/get/<param1>')
def rest_bmc_fan_get_hdl(param1):
    return rest_bmc.get_bmc_fan(param1)

# Handler for sys/server resource endpoint
@commonApp.route('/api/sys/server')
def rest_server_hdl():
    return rest_server.get_server()


# Handler for uServer resource endpoint
@commonApp.route('/api/sys/server', method='POST')
def rest_server_act_hdl():
    data = json.load(bottle.request.body)
    return rest_server.server_action(data)


# Handler for sensors resource endpoint
@commonApp.route('/api/sys/sensors')
def rest_sensors_hdl():
    return rest_sensors.get_sensors()


# Handler for gpios resource endpoint
@commonApp.route('/api/sys/gpios')
def rest_gpios_hdl():
    return rest_gpios.get_gpios()


# Handler for peer FC presence resource endpoint
@commonApp.route('/api/sys/fc_present')
def rest_fcpresent_hdl():
    return rest_fcpresent.get_fcpresent()


@commonApp.route('/api/sys/modbus_registers')
def modbus_registers_hdl():
    return rest_modbus.get_modbus_registers()


@commonApp.route('/api/sys/psu_update')
def psu_update_hdl():
    return rest_psu_update.get_jobs()


@commonApp.route('/api/sys/psu_update', method='POST')
def psu_update_hdl_post():
    data = json.load(bottle.request.body)
    return rest_psu_update.begin_job(data)


# Handler for get slotid from endpoint
@commonApp.route('/api/sys/slotid')
def rest_slotid_hdl():
    return rest_slotid.get_slotid()
