#!/usr/bin/python
#
# File is the tool for bringup of tofino chipset on Mavericks, Montara and Newport.
#
import sys
import os
import getopt
import subprocess
import bmc_command
import os.path
from time import sleep
import syslog
import fcntl

h_platforms = "montara/mavericks/newport"
h_platforms_with_p0c = "montara/mavericks/mavericks-p0c/newport/stinson"

nolimit_ir_vdd_core = 0

#
# btool usage for modules. Individual module usage is printed separately
#
def usage():

    print " "
    print "USAGE:"
    print "./btools.py --<device>"
    print "              PSU  => psu_driver power supply unit"
    print "              UCD  => UCD90120A power supply sequencer"
    print "              IR   => Multiphase Controller"
    print "              TMP  => Temperature Sensors"
    print "./btools.py --help"
    print " "
    print "Eg."
    print "btools.py --PSU help"
    print "btools.py --UCD help"
    print "btools.py --IR help"
    print "btools.py --TMP help"
    print "btools.py --help"
    print " "

    return
#
# Read board_type of FRU EEPROM
#
def get_project_by_weutil(cmd=['weutil']):
    proc = subprocess.Popen(cmd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    try:
        data, err = bmc_command.timed_communicate(proc)
    except bmc_command.TimeoutError as ex:
        data = ex.output
        err = ex.error

    # need to remove the first info line from weutil
    adata = data.split('\n', 1)
    for sdata in adata[1].split('\n'):
        tdata = sdata.split(':', 1)
        if (len(tdata) < 2):
            continue
        if tdata[0].strip() == "System Assembly Part Number" :
            file = open("/tmp/eeprom_sys_assembly_pn", "w+")
            file.write(sdata+'\n')
            file.close()
        if tdata[0].strip() == "Location on Fabric" :
            file = open("/tmp/eeprom_board_type", "w+")
            file.write(sdata+'\n')
            file.close()
            return tdata[1].strip()

    print "Error occured while capturing Location on Fabric"
    return

def get_sys_assembly_pn():
    if (not os.path.isfile("/tmp/eeprom_sys_assembly_pn")) :
        print "Error: /tmp/eeprom_sys_assembly_pn not found"
        board_type = get_project_by_weutil()

    try:
        tmp_data = subprocess.check_output(["cat", "/tmp/eeprom_sys_assembly_pn"])
    except subprocess.CalledProcessError as e:
        print e
        print "Error while reading /tmp/eeprom_sys_assembly_pn"
        return

    return tmp_data

def get_project():
    if (not os.path.isfile("/tmp/eeprom_board_type")) :
        print "Error: /tmp/eeprom_board_type not found"
        board_type = get_project_by_weutil()
    else:
        try:
            tmp_data = subprocess.check_output(["cat", "/tmp/eeprom_board_type"])
        except subprocess.CalledProcessError as e:
            print e
            print "Error while reading /tmp/eeprom_board_type"
            return
        tmp_board_type=tmp_data.split(':', 1)
        if len(tmp_board_type) >= 2 :
            board_type = tmp_board_type[1].strip()
        else:
            board_type = tmp_board_type[0].strip()

    if board_type.lower() == "montara":
        board_type = "montara"
    elif board_type.lower() == "mavericks" or board_type.lower() == "maverick":
        board_type = "mavericks"
    elif board_type.lower() == "mavericks-p0c":
        board_type = "mavericks-p0c"
    elif board_type.lower() == "newports" or board_type.lower() == "newport":
        board_type = "newport"
    elif board_type.lower() == "davenpor":
        board_type = "davenport"
    elif board_type.lower() == "pescader":
        board_type = "pescadero"
    elif board_type.lower() == "stinson":
        board_type = "stinson"
    else:
        print "Error: undefined board type [%s], defaulting to Montara" % board_type
        board_type = "montara"

    if board_type == "mavericks":
        sys_pn = get_sys_assembly_pn()
        if sys_pn.find("015-000001-02") > 0:
            board_type = "mavericks-p0c"

    return board_type

#
# Usage for PSU related arguments
#
def error_psu_usage():

    print " "
    print "USAGE:"
    print "./btools.py --PSU <power supply number> r v              => input voltage"
    print "                  <1 - 2>               r vo             => output voltage"
    print "                                        r i              => current"
    print "                                        r p              => power"
    print "                                        r ld             => load sharing"
    print "                                        r fspeed         => fan speed"
    print "                                        r ffault         => fan fault"
    print "                                        r presence       => power supply presence"
    print "                                        r sts_in_power   => power input status"
    print "                                        r sts_op_power   => power output status"
    print " "
    print "Eg."
    print "btools.py --PSU 1 r v   => Read input voltage for power supply 1"
    print " "

    return
#
# Presence and power status is read from CPLD
#
def psu_cpld_features(power_supply, feature):

    cpld_dev = "/sys/class/i2c-adapter/i2c-12/12-0031/"
    cmd = "cat"

    if feature == "presence" or feature == "presence_2":
        if power_supply == 1:
            path = cpld_dev + "psu1_present"
        elif power_supply == 2:
            path = cpld_dev + "psu2_present"
        else:
            error_psu_usage()
            return -1
    elif feature == "sts_in_power":
        if power_supply == 1:
            path = cpld_dev + "psu1_in_pwr_sts"
        elif power_supply == 2:
            path = cpld_dev + "psu2_in_pwr_sts"
        else:
            error_psu_usage()
            return -1
    elif feature == "sts_op_power":
        if power_supply == 1:
            path = cpld_dev + "psu1_output_pwr_sts"
        elif power_supply == 2:
            path = cpld_dev + "psu2_output_pwr_sts"
        else:
            error_psu_usage()
            return -1
    else:
        error_psu_usage()
        return -1

    try:
        output = subprocess.check_output([cmd, path])
    except subprocess.CalledProcessError as e:
        print e
        print "Error while executing psu cpld feature commands"
        return -1

    if feature == "presence":
        res = int(output, 16)
        if res == 0:
            print "Power supply %s present" % power_supply
            return 0
        else:
            print "Power supply %s not present" % power_supply
            return 1
    elif feature == "sts_in_power" or feature == "sts_op_power":
        # catching only first 3 characters of output
        res = int(output[:3], 16)
        if res == 0:
            print "Power supply status: BAD"
        elif res == 1:
            print "Power supply status: OK"
        else:
            print "Error while reading power supply status"
        return 0
    elif feature == "presence_2":
        res = int(output, 16)
        if res == 0:
            return 0
        else:
            return 1
    return -1

#
#open I2C sw before pfe devices and then load drivers
#
def psu_init():

    #check if psu_driver driver is loaded properly
    if os.path.isfile("/sys/class/i2c-adapter/i2c-7/7-0059/in1_input") \
       and os.path.isfile("/sys/class/i2c-adapter/i2c-7/7-005a/in1_input"):
        return 0

    try:
        cmd = "i2cset"
        I2C_ADDR = "0x70"
        I2C_BUS = "7"
        OPCODE_ON = "0x3"
        OPCODE_OFF = "0x0"

        # Open I2C swtich for PFE devices
        subprocess.check_output([cmd, "-f", "-y", I2C_BUS, I2C_ADDR, OPCODE_OFF])
        subprocess.check_output([cmd, "-f", "-y", I2C_BUS, I2C_ADDR, OPCODE_ON])

        # load driver for both devices
        o = subprocess.check_output(["lsmod", "psu_driver"])
        rtn = o.find("psu_driver")

        if rtn != -1:
            # load driver for both devices
            subprocess.check_output(["rmmod", "psu_driver"])

        # load driver for both devices
        subprocess.check_output(["modprobe", "psu_driver"])

    except subprocess.CalledProcessError as e:
        print e
        print "Error occured while initializing PSU"
        return -1

#function just for power supply check
def psu_check_pwr_presence(power_supply):

  s = psu_init()
  if s == -1:
      return -1

  r = psu_cpld_features(power_supply, "presence")

  return r

def psu_cpld_present(power_supply):
    cpld_dev = "/sys/class/i2c-adapter/i2c-12/12-0031/"
    cmd = "cat"
    if power_supply == 1:
        path = cpld_dev + "psu1_present"
    elif power_supply == 2:
        path = cpld_dev + "psu2_present"
    else:
        return -1

    try:
        output = subprocess.check_output([cmd, path])
        res = int(output, 16)
        if res == 0:
            return 0
        else:
            return 1
    except subprocess.CalledProcessError as e:
        return -1

def psu_get_ld(param):
    i2c_dev = "/sys/class/i2c-adapter/i2c-7/7-00"
    platform = get_project()
    val = param
    try:
        # load sharing checking
        if val == "curr2_input":
            if platform == "newport":
                output = subprocess.check_output(["i2cget", "-f", "-y", "12", "0x31", "0x10"])
                psu_sts = int(output, 16)
                ps1_sts=((psu_sts>>4)&0x7)
                ps2_sts=(psu_sts&0x7)
            else: #SYSCPLD Offset 0x10 definition for PSU1 and PSU2 on other projects is contrary, so temporarily skip to comply with the origin.
                ps1_sts=0x06
                ps2_sts=0x06

            if ps1_sts == 0x6:
                ps = "5a/"
                path = i2c_dev + ps + val
                r = psu_cpld_present(1)
                if int(r) == 0:
                  try:
                    output = subprocess.check_output(["cat", path])
                    a=int(output)
                  except subprocess.CalledProcessError as e:
                    return -1
                else:
                   a=0
            else:
                a=0
            if ps2_sts == 0x6:
                ps = "59/"
                path = i2c_dev + ps + val
                r = psu_cpld_present(2)
                if int(r) == 0:
                  try:
                    output = subprocess.check_output(["cat", path])
                    b=int(output)
                  except subprocess.CalledProcessError as e:
                    return -1
                else:
                  b=0
            else:
                 b=0
    except subprocess.CalledProcessError as e:
        return -1

    if a > 0 and b > 0 :
        return 1
    else :
        return 0


def psu_all(argv):
    arg_psu = argv[2:]
    i2c_dev = "/sys/class/i2c-adapter/i2c-7/7-00"
    list = ['in0_input', 'in1_input', 'curr1_input', 'power1_input', 'fan1_input', 'fan1_fault', 'presence', 'curr2_input',  'mfr_model_label', 'mfr_serial_label', 'mfr_revision']
    unit = ['V', 'V', 'mA', 'mW', 'rpm', 'ffault', 'presence', 'A', 'model', 'serial', 'rev']
    # Mapping i2c bus address according to power supply number
    # 2018.09.10 Swap PSUs mapping because of reverse.
    if arg_psu[0] == "1":
        power_supply = 1
        ps = "5a/"
    elif arg_psu[0] == "2":
        power_supply = 2
        ps = "59/"

    try:
        I2C_ADDR = "0x70"
        I2C_BUS = "7"
        OPCODE_ON = "0x3"
        OPCODE_OFF = "0x0"
        # Force Open I2C swtich for PFE devices. Facebook psu mon messes up i2c mux
        subprocess.check_output(["i2cset", "-f", "-y", I2C_BUS, I2C_ADDR, OPCODE_OFF])
        subprocess.check_output(["i2cset", "-f", "-y", I2C_BUS, I2C_ADDR, OPCODE_ON])
    except subprocess.CalledProcessError as e:
        print "Error while executing psu i2c command "
        return -1

    for i in range(11):
        if list[i] == 'curr2_input':
            ld = psu_get_ld("curr2_input")
            if ld >= 0:
              print "{} : {} {}".format(list[i], int(ld), unit[i])
            else:
              print "{} : {}".format(list[i], "error")
        elif list[i] == 'presence':
            r = psu_cpld_features(power_supply, "presence")
            if r == -1:
              print "{} : {}".format("present", "error")
        else:
            path = i2c_dev + ps + list[i]
            try:
              output = subprocess.check_output(["cat", path])
              if unit[i] == "V":
                print "{} : {} {}".format(list[i], float(output) / 1000, unit[i])             # convert milli volts to volts
              elif unit[i] == "mA":
                print "{} : {} {}".format(list[i], float(output), unit[i])                   # current is in milli Amperes
              elif unit[i] == "mW":
                print "{} : {} {}".format(list[i], float(output), unit[i])                   # Power in milli watts
              elif unit[i] == "rpm":
                print "{} : {} {}".format(list[i], int(output), unit[i])                    # Speed of FAN
              elif unit[i] == "ffault":
                print "{} : {}".format(list[i], int(output))
              elif unit[i] == "model":
                print "{} : {}".format(list[i], output)
              elif unit[i] == "serial":
                print "{} : {}".format(list[i], output)
              elif unit[i] == "rev":
                print "{} : {}".format(list[i], output)
            except subprocess.CalledProcessError as e:
              print "{} : {}".format(list[i], "error")

#
# Function to handle PSU related requests
#
def psu(argv):

    i2c_dev = "/sys/class/i2c-adapter/i2c-7/7-00"

    arg_psu = argv[2:]

    if arg_psu[0] == "help" or arg_psu[0] == "h" or len(arg_psu) != 3:
        error_psu_usage()
        return

    if arg_psu[0] != "1" and arg_psu[0] != "2":
        error_psu_usage()
        return

    s = psu_init()
    if s == -1:
        error_psu_usage()
        return
    if arg_psu[1] == "a":
       psu_all(argv)
       return

    # Mapping i2c bus address according to power supply number
    # 2018.09.10 Swap PSUs mapping because of reverse.
    if arg_psu[0] == "1":
        power_supply = 1
        ps = "5a/"
    elif arg_psu[0] == "2":
        power_supply = 2
        ps = "59/"

    if arg_psu[1] == "r":
        cmd = "cat"
    else:
        error_psu_usage()
        return

    if arg_psu[2] == "v":
        val = "in0_input"
        s = "V"
    elif arg_psu[2] == "i":
        val = "curr1_input"
        s = "mA"
    elif arg_psu[2] == "p":
        val = "power1_input"
        s = "mW"
    elif arg_psu[2] == "fspeed":
        val = "fan1_input"
        s = "rpm"
    elif arg_psu[2] == "ffault":
        val = "fan1_fault"
        s = "ffault"
    elif arg_psu[2] == "presence":
        psu_cpld_features(power_supply, "presence")
        return
    elif arg_psu[2] == "sts_in_power":
        psu_cpld_features(power_supply, "sts_in_power")
        return
    elif arg_psu[2] == "sts_op_power":
        psu_cpld_features(power_supply, "sts_op_power")
        return
    elif arg_psu[2] == "vo":
        val = "in1_input"
        s = "V"
    elif arg_psu[2] == "ld":
        val = "curr2_input"
        s = "A"
    elif arg_psu[2] == "psmodel":
        val = "mfr_model_label"
        s = "model"
    elif arg_psu[2] == "psserial":
        val = "mfr_serial_label"
        s = "serial"
    elif arg_psu[2] == "psrev":
        val = "mfr_revision"
        s = "rev"
    else:
        error_psu_usage()
        return

    platform = get_project()
    path = i2c_dev + ps + val

    try:
        I2C_ADDR = "0x70"
        I2C_BUS = "7"
        OPCODE_ON = "0x3"
        OPCODE_OFF = "0x0"

        # Force Open I2C swtich for PFE devices. Facebook psu mon messes up i2c mux
        subprocess.check_output(["i2cset", "-f", "-y", I2C_BUS, I2C_ADDR, OPCODE_OFF])
        subprocess.check_output(["i2cset", "-f", "-y", I2C_BUS, I2C_ADDR, OPCODE_ON])

        # load sharing checking
        if val == "curr2_input":
            if platform == "newport":
                output = subprocess.check_output(["i2cget", "-f", "-y", "12", "0x31", "0x10"])
                psu_sts = int(output, 16)
                ps1_sts=((psu_sts>>4)&0x7)
                ps2_sts=(psu_sts&0x7)
            else: #SYSCPLD Offset 0x10 definition for PSU1 and PSU2 on other projects is contrary, so temporarily skip to comply with the origin.
                ps1_sts=0x06
                ps2_sts=0x06

            if ps1_sts == 0x6:
                ps = "5a/"
                path = i2c_dev + ps + val
                r = psu_cpld_features(1, "presence_2")
                if int(r) == 0:
                  try:
                    output = subprocess.check_output([cmd, path])
                    print "Power Supply 1 output current  %.3f amp" % (float(output)/1000) # unit: A
                  except subprocess.CalledProcessError as e:
                    print e
                else:
                  print "Power Supply 1 output current  %.3f amp" % (float(0)/1000) # unit: A

            else:
                print "Power Supply 1 output current  0.000 amp"
            if ps2_sts == 0x6:
                ps = "59/"
                path = i2c_dev + ps + val
                r = psu_cpld_features(2, "presence_2")
                if int(r) == 0:
                  try:
                    output = subprocess.check_output([cmd, path])
                    print "Power Supply 2 output current  %.3f amp" % (float(output)/1000) # unit: A
                  except subprocess.CalledProcessError as e:
                    print e
                else:
                  print "Power Supply 2 output current  %.3f amp" % (float(0)/1000) # unit: A
            else:
                print "Power Supply 2 output current  0.000 amp"
        else:
            output = subprocess.check_output([cmd, path])

    except subprocess.CalledProcessError as e:
        print e
        print "Error while executing psu i2c command "
        return
    try:
        if s == "V":
            print "{}{}".format(float(output) / 1000, "V")             # convert milli volts to volts
        elif s == "mA":
            print "{}{}".format(float(output), "mA")                   # current is in milli Amperes
        elif s == "mW":
            print "{}{}".format(float(output), "mW")                   # Power in milli watts
        elif s == "rpm":
            print "{}{}".format(int(output), "rpm")                    # Speed of FAN
        elif s == "ffault":
            print "{}".format(int(output))
        elif s == "model":
            print "{}".format(output)
        elif s == "serial":
            print "{}".format(output)
        elif s == "rev":
            print "{}".format(output)
        return
    except Exception as e:
        print e
        print "Error while format output "
        return
#
# Usage for UCD device
#
def error_ucd_usage():

    print " "
    print "Usage:"
    print "./btools.py --UCD sh v [%s]    => Show Voltage of all rails" % h_platforms_with_p0c
    print "                  fault    => Show Voltage fault/warnings of all rails"
    print "                  set_margin <rail number> <margin> [%s]" % h_platforms
    print "                             <1 - 15>      l /h /n for mavericks"
    print "                             <1 - 16>      l /h /n for mavericks-p0c"
    print "                             <1 - 12>      l /h /n for newport & montara"
    print "                                           l => low"
    print "                                           h => high"
    print "                                           n => none"
    print " "
    print "                  set_gpio <gpio_number> <l or h> for Newport"
    print "Eg."
    print "btools.py --UCD sh v"
    print "btools.py --UCD set_margin 5 l"
    print " "

    return
#
# Reads voltage faults on all rails
#
def ucd_rail_voltage_fault(platform):

    i = 1

    UCD_I2C_BUS = "2"
    UCD_I2C_ADDR = "0x34"
    UCD_STATUS_VOUT_OP = "0x7A"
    UCD_PAGE_OP = "0x00"
    UCD_NUM_PAGES = "0xD6"

    print " "
    print " RAIL      Voltage Warnings"

    if platform == "newport":
        try:
            get_cmd = "i2cget"
            output = subprocess.check_output([get_cmd, "-f", "-y", UCD_I2C_BUS,
                                     UCD_I2C_ADDR, UCD_NUM_PAGES])
            numpages = int(output, 16)
        except subprocess.CalledProcessError as e:
            print e
            print "Error occured while processing i2cget NUM_PAGES"
            return
    else:
        numpages = 12

    # Parse 1 to N voltage rails
    for i in range(0, numpages):

        try:
            # i2cset -f -y 2 0x34 0x00 i
            set_cmd = "i2cset"
            output = subprocess.check_output([set_cmd, "-f", "-y", UCD_I2C_BUS,
                                     UCD_I2C_ADDR, UCD_PAGE_OP, str(hex(i))])
        except subprocess.CalledProcessError as e:
            print e
            print "Error occured while processing i2cset for rail %.2d " % i
            continue

        try:
            # i2cget -f -y 2 0x34 w
            get_cmd = "i2cget"
            output = subprocess.check_output([get_cmd, "-f", "-y", UCD_I2C_BUS,
                                               UCD_I2C_ADDR, UCD_STATUS_VOUT_OP])

        except subprocess.CalledProcessError as e:
            print e
            print "Error occured while processing i2cget for rail %.2d " % i
            continue


        o = int(output, 16)

        fault_warnings = ""

        if o == 0:
            fault_warnings = "No fault/warning"
        else:
            if o & 0x10:
                fault_warnings = fault_warnings + "Under Voltage Fault,"

            if o & 0x20:
                fault_warnings = fault_warnings + "Under Voltage Warning,"

            if o & 0x40:
                fault_warnings = fault_warnings + "Over Voltage Warning,"

            if o & 0x80:
                fault_warnings = fault_warnings + "Over Voltage Fault,"

        print "  %.2d         %s" % (i + 1, fault_warnings)

    print " "

    return


#
# Displays all rails voltages newport
#
def ucd_rail_voltage_newport():

    i = 1

    UCD_I2C_BUS = "2"
    UCD_I2C_ADDR = "0x34"
    UCD_READ_OP = "0x8b"
    UCD_PAGE_OP = "0x00"
    UCD_VOUT_MODE_OP = "0x20"

    print " "
    print " RAIL                          Voltage(V)"

    string = {1: "01** - VDD12V", 2: "02 - VDD_0_75V", 3: "03 - VDD5V_stby_IR", 4: "04 - VDD5V_stby",
              5: "05* - VDD3_3V", 6: "06 - VDD3_3V_iso", 7: "07 - VDD3_3V_stby", 8: "08- VDD2_5V_stby",
              9: "09- VDD1_8V", 10: "10*- VDDA_1_8V", 11: "11- VDD1_8V_stby", 12: "12- VDD1_5V_stby",
              13: "13*- VDD1_2V", 14: "14- VDD1_2V_stby", 15: "15*- VDD1_0V", 16: "16*- VDD_core"}
    index = 16

# Parse 1 to 16 voltage rails
    for i in range(0, index):

        try:
            set_cmd = "i2cset"
            output = subprocess.check_output([set_cmd, "-f", "-y", UCD_I2C_BUS,
                                     UCD_I2C_ADDR, UCD_PAGE_OP, str(hex(i))])
        except subprocess.CalledProcessError as e:
            print e
            print "Error occured while processing i2cset for rail %.2d " % i
            continue

        try:
            get_cmd = "i2cget"
            mantissa = subprocess.check_output([get_cmd, "-f", "-y", UCD_I2C_BUS,
                                               UCD_I2C_ADDR, UCD_READ_OP, "w"])

        except subprocess.CalledProcessError as e:
            print e
            print "Error occured while processing i2cget for rail %.2d " % i
            continue

        try:
            # i2cget -f -y 2 0x34 0x20
            get_cmd = "i2cget"
            exponent = subprocess.check_output([get_cmd, "-f", "-y", UCD_I2C_BUS,
                                               UCD_I2C_ADDR, UCD_VOUT_MODE_OP])
        except subprocess.CalledProcessError as e:
            print e
            print "Error occured while processing i2cget for rail %.2d " % i
            continue

        # 2 ^ exponent
        # exponent is 5 bit signed value. Thus calculating first exponent.
        # It is based on UCD90120A device spec section 2.2
        exp = int(exponent, 16) | ~0x1f
        exp = ~exp + 1
        div = 1 << exp

        mantissa = int(mantissa, 16)

        print "  %-*s          %.3f" % (20, string.get(i + 1), float(mantissa) / float(div))

    print "  "
    print "* voltages can be margined by IR CLI only "
    print "** voltages cannot be margined "
    print "  "

    return

#
# Displays all rails voltages stinson
#
def ucd_rail_voltage_stinson():

    i = 1

    UCD_I2C_BUS = "2"
    UCD_I2C_ADDR = "0x34"
    UCD_READ_OP = "0x8b"
    UCD_PAGE_OP = "0x00"
    UCD_VOUT_MODE_OP = "0x20"

    print " "
    print " RAIL                          Voltage(V)"

    string = {1: "01** - VDD12V_FUSED_2", 2: "02 - VDD1_1V", 3: "03** - VDD5V_stby_IR", 4: "04 - VDD5V_stby",
              5: "05* - VDD3_3V", 6: "06** - VDD2_5V", 7: "07 - VDD3_3V_stby", 8: "08 - VDD2_5V_stby",
              9: "09 - VDD1_2V_stby", 10: "10 - VDD1_8V", 11: "11 - VDD1_8V_stby", 12: "12 - VDD1_5V_stby",
              13: "13* - VDDAH_1_2V", 14: "14* - VDDL_CORE", 15: "15* - VDDAL_0_75V", 16: "16* - VDD_CORE"}
    index = 16
# Parse 1 to 16 voltage rails
    for i in range(0, index):

        try:
            set_cmd = "i2cset"
            output = subprocess.check_output([set_cmd, "-f", "-y", UCD_I2C_BUS,
                                     UCD_I2C_ADDR, UCD_PAGE_OP, str(hex(i))])
        except subprocess.CalledProcessError as e:
            print e
            print "Error occured while processing i2cset for rail %.2d " % i
            continue

        try:
            get_cmd = "i2cget"
            mantissa = subprocess.check_output([get_cmd, "-f", "-y", UCD_I2C_BUS,
                                               UCD_I2C_ADDR, UCD_READ_OP, "w"])

        except subprocess.CalledProcessError as e:
            print e
            print "Error occured while processing i2cget for rail %.2d " % i
            continue

        try:
            # i2cget -f -y 2 0x34 0x20
            get_cmd = "i2cget"
            exponent = subprocess.check_output([get_cmd, "-f", "-y", UCD_I2C_BUS,
                                               UCD_I2C_ADDR, UCD_VOUT_MODE_OP])
        except subprocess.CalledProcessError as e:
            print e
            print "Error occured while processing i2cget for rail %.2d " % i
            continue

        # 2 ^ exponent
        # exponent is 5 bit signed value. Thus calculating first exponent.
        # It is based on UCD90120A device spec section 2.2
        exp = int(exponent, 16) | ~0x1f
        exp = ~exp + 1
        div = 1 << exp

        mantissa = int(mantissa, 16)

        print "  %-*s          %.3f" % (20, string.get(i + 1), float(mantissa) / float(div))

    print "  "
    print "* voltages can be margined by IR CLI only "
    print "** voltages cannot be margined "
    print "  "

    return
#
# Displays all rails voltages stinson
#
def ucd_rail_voltage_davenport():

    i = 1

    UCD_I2C_BUS = "2"
    UCD_I2C_ADDR = "0x34"
    UCD_READ_OP = "0x8b"
    UCD_PAGE_OP = "0x00"
    UCD_VOUT_MODE_OP = "0x20"

    print " "
    print " RAIL                          Voltage(V)"
    string = {1: "01** - VDD12V_FUSED_2", 2: "02** - VDD1_1V", 3: "03** - VDD5V_stby_IR", 4: "04 - VDD5V_stby",
              5: "05* - VDD3_3V", 6: "06** - VDD2_5V", 7: "07 - VDD3_3V_stby", 8: "08 - VDD2_5V_stby",
              9: "09 - VDD1_2V_stby", 10: "10 - VDD1_8V", 11: "11 - VDD1_8V_stby", 12: "12 - VDD1_5V_stby",
              13: "13* - VDDAH_1_2V", 14: "14* - VDDAL_0_75V", 15: "15* - VDDL_CORE", 16: "16* - VDD_CORE"}
    index = 16

# Parse 1 to 16 voltage rails
    for i in range(0, index):

        try:
            set_cmd = "i2cset"
            output = subprocess.check_output([set_cmd, "-f", "-y", UCD_I2C_BUS,
                                     UCD_I2C_ADDR, UCD_PAGE_OP, str(hex(i))])
        except subprocess.CalledProcessError as e:
            print e
            print "Error occured while processing i2cset for rail %.2d " % i
            continue

        try:
            get_cmd = "i2cget"
            mantissa = subprocess.check_output([get_cmd, "-f", "-y", UCD_I2C_BUS,
                                               UCD_I2C_ADDR, UCD_READ_OP, "w"])

        except subprocess.CalledProcessError as e:
            print e
            print "Error occured while processing i2cget for rail %.2d " % i
            continue

        try:
            # i2cget -f -y 2 0x34 0x20
            get_cmd = "i2cget"
            exponent = subprocess.check_output([get_cmd, "-f", "-y", UCD_I2C_BUS,
                                               UCD_I2C_ADDR, UCD_VOUT_MODE_OP])
        except subprocess.CalledProcessError as e:
            print e
            print "Error occured while processing i2cget for rail %.2d " % i
            continue

        # 2 ^ exponent
        # exponent is 5 bit signed value. Thus calculating first exponent.
        # It is based on UCD90120A device spec section 2.2
        exp = int(exponent, 16) | ~0x1f
        exp = ~exp + 1
        div = 1 << exp

        mantissa = int(mantissa, 16)

        print "  %-*s          %.3f" % (20, string.get(i + 1), float(mantissa) / float(div))

    print "  "
    print "* voltages can be margined by IR CLI only "
    print "** voltages cannot be margined "
    print "  "

    return

#
# Displays all rails voltages mavericks
#
def ucd_rail_voltage_mavericks(poc):

    i = 1
    UCD_I2C_BUS = "2"
    UCD_I2C_ADDR = "0x34"
    UCD_READ_OP = "0x8b"
    UCD_PAGE_OP = "0x00"
    UCD_VOUT_MODE_OP = "0x20"

    print " "
    print " RAIL                          Voltage(V)"

    if (poc == 0):
        string = {1: "01** - VDD12V", 2: "02** - VDD5V_IR", 3: "03 - VDD5V_stby",
                   4: "04 - VDD3_3V_iso", 5: "05 - VDD3_3V_stby", 6: "06*- VDD3_3V_lower",
                   7: "07*- VDD3_3V_upper", 8: "08- VDD2_5V_stby", 9: "09*- VDD2_5V_rptr",
                   10: "10- VDD2_5V_tf", 11: "11- VDD1_8V_stby", 12: "12- VDD1_5V_stby",
                   13: "13- VDD1_2V_stby", 14: "14*- VDD0_9V_anlg", 15: "15*- VDD_core"}
        index = 15
    else:
        string = {1: "01** - VDD12V", 2: "02** - VDD5V_IR", 3: "03 - VDD5V_stby",
                   4: "04 - VDD3_3V_iso", 5: "05 - VDD3_3V_stby", 6: "06*- VDD3_3V_lower",
                   7: "07*- VDD3_3V_upper", 8: "08- VDD2_5V_stby", 9: "09*- VDD1_8V_rt",
                   10: "10- VDD2_5V_tf", 11: "11- VDD1_8V_stby", 12: "12- VDD1_5V_stby",
                   13: "13- VDD1_2V_stby", 14: "14*- VDD0_9V_anlg", 15: "15*- VDD_core",
                   16: "16- VDD1_0V_rt"}
        index = 16

# Parse 1 to 15 voltage rails if mav p0c
    for i in range(0, index):
        retry0 = 0
        err = 1
        while ((err == 1) and (retry0 < 5)):
            try:
                # i2cset -f -y 2 0x34 0x00 i
                set_cmd = "i2cset"
                output = subprocess.check_output([set_cmd, "-f", "-y", UCD_I2C_BUS,
                                     UCD_I2C_ADDR, UCD_PAGE_OP, str(hex(i))])
                err = 0
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing i2cset for rail %.2d " % i
                err = 1
            retry0 = retry0 + 1
        if (err == 1):
            continue

        retry1 = 0
        mantissa = 0
        while (((mantissa == 0) or (mantissa == 65535)) and (retry1 < 5)):
            try:
                # i2cget -f -y 2 0x34 w
                get_cmd = "i2cget"
                mantissa = subprocess.check_output([get_cmd, "-f", "-y", UCD_I2C_BUS,
                                               UCD_I2C_ADDR, UCD_READ_OP, "w"])
                mantissa = int(mantissa, 16)
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing i2cget for rail %.2d " % i
                mantissa = 0
            if retry1:
                sleep(0.010)
                #print "retry1 %d  mantissa %s"  % (retry1, str(mantissa))
            retry1 = retry1 + 1
        if  (mantissa == 0) or (mantissa == 65535):
            continue

        retry2 = 0
        exponent = 0
        while (((exponent == 0) or (exponent == 65535) or (exponent == 255)) and (retry2 < 5)):
            try:
                # i2cget -f -y 2 0x34 0x20
                get_cmd = "i2cget"
                exponent = subprocess.check_output([get_cmd, "-f", "-y", UCD_I2C_BUS,
                                               UCD_I2C_ADDR, UCD_VOUT_MODE_OP])
                exponent = int(exponent, 16)
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing i2cget for rail %.2d " % i
                exponent = 0
            if retry2 :
                sleep(0.010)
                #print "retry2 %d  exponent %s"  % (retry2, str(exponent))
            retry2 = retry2 + 1
        if (exponent == 0) or (exponent == 65535) or (exponent == 255):
            continue

        # 2 ^ exponent
        # exponent is 5 bit signed value. Thus calculating first exponent.
        # It is based on UCD90120A device spec section 2.2
        exp = exponent | ~0x1f
        exp = ~exp + 1
        div = 1 << exp

        print "  %-*s          %.3f" % (20, string.get(i + 1), float(mantissa) / float(div))

    print "  "
    print "* voltages can be margined by IR CLI only "
    print "** voltages cannot be margined "
    print "  "

    return


#
# Displays all rails voltages montara
#
def ucd_rail_voltage_montara():

    i = 1
    UCD_I2C_BUS = "2"
    UCD_I2C_ADDR = "0x34"
    UCD_READ_OP = "0x8b"
    UCD_PAGE_OP = "0x00"
    UCD_VOUT_MODE_OP = "0x20"

    print " "
    print " RAIL                          Voltage(V)"

    string = {1: "01-  VDD12V", 2: "02-  VDD5V_stby", 3: "03-  VDD3_3V_iso",
              4: "04*- VDD3_3V", 5: "05-  VDD3_3V_stby", 6: "06-  VDD2_5V_stby",
              7: "07-  VDD2_5V_tf", 8: "08-  VDD1_8V_stby", 9: "09-  VDD1_5V_stby",
              10: "10-  VDD1_2V_stby", 11: "11*-  VDD0_9V_anlg", 12: "12*-  VDD_core"}
    index = 12

# Parse 1 to 12 voltage rails
    for i in range(0, index):
           
        retry0 = 0
        err = 1
        while ((err == 1) and (retry0 < 5)):
            try:
                # i2cset -f -y 2 0x34 0x00 i
                set_cmd = "i2cset"
                output = subprocess.check_output([set_cmd, "-f", "-y", UCD_I2C_BUS,
                                     UCD_I2C_ADDR, UCD_PAGE_OP, str(hex(i))])
                err = 0
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing i2cset for rail %.2d " % i
                err = 1
            retry0 = retry0 + 1
        if (err == 1):
            continue
            
        retry1 = 0
        mantissa = 0
        while (((mantissa == 0) or (mantissa == 65535)) and (retry1 < 5)):
            try:
                # i2cget -f -y 2 0x34 w
                get_cmd = "i2cget"
                mantissa = subprocess.check_output([get_cmd, "-f", "-y", UCD_I2C_BUS,
                                               UCD_I2C_ADDR, UCD_READ_OP, "w"])
                mantissa = int(mantissa, 16)
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing i2cget for rail %.2d " % i
                mantissa = 0
            if retry1:
                sleep(0.010)
                #print "retry1 %d  mantissa %s"  % (retry1, str(mantissa))
            retry1 = retry1 + 1
        if  (mantissa == 0) or (mantissa == 65535):
            continue

        retry2 = 0
        exponent = 0
        while (((exponent == 0) or (exponent == 65535) or (exponent == 255)) and (retry2 < 5)):
            try:
                # i2cget -f -y 2 0x34 0x20
                get_cmd = "i2cget"
                exponent = subprocess.check_output([get_cmd, "-f", "-y", UCD_I2C_BUS,
                                               UCD_I2C_ADDR, UCD_VOUT_MODE_OP])
                exponent = int(exponent, 16)
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing i2cget for rail %.2d " % i
                exponent = 0
            if retry2 :
                sleep(0.010)
                #print "retry2 %d  exponent %s"  % (retry2, str(exponent))
            retry2 = retry2 + 1
        if (exponent == 0) or (exponent == 65535) or (exponent == 255):
            continue

        # 2 ^ exponent
        # exponent is 5 bit signed value. Thus calculating first exponent.
        # It is based on UCD90120A device spec section 2.2
        exp = exponent | ~0x1f
        exp = ~exp + 1
        div = 1 << exp

        print "  %-*s          %.3f" % (20, string.get(i + 1), float(mantissa) / float(div))

    print "  "
    print "* voltages can be margined by IR CLI only "
    print "  "

    return

#
# Functions set the UCD GPIO
#
def ucd_set_gpio(platform, arg):

    UCD_I2C_BUS = "2"
    UCD_I2C_ADDR = "0x34"
    UCD_GPIO_SEL_OP = "0xFA"
    UCD_GPIO_CONFIG_OP = "0xFB"

    gpio = int(arg[1])
    if arg[2] == "l":
      gpio_val = 0
    else:
      gpio_val = 1

    if platform == "newport":
        if gpio != 3 and gpio != 4 and gpio != 13 and gpio != 14 and gpio != 15:
             error_ucd_usage()
             return
    else:
        error_ucd_usage()
        return
    #convert GPIO to "Pin Id" as understood by UCD PMBUS GPIO_SEL operation
    if gpio == 3:
      gpio_mod = 20
    if gpio == 4:
      gpio_mod = 21
    if gpio == 13:
      gpio_mod = 22
    if gpio == 14:
      gpio_mod = 12
    if gpio == 15:
      gpio_mod = 13

    #set gpio selection
    try:
        set_cmd = "i2cset"
        output = subprocess.check_output([set_cmd, "-f", "-y", UCD_I2C_BUS,
                                         UCD_I2C_ADDR, UCD_GPIO_SEL_OP, str(gpio_mod)])

    except subprocess.CalledProcessError as e:
        print e
        print "Error occured while selecting GPIO %.2d " % (gpio)
        return

    #set gpio value
    try:
        set_cmd = "i2cset"
        if gpio_val == 0:
            val = "3" # en-true, out_en-true, out_val-false status-false
        else:
            val = "7" # en-true, out_en-true, out_val-true status-false

        output = subprocess.check_output([set_cmd, "-f", "-y", UCD_I2C_BUS,
                                         UCD_I2C_ADDR, UCD_GPIO_CONFIG_OP, val])

    except subprocess.CalledProcessError as e:
        print e
        print "Error occured while programming-1 GPIO %.2d" % (gpio)
        return

    #set gpio value but dont write it to flash
    try:
        set_cmd = "i2cset"
        if gpio_val == 0:
            val = "0" # en-false, out_en-false, out_val-false status-false
        else:
            val = "4" # en-false, out_en-false, out_val-true status-false
        output = subprocess.check_output([set_cmd, "-f", "-y", UCD_I2C_BUS,
                                         UCD_I2C_ADDR, UCD_GPIO_CONFIG_OP, val])

    except subprocess.CalledProcessError as e:
        print e
        print "Error occured while setting GPIO %.2d low" % (gpio)
        return

    # read status
    try:
        set_cmd = "i2cget"
        output = subprocess.check_output([set_cmd, "-f", "-y", UCD_I2C_BUS,
                                         UCD_I2C_ADDR, UCD_GPIO_CONFIG_OP])

    except subprocess.CalledProcessError as e:
        print e
        print "Error occured while setting GPIO %.2d low" % (gpio)
        return

    return

#
# Functions set the voltage margins
#
def ucd_voltage_margin(platform, arg):

    UCD_I2C_BUS = "2"
    UCD_I2C_ADDR = "0x34"
    UCD_LOW_MARGIN_OP = "0x18"
    UCD_HIGH_MARGIN_OP = "0x28"
    UCD_NONE_MARGIN_OP = "0x08"
    UCD_PAGE_OP = "0x00"
    UCD_MARGIN_OP = "0x01"

    if platform == "mavericks":
        if not 1 <= int(arg[1]) <= 15:
             error_ucd_usage()
             return
    elif platform == "mavericks-p0c":
        if not 1 <= int(arg[1]) <= 16:
             error_ucd_usage()
             return
    elif platform == "montara" or platform == "newport" or platform == "stinson":
        if not 1 <= int(arg[1]) <= 12:
             error_ucd_usage()
             return
    else:
        error_ucd_usage()
        return
    if arg[2] == "l":
        opcode = str(UCD_LOW_MARGIN_OP)
    elif arg[2] == "h":
        opcode = str(UCD_HIGH_MARGIN_OP)
    elif arg[2] == "n":
        opcode = str(UCD_NONE_MARGIN_OP)
    else:
        error_ucd_usage()
        return


    # Rail number mapping starts from 1
    # But UCD understand from 0. Thus reducing 1
    rail_number = int(arg[1]) - 1

    try:

        set_cmd = "i2cset"
        output = subprocess.check_output([set_cmd, "-f", "-y", UCD_I2C_BUS,
                                         UCD_I2C_ADDR, UCD_PAGE_OP, str(rail_number)])

    except subprocess.CalledProcessError as e:
        print e
        print "Error occured while processing i2cset for rail %.2d " % (rail_number + 1)
        return

    try:

        set_cmd = "i2cset"
        output = subprocess.check_output([set_cmd, "-f", "-y", UCD_I2C_BUS,
                                         UCD_I2C_ADDR, UCD_MARGIN_OP, opcode])
    except subprocess.CalledProcessError as e:
        print e
        print "Error occured while processing i2cset for rail %.2d " % (rail_number + 1)
        return

    try:

        set_cmd = "i2cget"
        output = subprocess.check_output([set_cmd, "-f", "-y", UCD_I2C_BUS,
                                         UCD_I2C_ADDR, UCD_MARGIN_OP])

    except subprocess.CalledProcessError as e:
        print e
        print "Error occured while processing i2cset for rail %.2d " % (rail_number + 1)
        return

    print "Voltage margining done"

    return

#
# Dispatches UCD requests
#
def ucd(argv):

    arg_ucd = argv[2:]

    if arg_ucd[0] == "help" or arg_ucd[0] == "h" or len(arg_ucd) <= 0:
        error_ucd_usage()
        return

    # ./btools.py --UCD sh v [%s]
    if arg_ucd[0] == "sh":
        if len(arg_ucd) == 3:
            platform = arg_ucd[2]
        elif len(arg_ucd) == 2:
            platform = get_project()
        else:
            error_ucd_usage()
            return
    # ./btools.py --UCD fault
    elif arg_ucd[0] == "fault":
        if len(arg_ucd) == 1:
            platform = get_project()
        else:
            error_ucd_usage()
            return
    # ./btools.py --UCD set_margin <rail number> <margin> [%s]
    elif arg_ucd[0] == "set_margin" or arg_ucd[0] == "set_gpio":
        if len(arg_ucd) == 4:
            platform = arg_ucd[3]
        elif len(arg_ucd) == 3:
            platform = get_project()
        else:
            error_ucd_usage()
            return

    if arg_ucd[0] == "sh":
        if platform == "mavericks":
            ucd_rail_voltage_mavericks(0)
        elif platform == "mavericks-p0c":
            ucd_rail_voltage_mavericks(1)
        elif platform == "montara":
            ucd_rail_voltage_montara()
        elif platform == "newport":
            ucd_rail_voltage_newport()
        elif platform == "davenport":
            ucd_rail_voltage_davenport()
        elif platform == "stinson":
            ucd_rail_voltage_stinson()
        else :
            error_ucd_usage()
            return
    elif arg_ucd[0] == "set_margin":
        ucd_voltage_margin(platform, arg_ucd)
        #ucd_ir_voltage_margin(argv)
    elif arg_ucd[0] == "fault":
        ucd_rail_voltage_fault(platform)
    elif arg_ucd[0] == "set_gpio":
        ucd_set_gpio(platform, arg_ucd)
    else:
        error_ucd_usage()
        return

    return

def set_ir_page(i2c_bus, i2c_addr, page):
    IR_PAGE_ADDR = "0x00"
    set_cmd = "i2cset"
    try:
        exponent = subprocess.check_output([set_cmd, "-f", "-y", i2c_bus,
                                           i2c_addr, IR_PAGE_ADDR, page])
    except subprocess.CalledProcessError as e:
      print e

    return

def ir_voltage_show_montara():

    IR_I2C_BUS = "0x1"
    IR_PMBUS_ADDR = {1: "0x70", 2: "0x72", 3: "0x75"}
    IR_VOUT_MODE_OP = "0x20"
    IR_READ_VOUT_OP = "0x8b"
    IR_READ_IOUT_OP = "0x8c"
    string ={1: "VDD_CORE", 2: "AVDD", 3: "QSFP"}

    for i in range(1, 4):
        retry = 0
        err = 1
        while((err == 1) and (retry < 5)):
            try:
                # i2cget -f -y 1 0x70 0x8b w
                get_cmd = "i2cget"
                exponent = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                         IR_PMBUS_ADDR.get(i), IR_VOUT_MODE_OP, "w"])
                err = 0
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing VOUT_MODE for IR "
                err = 1
            retry = retry + 1
        if (err == 1):
            continue

        retry = 0
        err = 1
        while((err == 1) and (retry < 5)):
            try:
                # i2cget -f -y 1 0x70 0x8b w
                get_cmd = "i2cget"
                mantissa = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                             IR_PMBUS_ADDR.get(i), IR_READ_VOUT_OP, "w"])
                err = 0
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing i2cget for IR "
                err = 1
            retry = retry + 1
        if (err == 1):
            continue

        # 2 ^ exponent
        # exponent is 5 bit signed value. Thus calculating first exponent.
        exp = int(exponent, 16) | ~0x1f
        exp = ~exp + 1
        div = 1 << exp

        mantissa = int(mantissa, 16)

        v = (float(mantissa)/float(div))

        # As referred by hardware spec QSFP voltage need to be * 2
        if i == 3:
            v = v * 2

        # find current
        retry = 0
        err = 1
        while((err == 1) and (retry < 5)):
            try:
                # i2cget -f -y 1 0x70 0x8c w
                get_cmd = "i2cget"
                mantissa = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                             IR_PMBUS_ADDR.get(i), IR_READ_IOUT_OP, "w"])
                err = 0
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing i2cget for IR "
                err = 1
            retry = retry + 1
        if (err == 1):
            continue

        m = int(mantissa, 16) & 0x07ff

        # 2 ^ exponent
        # exponent is 5 bit signed value. Thus calculating first exponent.
        exp = int(mantissa, 16) & 0xf800
        exp = exp >> 11
        exp = ~exp + 1
        exp = exp & 0x1f
        div = 1 << exp

        amp = (float(m)/float(div))

        print "IR %-*s       %.3f V    %.3f A      %.3f W" % (15, string.get(i), v, amp, (v * amp))

    return

def open_upper_PCA9548_lock():
    lock_file = "/tmp/mav_9548_10_lock"
    timeout_counter = 0

    while os.path.isfile(lock_file):
        timeout_counter = timeout_counter + 1
        if timeout_counter >= 20:
            # It's possible that the other process using the lock might have
            # malfunctioned. Hence explicitly delete the file and proceed
            print "Some process didn't clean up the lock file. Hence explicitly cleaning it up and proceeding"
            os.remove(lock_file)
            break
        sleep(0.5)

    open(lock_file, "w+")
    return

def close_upper_PCA9548_lock():
    lock_file = "/tmp/mav_9548_10_lock"
    try:
         os.remove(lock_file)
    except OSError:
         pass
    return

#
# Mavericks need i2c switch to be opened for reading IR
#
def ir_open_i2c_switch():

    open_upper_PCA9548_lock()

    IR_I2C_SW_BUS = "9"
    IR_I2C_SW_ADDR = "0x70"

    try:
        get_cmd = "i2cget"
        output = subprocess.check_output([get_cmd, "-f", "-y",
                                          IR_I2C_SW_BUS,
                                          IR_I2C_SW_ADDR])

        output = int(output, 16)

        # opening i2c switch for 0x08, 0x09, 0x0c
        res = output | 0x7
        set_cmd = "i2cset"
        o = subprocess.check_output([set_cmd, "-f", "-y",
                                    IR_I2C_SW_BUS,
                                    IR_I2C_SW_ADDR, str(res)])
        #close_upper_PCA9548_lock()

    except subprocess.CalledProcessError as e:
        print e
        print "Error occured while processing opening i2c switch" \
              " on mavericks upper board"
        #close_upper_PCA9548_lock()
        output = -1
        
    return output

#
# Restoring the i2c switch state
#
def ir_restore_i2c_switch(res):
    output = 0
    #open_upper_PCA9548_lock()

    IR_I2C_SW_BUS = "9"
    IR_I2C_SW_ADDR = "0x70"

    try:
        set_cmd = "i2cset"
        o = subprocess.check_output([set_cmd, "-f", "-y",
                                    IR_I2C_SW_BUS,
                                    IR_I2C_SW_ADDR, str(res)])
        close_upper_PCA9548_lock()

    except subprocess.CalledProcessError as e:
        print e
        print "Error occured while processing restoring i2c switch" \
              " on mavericks upper board"
        close_upper_PCA9548_lock()
        output = -1

    return output

def ir_voltage_show_newport(arg_ir):

    IR_I2C_BUS = "0x1"
    IR_PMBUS_ADDR = {1: "0x40", 2: "0x42", 3: "0x44", 4:"0x44", 5:"0x46"}
    IR_VOUT_MODE_OP = "0x20"
    IR_READ_VOUT_OP = "0x8b"
    IR_READ_IOUT_OP = "0x8c"
    IR_READ_POUT_OP = "0x96"
    IR_READ_TEMP1_OP = "0x8d"
    PAGE_ADDR = "0"
    string = {1: "VDD_CORE_0.75V", 2: "VDDT_0.9V", 3: "VDDA_1.7V", 4:"VDDA_AGC_1.8V", 5:"VDD_QSFP_3.3V"}
    for i in range(1, 6):
        retry = 0
        err = 1
        while((err == 1) and (retry < 5)):
            try:
                get_cmd = "i2cget"
                if i == 4:
                    set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(i), "1")
                else:
                    set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(i), "0")

                exponent = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                         IR_PMBUS_ADDR.get(i), IR_VOUT_MODE_OP, "w"])
                err = 0
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing VOUT_MODE for IR "
                err = 1
            retry = retry + 1
        if (err == 1):
            continue

        # 2 ^ exponent
        # exponent is 5 bit signed value. Thus calculating first exponent.
        exp = int(exponent, 16) | ~0x1f
        exp = ~exp + 1
        div = 1 << exp

        if arg_ir[1] == "pout":
            retry = 0
            err = 1
            while((err == 1) and (retry < 5)):
                try:
                    get_cmd = "i2cget"
                    mantissa = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                                 IR_PMBUS_ADDR.get(i), IR_READ_POUT_OP, "w"])
                    err = 0
                except subprocess.CalledProcessError as e:
                    print e
                    print "Error occured while processing i2cget for IR pout"
                    err = 1
                retry = retry + 1
            if (err == 1):
                continue

            m = int(mantissa, 16) & 0x07ff

            # 2 ^ exponent
            # exponent is 5 bit signed value. Thus calculating first exponent.
            exp = int(mantissa, 16) & 0xf800
            exp = exp >> 11
            exp = ~exp + 1
            exp = exp & 0x1f
            div = 1 << exp

            pout = (float(m)/float(div))

            print "IR %-*s       %.3f W" % (15, string.get(i), pout)
        elif arg_ir[1] == "temp1":
            retry = 0
            err = 1
            while((err == 1) and (retry < 5)):
                try:
                    get_cmd = "i2cget"
                    mantissa = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                                 IR_PMBUS_ADDR.get(i), IR_READ_TEMP1_OP, "w"])
                    err = 0 
                except subprocess.CalledProcessError as e:
                    print e
                    print "Error occured while processing i2cget for IR temp1"
                    err = 1
                retry = retry + 1
            if (err == 1):
                continue

            m = int(mantissa, 16) & 0x07ff

            # 2 ^ exponent
            # exponent is 5 bit signed value. Thus calculating first exponent.
            exp = int(mantissa, 16) & 0xf800
            exp = exp >> 11
            exp = ~exp + 1
            exp = exp & 0x1f
            div = 1 << exp

            temp1 = (float(m)/float(div))

            print "IR %-*s       %.3f C" % (15, string.get(i), temp1)
        else:
            retry = 0
            err = 1
            while((err == 1) and (retry < 5)):
                try:
                    get_cmd = "i2cget"
                    mantissa = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                                 IR_PMBUS_ADDR.get(i), IR_READ_VOUT_OP, "w"])
                    err = 0
                except subprocess.CalledProcessError as e:
                    print e
                    print "Error occured while processing i2cget for IR "
                    err = 1
                retry = retry + 1
            if (err == 1):
                continue

            mantissa = int(mantissa, 16)

            v = (float(mantissa)/float(div))

            # As referred by hardware spec QSFP voltage need to be * 2
            if i == 5:
                v = v * 2

            # find current
            retry = 0
            err = 1
            while((err == 1) and (retry < 5)):
                try:
                    # i2cget -f -y 1 0x70 0x8c w
                    get_cmd = "i2cget"
                    mantissa = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                                IR_PMBUS_ADDR.get(i), IR_READ_IOUT_OP, "w"])
                    err = 0
                except subprocess.CalledProcessError as e:
                    print e
                    print "Error occured while processing i2cget for IR "
                    err = 1
                retry = retry + 1
            if (err == 1):
                continue

            m = int(mantissa, 16) & 0x07ff

            # 2 ^ exponent
            # exponent is 5 bit signed value. Thus calculating first exponent.
            exp = int(mantissa, 16) & 0xf800
            exp = exp >> 11
            exp = ~exp + 1
            exp = exp & 0x1f
            div = 1 << exp

            amp = (float(m)/float(div))

            print "IR %-*s       %.3f V    %.3f A      %.3f W" % (15, string.get(i), v, amp, (v * amp))
    return

def ir_voltage_show_davenport(arg_ir):

    IR_I2C_BUS = "0x1"
    IR_PMBUS_ADDR = {1: "0x40", 2: "0x42", 3: "0x40", 4:"0x44", 5:"0x46"}
#    IR_PMBUS_ADDR = {1: "0x40", 2: "0x42", 3: "0x44", 4:"0x44", 5:"0x46"}
    IR_VOUT_MODE_OP = "0x20"
    IR_READ_VOUT_OP = "0x8b"
    IR_READ_IOUT_OP = "0x8c"
    IR_READ_POUT_OP = "0x96"
    IR_READ_TEMP1_OP = "0x8d"
    PAGE_ADDR = "0"
    string = {1: "VDD_CORE", 2: "VDDAH", 3: "VDDL_CORE", 4:"VDDAL", 5:"VDD_QSFP_3.3V"}
#    string = {1: "VDD_CORE_0.75V", 2: "VDDT_0.9V", 3: "VDDA_1.7V", 4:"VDDA_AGC_1.8V", 5:"VDD_QSFP_3.3V"}
    for i in range(1, 6):
        retry = 0
        err = 1
        while((err == 1) and (retry < 5)):
            try:
                get_cmd = "i2cget"
                if i == 3:
                    set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(i), "1")
                else:
                    set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(i), "0")

                exponent = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                         IR_PMBUS_ADDR.get(i), IR_VOUT_MODE_OP, "w"])
                err = 0
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing VOUT_MODE for IR "
                err = 1
            retry = retry + 1
        if (err == 1):
            continue

        # 2 ^ exponent
        # exponent is 5 bit signed value. Thus calculating first exponent.
        exp = int(exponent, 16) | ~0x1f
        exp = ~exp + 1
        div = 1 << exp

        if arg_ir[1] == "pout":
            retry = 0
            err = 1
            while((err == 1) and (retry < 5)):
                try:
                    get_cmd = "i2cget"
                    mantissa = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                                 IR_PMBUS_ADDR.get(i), IR_READ_POUT_OP, "w"])
                    err = 0
                except subprocess.CalledProcessError as e:
                    print e
                    print "Error occured while processing i2cget for IR pout"
                    err = 1
                retry = retry + 1
            if (err == 1):
                continue

            m = int(mantissa, 16) & 0x07ff

            # 2 ^ exponent
            # exponent is 5 bit signed value. Thus calculating first exponent.
            exp = int(mantissa, 16) & 0xf800
            exp = exp >> 11
            exp = ~exp + 1
            exp = exp & 0x1f
            div = 1 << exp

            pout = (float(m)/float(div))

            print "IR %-*s       %.3f W" % (15, string.get(i), pout)
        elif arg_ir[1] == "temp1":
            retry = 0
            err = 1
            while((err == 1) and (retry < 5)):
                try:
                    get_cmd = "i2cget"
                    mantissa = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                                 IR_PMBUS_ADDR.get(i), IR_READ_TEMP1_OP, "w"])
                    err = 0
                except subprocess.CalledProcessError as e:
                    print e
                    print "Error occured while processing i2cget for IR temp1"
                    err = 1
                retry = retry + 1
            if (err == 1):
                continue

            m = int(mantissa, 16) & 0x07ff

            # 2 ^ exponent
            # exponent is 5 bit signed value. Thus calculating first exponent.
            exp = int(mantissa, 16) & 0xf800
            exp = exp >> 11
            exp = ~exp + 1
            exp = exp & 0x1f
            div = 1 << exp

            temp1 = (float(m)/float(div))

            print "IR %-*s       %.3f C" % (15, string.get(i), temp1)
        else:
            retry = 0
            err = 1
            while((err == 1) and (retry < 5)):
                try:
                    get_cmd = "i2cget"
                    mantissa = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                                 IR_PMBUS_ADDR.get(i), IR_READ_VOUT_OP, "w"])
                    err = 0
                except subprocess.CalledProcessError as e:
                    print e
                    print "Error occured while processing i2cget for IR "
                    err = 1
                retry = retry + 1
            if (err == 1):
                continue

            mantissa = int(mantissa, 16)

            v = (float(mantissa)/float(div))

            # As referred by hardware spec QSFP voltage need to be * 2
            if i == 5:
                v = v * 2

            # find current
            retry = 0
            err = 1
            while((err == 1) and (retry < 5)):
                try:
                    # i2cget -f -y 1 0x70 0x8c w
                    get_cmd = "i2cget"
                    mantissa = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                                IR_PMBUS_ADDR.get(i), IR_READ_IOUT_OP, "w"])
                    err = 0
                except subprocess.CalledProcessError as e:
                    print e
                    print "Error occured while processing i2cget for IR "
                    err = 1
                retry = retry + 1
            if (err == 1):
                continue

            m = int(mantissa, 16) & 0x07ff

            # 2 ^ exponent
            # exponent is 5 bit signed value. Thus calculating first exponent.
            exp = int(mantissa, 16) & 0xf800
            exp = exp >> 11
            exp = ~exp + 1
            exp = exp & 0x1f
            div = 1 << exp

            amp = (float(m)/float(div))

            print "IR %-*s       %.3f V    %.3f A      %.3f W" % (15, string.get(i), v, amp, (v * amp))
    return
def ir_voltage_show_stinson(arg_ir):

    IR_I2C_BUS = "0x1"
    IR_PMBUS_ADDR = {1: "0x40", 2: "0x40", 3: "0x47", 4:"0x47", 5:"0x58"}
    IR_VOUT_MODE_OP = "0x20"
    IR_READ_VOUT_OP = "0x8b"
    IR_READ_IOUT_OP = "0x8c"
    IR_READ_POUT_OP = "0x96"
    IR_READ_TEMP1_OP = "0x8d"
    PAGE_ADDR = "0"
    string = {1: "VDD_CORE", 2: "VDDAH", 3: "VDDL_CORE", 4:"VDDAL", 5:"VDD_QSFP_3.3V"}
    for i in range(1, 6):
        retry = 0
        err = 1
        while((err == 1) and (retry < 5)):
            try:
                get_cmd = "i2cget"
                if i == 4 or i == 2:
                    set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(i), "1")
                else:
                    set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(i), "0")

                exponent = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                         IR_PMBUS_ADDR.get(i), IR_VOUT_MODE_OP, "w"])
                err = 0
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing VOUT_MODE for IR "
                err = 1
            retry = retry + 1
        if (err == 1):
            continue

        # 2 ^ exponent
        # exponent is 5 bit signed value. Thus calculating first exponent.

        exp = int(exponent, 16) | ~0x1f
        exp = ~exp + 1
        div = 1 << exp

        if arg_ir[1] == "pout":
            retry = 0
            err = 1
            while((err == 1) and (retry < 5)):
                try:
                    get_cmd = "i2cget"
                    mantissa = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                                 IR_PMBUS_ADDR.get(i), IR_READ_POUT_OP, "w"])
                    err = 0
                except subprocess.CalledProcessError as e:
                    print e
                    print "Error occured while processing i2cget for IR pout"
                    err = 1
                retry = retry + 1
            if (err == 1):
                continue

            m = int(mantissa, 16) & 0x07ff

            # 2 ^ exponent
            # exponent is 5 bit signed value. Thus calculating first exponent.
            exp = int(mantissa, 16) & 0xf800
            exp = exp >> 11
            exp = ~exp + 1
            exp = exp & 0x1f
            div = 1 << exp

            pout = (float(m)/float(div))

            print "IR %-*s       %.3f W" % (15, string.get(i), pout)
        elif arg_ir[1] == "temp1":
            retry = 0
            err = 1
            while((err == 1) and (retry < 5)):
                try:
                    get_cmd = "i2cget"
                    mantissa = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                                 IR_PMBUS_ADDR.get(i), IR_READ_TEMP1_OP, "w"])
                    err = 0
                except subprocess.CalledProcessError as e:
                    print e
                    print "Error occured while processing i2cget for IR temp1"
                    err = 1
                retry = retry + 1
            if (err == 1):
                continue

            m = int(mantissa, 16) & 0x07ff

            # 2 ^ exponent
            # exponent is 5 bit signed value. Thus calculating first exponent.
            exp = int(mantissa, 16) & 0xf800
            exp = exp >> 11
            exp = ~exp + 1
            exp = exp & 0x1f
            div = 1 << exp

            temp1 = (float(m)/float(div))

            print "IR %-*s       %.3f C" % (15, string.get(i), temp1)
        else:
            retry = 0
            err = 1
            while((err == 1) and (retry < 5)):
                try:
                    get_cmd = "i2cget"
                    mantissa = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                                 IR_PMBUS_ADDR.get(i), IR_READ_VOUT_OP, "w"])
                    err = 0
                except subprocess.CalledProcessError as e:
                    print e
                    print "Error occured while processing i2cget for IR "
                    err = 1
                retry = retry + 1
            if (err == 1):
                continue

            mantissa = int(mantissa, 16)

            v = (float(mantissa)/float(div))

            # As referred by hardware spec QSFP voltage need to be * 2
            if i == 5:
                # QSFP rail IR device XDPE12284 has vid=3 (not linear conversion)
                v = float(mantissa)
                if v >= 1:
                  v = ((v - 1) * 10) + 200
                else:
                  v = 0
                v = (v * 2) / 1000

            # find current
            retry = 0
            err = 1
            while((err == 1) and (retry < 5)):
                try:
                    # i2cget -f -y 1 0x70 0x8c w
                    get_cmd = "i2cget"
                    mantissa = subprocess.check_output([get_cmd, "-f", "-y", IR_I2C_BUS,
                                                IR_PMBUS_ADDR.get(i), IR_READ_IOUT_OP, "w"])
                    err = 0
                except subprocess.CalledProcessError as e:
                    print e
                    print "Error occured while processing i2cget for IR "
                    err = 1
                retry = retry + 1
            if (err == 1):
                continue

            m = int(mantissa, 16) & 0x07ff

            # 2 ^ exponent
            # exponent is 5 bit signed value. Thus calculating first exponent.
            exp = int(mantissa, 16) & 0xf800
            exp = exp >> 11
            exp = ~exp + 1
            exp = exp & 0x1f
            div = 1 << exp

            amp = (float(m)/float(div))

            print "IR %-*s       %.3f V    %.3f A      %.3f W" % (15, string.get(i), v, amp, (v * amp))
    return

def ir_voltage_show_mavericks(poc):

    #a = ir_open_i2c_switch()
    UPPER_IR_I2C_BUS = "0x9"
    UPPER_IR_PMBUS_ADDR = {1: "0x40", 2: "0x74", 3: "0x71"}
    IR_VOUT_MODE_OP = "0x20"
    IR_READ_VOUT_OP = "0x8b"
    IR_READ_IOUT_OP = "0x8c"
    string = {1: "VDD_CORE", 2: "AVDD", 3: "QSFP_UPPER"}

    for i in range(1, 4):
        a = ir_open_i2c_switch()
        while (a < 0):
          time.sleep(0.010) # 10ms
          a = ir_open_i2c_switch()
        retry = 0
        err = 1
        while((err == 1) and (retry < 5)):
            try:
                # i2cget -f -y 1 0x70 0x20 w
                get_cmd = "i2cget"
                exponent = subprocess.check_output([get_cmd, "-f", "-y", UPPER_IR_I2C_BUS,
                                         UPPER_IR_PMBUS_ADDR.get(i), IR_VOUT_MODE_OP, "w"])
                err = 0
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing VOUT_MODE for UPPER IR "
                ir_restore_i2c_switch(a)
                err = 1
            retry = retry + 1
        if (err == 1):
            continue
            
        retry = 0
        err = 1
        while((err == 1) and (retry < 5)):
            try:
                # i2cget -f -y 1 0x70 0x8b w
                get_cmd = "i2cget"
                mantissa = subprocess.check_output([get_cmd, "-f", "-y", UPPER_IR_I2C_BUS,
                                             UPPER_IR_PMBUS_ADDR.get(i), IR_READ_VOUT_OP, "w"])
                err = 0
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing i2cget for UPPER IR "
                ir_restore_i2c_switch(a)
                err = 1
            retry = retry + 1
        if (err == 1):
            continue

        # 2 ^ exponent
        # exponent is 5 bit signed value. Thus calculating first exponent.
        exp = int(exponent, 16) | ~0x1f
        exp = ~exp + 1
        div = 1 << exp

        mantissa = int(mantissa, 16)

        v = (float(mantissa)/float(div))

        # As referred by hardware spec QSFP voltage need to be * 2
        if i == 3:
            v = v * 2

        # find current
        retry = 0
        err = 1
        while((err == 1) and (retry < 5)):
            try:
                # i2cget -f -y 1 0x70 0x8c w
                get_cmd = "i2cget"
                mantissa = subprocess.check_output([get_cmd, "-f", "-y", UPPER_IR_I2C_BUS,
                                            UPPER_IR_PMBUS_ADDR.get(i), IR_READ_IOUT_OP, "w"])
                ir_restore_i2c_switch(a)
                err = 0
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing i2cget for IR "
                ir_restore_i2c_switch(a) 
                err = 1
            retry = retry + 1
        if (err == 1):
            continue

        m = int(mantissa, 16) & 0x07ff

        # 2 ^ exponent
        # exponent is 5 bit signed value. Thus calculating first exponent.
        exp = int(mantissa, 16) & 0xf800
        exp = exp >> 11
        exp = ~exp + 1
        exp = exp & 0x1f
        div = 1 << exp

        amp = (float(m)/float(div))

        print "IR %-*s       %.3f V    %.3f A      %.3f W" % (15, string.get(i), v, amp, (v * amp))

    #ir_restore_i2c_switch(a)

    LOWER_IR_I2C_BUS = "0x1"
#for Mavericks-P0C
    if (poc == 1):
      LOWER_IR_PMBUS_ADDR = {1: "0x71", 2: "0x72", 3: "0x70"}
      lower_string = {1: "QSFP_LOWER", 2: "RETIMER_VDDA", 3:"RETIMER_VDD"}
      range_p0c = 4;
#for MAvericks-P0A/P0B
    else:
      LOWER_IR_PMBUS_ADDR = {1: "0x71", 2: "0x72"}
      lower_string = {1: "QSFP_LOWER", 2: "REPEATER"}
      range_p0c = 3;

    for i in range(1, range_p0c):
        retry = 0
        err = 1
        while((err == 1) and (retry < 5)):
            try:
                # i2cget -f -y 1 0x70 0x8b w
                get_cmd = "i2cget"
                exponent = subprocess.check_output([get_cmd, "-f", "-y", LOWER_IR_I2C_BUS,
                                         LOWER_IR_PMBUS_ADDR.get(i), IR_VOUT_MODE_OP, "w"])
                err = 0
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing VOUT_MODE for LOWER IR "
                err = 1
            retry = retry + 1
        if (err == 1):
            continue

        retry = 0
        err = 1
        while((err == 1) and (retry < 5)):
            try:
                # i2cget -f -y 1 0x70 0x8b w
                get_cmd = "i2cget"
                mantissa = subprocess.check_output([get_cmd, "-f", "-y", LOWER_IR_I2C_BUS,
                                             LOWER_IR_PMBUS_ADDR.get(i), IR_READ_VOUT_OP, "w"])
                err = 0
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing i2cget for LOWER IR "
                err = 1
            retry = retry + 1
        if (err == 1):
            continue

        # 2 ^ exponent
        # exponent is 5 bit signed value. Thus calculating first exponent.
        exp = int(exponent, 16) | ~0x1f
        exp = ~exp + 1
        div = 1 << exp

        mantissa = int(mantissa, 16)
        if (poc == 1):
          v = (float(mantissa)/float(div))
          if (i == 1):
            v = v * 2
        else:
          v = (float(mantissa)/float(div)) * 2

        # find current
        retry = 0
        err = 1
        while((err == 1) and (retry < 5)):
            try:
                # i2cget -f -y 1 0x70 0x8c w
                get_cmd = "i2cget"
                mantissa = subprocess.check_output([get_cmd, "-f", "-y", LOWER_IR_I2C_BUS,
                                            LOWER_IR_PMBUS_ADDR.get(i), IR_READ_IOUT_OP, "w"])
                err = 0
            except subprocess.CalledProcessError as e:
                print e
                print "Error occured while processing i2cget for LOWER IR "
                err = 1
            retry = retry + 1
        if (err == 1):
            continue

        m = int(mantissa, 16) & 0x07ff

        # 2 ^ exponent
        # exponent is 5 bit signed value. Thus calculating first exponent.
        exp = int(mantissa, 16) & 0xf800
        exp = exp >> 11
        exp = ~exp + 1
        exp = exp & 0x1f
        div = 1 << exp

        amp = (float(m)/float(div))

        print "IR %-*s       %.3f V    %.3f A      %.3f W" % (15, lower_string.get(i), v, amp, (v * amp))


    return

# IR utility usage
def error_ir_usage():

    print ""
    print "Usage:"
    print "./btools.py --IR sh v [%s]                         => Show IR voltages " % h_platforms_with_p0c
    print "./btools.py --IR set [%s] <margin> <voltage rail>  => Set IR voltages margin" % h_platforms_with_p0c
    print "                     <margin>          <voltage rail>"
    print "                     l = low margin    AVDD          (mon/mav/mav p0c only)"
    print "                     h = high margin   VDD_CORE"
    print "                     n = normal        QSFP          (mon/new only)"
    print "                                       QSFP_UPPER    (mav/mav p0c only)"
    print "                                       QSFP_LOWER    (mav/mav p0c only)"
    print "                                       RETIMER_VDD   (mav p0c only)"
    print "                                       RETIMER_VDDA  (mav p0c only)"
    print "                                       REPEATER      (mav only)"
    print "                                       VDDA_1.7V     (new only)"
    print "                                       VDDT_0.9V     (new only)"
    print "                                       VDDA_AGC_1.8V (new only)"
    print "                                       VDDAH         (TF3 only)"
    print "                                       VDDAL         (TF3 only)"
    print "                                       VDDL_CORE     (TF3 only)"
    print ""
    # Commenting this part as nobody other than Barefoot Hardware team should touch this functionality
    print "./btools.py --IR set_vdd_core [%s] <voltage>  => Set IR VDD_CORE voltages " % h_platforms_with_p0c
    print "                              <voltage> must be in range of .65-.95V else discarded"
    print "./btools.py --IR set_vddl_core [%s] <voltage> => Set IR VDDL_CORE voltages " % h_platforms_with_p0c
    print "                              <voltage> must be in range of .65-.95V and on TF3 else discarded"
    print "Eg."
    print "btools.py --IR sh v"
    print "btools.py --IR set_vdd_core .80"
    print ""

    return

def read_vout(rail, I2C_BUS, I2C_ADDR):

    IR_VOUT_MODE_OP = "0x20"
    IR_READ_VOUT_OP = "0x8b"

    try:
      # i2cget -f -y 1 0x70 0x20 w
      get_cmd = "i2cget"
      exponent = subprocess.check_output([get_cmd, "-f", "-y", I2C_BUS,
                                     I2C_ADDR, IR_VOUT_MODE_OP, "w"])
    except subprocess.CalledProcessError as e:
      print e
      print "Error occured while processing VOUT_MODE "

    try:
      # i2cget -f -y 1 0x70 0x8b w
      get_cmd = "i2cget"
      mantissa = subprocess.check_output([get_cmd, "-f", "-y", I2C_BUS,
                                         I2C_ADDR, IR_READ_VOUT_OP, "w"])
    except subprocess.CalledProcessError as e:
      print e
      print "Error occured while processing i2cget "

    # 2 ^ exponent
    # exponent is 5 bit signed value. Thus calculating first exponent.
    exp = int(exponent, 16) | ~0x1f
    exp = ~exp + 1
    div = 1 << exp

    mantissa = int(mantissa, 16)

    v = (float(mantissa)/float(div))

    if (rail == "QSFP" or rail == "QSFP_UPPER" or rail == "QSFP_LOWER" or
            rail == "REPEATER"):
        v = v * 2

    print ("IR %s       %.3f V" % (rail, v))

    return

def set_ir_voltage(mod, i2c_bus, i2c_addr, margin_cmd, margin_apply, voltage):

  IR_OPERATION = "0x1"
  if i2c_bus == "0x9":
    a = ir_open_i2c_switch()
    while (a < 0):
      time.sleep(0.010) # 10ms
      a = ir_open_i2c_switch()

  try:
    # set voltage margin value in register
    set_cmd = "i2cset"
    o = subprocess.check_output([set_cmd, "-f", "-y",
                                i2c_bus, i2c_addr,
                                margin_cmd, voltage, 'w'])

    # execute operation 0x1 with voltage margin AOF
    set_cmd = "i2cset"
    o = subprocess.check_output([set_cmd, "-f", "-y",
                                i2c_bus, i2c_addr,
                                IR_OPERATION, margin_apply])

  except subprocess.CalledProcessError as e:
    print e
    print "Error occured while setting %s voltage" % mod

  read_vout(mod, i2c_bus, i2c_addr)

  if i2c_bus == "0x9":
    a = ir_restore_i2c_switch(a)

  return


def fix_montara_vdd_core_ir_pmbus():

  try:
    # Fix VDD CORE to pmbus
    set_cmd = "i2cset"
    o = subprocess.check_output([set_cmd, "-f", "-y",
                                "1", "0x8", "0x2B", "0x80"])

  except subprocess.CalledProcessError as e:
    print e
    print "Error occured while shifting baxter/IR to PMBUS"

  return

def fix_montara_avdd_ir_pmbus():

  try:
    # Fix AVDD to pmbus
    set_cmd = "i2cset"
    o = subprocess.check_output([set_cmd, "-f", "-y",
                                "1", "0xA", "0x2B", "0x80"])

  except subprocess.CalledProcessError as e:
    print e
    print "Error occured while shifting baxter/IR to PMBUS"

  return

def ir_voltage_set_montara(arg_ir):

    IR_I2C_BUS = "0x1"
    IR_PMBUS_ADDR = {1: "0x70", 2: "0x72", 3: "0x75"}
    string_upper = {1: "VDD_CORE", 2: "AVDD", 3: "QSFP"}

    IR_MARGIN_LOW_AOF_OP = "0x98"
    IR_MARGIN_HIGH_AOF_OP = "0xA8"
    IR_MARGIN_OFF = "0x80"
    IR_OPERATION = "0x1"

    IR_VOUT_MARGIN_HIGH = "0x25"
    IR_VOUT_MARGIN_LOW = "0x26"
    IR_VOUT_CMD = "0x21"

    if arg_ir[2] == "AVDD":

      # keep this command for few boards
      #fix_montara_avdd_ir_pmbus()
      # voltage +3% -3%
      VOLT_MARGIN_HIGH = "0x1DB"
      VOLT_MARGIN_LOW = "0x1BF"
      VOLT_NORMAL = "0x1CE"
      i2c_addr = IR_PMBUS_ADDR.get(2)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    elif arg_ir[2] == "VDD_CORE":

      # keep this command for few boards
      #fix_montara_vdd_core_ir_pmbus()

      # voltage +2% -2%
      VOLT_MARGIN_HIGH = "0x1B6"
      VOLT_MARGIN_LOW = "0x1A5"
      VOLT_NORMAL = "0x1AE"
      i2c_addr = IR_PMBUS_ADDR.get(1)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    elif arg_ir[2] == "QSFP":

      VOLT_MARGIN_HIGH = "0x361"
      VOLT_MARGIN_LOW = "0x323"
      VOLT_NORMAL =  "0x34D"
      i2c_addr = IR_PMBUS_ADDR.get(3)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    else:
        error_ir_usage()

    return

# Only available for Part SKEW Need by hardware
def ir_set_vdd_core_dynamic_range_montara(arg_ir):

    VDD_CORE_IR_I2C_BUS = "0x1"
    VDD_CORE_IR_PMBUS_ADDR = "0x70"
    IR_MARGIN_OFF = "0x80"
    IR_VOUT_CMD = "0x21"

    try:
        v = float(arg_ir[1])
    except ValueError:
        error_ir_usage()
        return

    if v < 0.65 or v > 0.95:
        print "Voltage value not in range .65 - .95"
        return
    voltage_scale = {0: "0x14D", 10: "0x152", 20: "0x157", 30: "0x15C", 40: "0x161", 50: "0x166", 60: "0x16c",
                    70: "0x171",  80: "0x176",  85: "0x178",  90: "0x17B", 100: "0x180", 105: "0x182", 110: "0x185", 
                   120: "0x18A", 125: "0x18D", 130: "0x18F", 135: "0x191", 140: "0x194", 150: "0x19A", 155: "0x19C",
                   160: "0x19F", 170: "0x1A4", 175: "0x1A6", 180: "0x1A9", 185: "0x1AB", 190: "0x1AE", 200: "0x1B3",
                   205: "0x1B5", 210: "0x1B8", 220: "0x1BD", 225: "0x1C0", 230: "0x1C3", 235: "0x1C5", 240: "0x1C8",
                   250: "0x1CD", 255: "0x1CF", 260: "0x1D2", 270: "0x1D7", 280: "0x1DC", 290: "0x1E1", 300: "0x1E6"}

    # Convert to mv with -9 exponent
    i = (v * 1000) % 650
    voltage = voltage_scale.get(i)

    if voltage == None:
        error_ir_usage()
        return

    margin_cmd = IR_VOUT_CMD
    margin_apply = IR_MARGIN_OFF
    set_ir_voltage("VDD_CORE", VDD_CORE_IR_I2C_BUS, VDD_CORE_IR_PMBUS_ADDR, margin_cmd, margin_apply, voltage)

    return

# Only available for Part SKEW Need by hardware
def ir_set_vdd_core_dynamic_range_mavericks(arg_ir):

    #a = ir_open_i2c_switch()

    VDD_CORE_IR_I2C_BUS = "0x9"
    VDD_CORE_IR_PMBUS_ADDR = "0x40"
    IR_MARGIN_OFF = "0x80"
    IR_VOUT_CMD = "0x21"

    try:
        v = float(arg_ir[1])
    except ValueError:
        error_ir_usage()
        return

    if v < 0.65 or v > 0.95:
        print "Voltage value not in range .65 - .95"
        return
    voltage_scale = {0: "0xA6", 10: "0xA9", 20: "0xAC", 30: "0xAE", 40: "0xB1", 50: "0xB3", 60: "0xB6", 70: "0xB8",
               80: "0xBB",  85: "0xBC",  90: "0xBD", 100: "0xC0", 105: "0xC1", 110: "0xC2", 120: "0xC5", 125: "0xC6",
              130: "0xC8", 135: "0xC9", 140: "0xCA", 150: "0xCD", 155: "0xCE", 160: "0xCF", 170: "0xD2", 175: "0xD3",
              180: "0xD4", 185: "0xD5", 190: "0xD7", 200: "0xD9", 205: "0xDA", 210: "0xDC", 220: "0xDF", 225: "0xE0",
              230: "0xE1", 235: "0xE2", 240: "0xE4", 250: "0xE6", 255: "0xE7", 260: "0xE9", 270: "0xEC", 280: "0xEE",
              290: "0xF1", 300: "0xF3"}

    # Convert to mv with -8 exponent
    i = (v * 1000) % 650
    voltage = voltage_scale.get(i)

    if voltage == None:
        error_ir_usage()
        return

    margin_cmd = IR_VOUT_CMD
    margin_apply = IR_MARGIN_OFF
    set_ir_voltage("VDD_CORE", VDD_CORE_IR_I2C_BUS, VDD_CORE_IR_PMBUS_ADDR, margin_cmd, margin_apply, voltage)

    #ir_restore_i2c_switch(a)
    return

# Only available for Part SKEW Need by hardware
def ir_set_vddl_core_dynamic_range_stinson(arg_ir):
    global nolimit_ir_vdd_core

    VDDL_CORE_IR_I2C_BUS = "0x1"
    VDDL_CORE_IR_PMBUS_ADDR = "0x47"
    IR_MARGIN_OFF = "0x80"
    IR_VOUT_CMD = "0x21"

    try:
        v = float(arg_ir[1])
    except ValueError:
        error_ir_usage()
        return

    if nolimit_ir_vdd_core == 0:
        if v < 0.65 or v > 0.925:
            print "Voltage value not in range .65 - .925"
            return
#    voltage_scale = {0: "0xA66", 10: "0xA8F", 20: "0xAB8", 30: "0xAE1", 40: "0xB0A", 50: "0xB33", 60: "0xB5C", 70: "0xB85",
#               80: "0xBAE", 90: "0xBD7", 100: "0xC00", 110: "0xC29", 120: "0xC52",
#              130: "0xC7B", 140: "0xCA4", 150: "0xCCD", 160: "0xCF6", 170: "0xD1F", 180: "0xD48", 190: "0xD71", 200: "0xD9A"}

    # Convert to mv with -8 exponent
#    i = (v * 1000) % 650
#    voltage = voltage_scale.get(i)

#   allow vdd_core setting in 1 mV increment in the range .65V and .85V. It seems that register settings
#   for such a range is linear.
#   0.65V --> 0xA66 (2662 in decimal)
#   0.85V --> 0xD9A (3482 in decimal)
#   increment for 1mV is 4.1 ( 3482-2662)/200 )
    v = ((v - 0.65) * 1000 * 4.1) + 2662


    if v == None:
        error_ir_usage()
        return

    # set page register in IR
    set_ir_page(VDDL_CORE_IR_I2C_BUS, VDDL_CORE_IR_PMBUS_ADDR, "0")

    margin_cmd = IR_VOUT_CMD
    margin_apply = IR_MARGIN_OFF
    set_ir_voltage("VDDL_CORE", VDDL_CORE_IR_I2C_BUS, VDDL_CORE_IR_PMBUS_ADDR, margin_cmd, margin_apply, hex(int(v)))

    return



# Only available for Part SKEW Need by hardware
def ir_set_vdd_core_dynamic_range_newport(arg_ir):
    global nolimit_ir_vdd_core

    VDD_CORE_IR_I2C_BUS = "0x1"
    VDD_CORE_IR_PMBUS_ADDR = "0x40"
    IR_MARGIN_OFF = "0x80"
    IR_VOUT_CMD = "0x21"

    try:
        v = float(arg_ir[1])
    except ValueError:
        error_ir_usage()
        return

    if nolimit_ir_vdd_core == 0:
        if v < 0.65 or v > 0.925:
            print "Voltage value not in range .65 - .925"
            return
#    voltage_scale = {0: "0xA66", 10: "0xA8F", 20: "0xAB8", 30: "0xAE1", 40: "0xB0A", 50: "0xB33", 60: "0xB5C", 70: "0xB85",
#               80: "0xBAE", 90: "0xBD7", 100: "0xC00", 110: "0xC29", 120: "0xC52",
#              130: "0xC7B", 140: "0xCA4", 150: "0xCCD", 160: "0xCF6", 170: "0xD1F", 180: "0xD48", 190: "0xD71", 200: "0xD9A"}

    # Convert to mv with -8 exponent
#    i = (v * 1000) % 650
#    voltage = voltage_scale.get(i)

#   allow vdd_core setting in 1 mV increment in the range .65V and .85V. It seems that register settings
#   for such a range is linear.
#   0.65V --> 0xA66 (2662 in decimal)
#   0.85V --> 0xD9A (3482 in decimal)
#   increment for 1mV is 4.1 ( 3482-2662)/200 )
    v = ((v - 0.65) * 1000 * 4.1) + 2662


    if v == None:
        error_ir_usage()
        return

    # set page register in IR
    set_ir_page(VDD_CORE_IR_I2C_BUS, VDD_CORE_IR_PMBUS_ADDR, "0")

    margin_cmd = IR_VOUT_CMD
    margin_apply = IR_MARGIN_OFF
    set_ir_voltage("VDD_CORE", VDD_CORE_IR_I2C_BUS, VDD_CORE_IR_PMBUS_ADDR, margin_cmd, margin_apply, hex(int(v)))

    return

# Only available for Part SKEW Need by hardware
def ir_set_vdd_core_dynamic_range_stinson(arg_ir):
    global nolimit_ir_vdd_core

    VDD_CORE_IR_I2C_BUS = "0x1"
    VDD_CORE_IR_PMBUS_ADDR = "0x40"
    IR_MARGIN_OFF = "0x80"
    IR_VOUT_CMD = "0x21"

    try:
        v = float(arg_ir[1])
    except ValueError:
        error_ir_usage()
        return

    if nolimit_ir_vdd_core == 0:
        if v < 0.65 or v > 0.925:
            print "Voltage value not in range .65 - .925"
            return
#    voltage_scale = {0: "0xA66", 10: "0xA8F", 20: "0xAB8", 30: "0xAE1", 40: "0xB0A", 50: "0xB33", 60: "0xB5C", 70: "0xB85",
#               80: "0xBAE", 90: "0xBD7", 100: "0xC00", 110: "0xC29", 120: "0xC52",
#              130: "0xC7B", 140: "0xCA4", 150: "0xCCD", 160: "0xCF6", 170: "0xD1F", 180: "0xD48", 190: "0xD71", 200: "0xD9A"}

    # Convert to mv with -8 exponent
#    i = (v * 1000) % 650
#    voltage = voltage_scale.get(i)

#   allow vdd_core setting in 1 mV increment in the range .65V and .85V. It seems that register settings
#   for such a range is linear.
#   0.65V --> 0xA66 (2662 in decimal)
#   0.85V --> 0xD9A (3482 in decimal)
#   increment for 1mV is 4.1 ( 3482-2662)/200 )
    v = ((v - 0.65) * 1000 * 4.1) + 2662


    if v == None:
        error_ir_usage()
        return

    # set page register in IR
    set_ir_page(VDD_CORE_IR_I2C_BUS, VDD_CORE_IR_PMBUS_ADDR, "0")

    margin_cmd = IR_VOUT_CMD
    margin_apply = IR_MARGIN_OFF
    set_ir_voltage("VDD_CORE", VDD_CORE_IR_I2C_BUS, VDD_CORE_IR_PMBUS_ADDR, margin_cmd, margin_apply, hex(int(v)))

    return

# Only available for Part SKEW Need by hardware
def ir_set_vdd_core_dynamic_range_davenport(arg_ir):
    global nolimit_ir_vdd_core

    VDD_CORE_IR_I2C_BUS = "0x1"
    VDD_CORE_IR_PMBUS_ADDR = "0x40"
    IR_MARGIN_OFF = "0x80"
    IR_VOUT_CMD = "0x21"

    try:
        v = float(arg_ir[1])
    except ValueError:
        error_ir_usage()
        return

    if nolimit_ir_vdd_core == 0:
        if v < 0.65 or v > 0.925:
            print "Voltage value not in range .65 - .925"
            return
#    voltage_scale = {0: "0xA66", 10: "0xA8F", 20: "0xAB8", 30: "0xAE1", 40: "0xB0A", 50: "0xB33", 60: "0xB5C", 70: "0xB85",
#               80: "0xBAE", 90: "0xBD7", 100: "0xC00", 110: "0xC29", 120: "0xC52",
#              130: "0xC7B", 140: "0xCA4", 150: "0xCCD", 160: "0xCF6", 170: "0xD1F", 180: "0xD48", 190: "0xD71", 200: "0xD9A"}

    # Convert to mv with -8 exponent
#    i = (v * 1000) % 650
#    voltage = voltage_scale.get(i)

#   allow vdd_core setting in 1 mV increment in the range .65V and .85V. It seems that register settings
#   for such a range is linear.
#   0.65V --> 0xA66 (2662 in decimal)
#   0.85V --> 0xD9A (3482 in decimal)
#   increment for 1mV is 4.1 ( 3482-2662)/200 )
    v = ((v - 0.65) * 1000 * 4.1) + 2662


    if v == None:
        error_ir_usage()
        return

    # set page register in IR
    set_ir_page(VDD_CORE_IR_I2C_BUS, VDD_CORE_IR_PMBUS_ADDR, "0")

    margin_cmd = IR_VOUT_CMD
    margin_apply = IR_MARGIN_OFF
    set_ir_voltage("VDD_CORE", VDD_CORE_IR_I2C_BUS, VDD_CORE_IR_PMBUS_ADDR, margin_cmd, margin_apply, hex(int(v)))

    return

# Only available for Part SKEW Need by hardware
def ir_set_vddl_core_dynamic_range_davenport(arg_ir):
    global nolimit_ir_vdd_core

    VDDL_CORE_IR_I2C_BUS = "0x1"
    VDDL_CORE_IR_PMBUS_ADDR = "0x40"
    IR_MARGIN_OFF = "0x80"
    IR_VOUT_CMD = "0x21"

    try:
        v = float(arg_ir[1])
    except ValueError:
        error_ir_usage()
        return

    if nolimit_ir_vdd_core == 0:
        if v < 0.65 or v > 0.925:
            print "Voltage value not in range .65 - .925"
            return
#    voltage_scale = {0: "0xA66", 10: "0xA8F", 20: "0xAB8", 30: "0xAE1", 40: "0xB0A", 50: "0xB33", 60: "0xB5C", 70: "0xB85",
#               80: "0xBAE", 90: "0xBD7", 100: "0xC00", 110: "0xC29", 120: "0xC52",
#              130: "0xC7B", 140: "0xCA4", 150: "0xCCD", 160: "0xCF6", 170: "0xD1F", 180: "0xD48", 190: "0xD71", 200: "0xD9A"}

    # Convert to mv with -8 exponent
#    i = (v * 1000) % 650
#    voltage = voltage_scale.get(i)

#   allow vdd_core setting in 1 mV increment in the range .65V and .85V. It seems that register settings
#   for such a range is linear.
#   0.65V --> 0xA66 (2662 in decimal)
#   0.85V --> 0xD9A (3482 in decimal)
#   increment for 1mV is 4.1 ( 3482-2662)/200 )
    v = ((v - 0.65) * 1000 * 4.1) + 2662


    if v == None:
        error_ir_usage()
        return

    # set page register in IR
    set_ir_page(VDDL_CORE_IR_I2C_BUS, VDDL_CORE_IR_PMBUS_ADDR, "1")

    margin_cmd = IR_VOUT_CMD
    margin_apply = IR_MARGIN_OFF
    set_ir_voltage("VDDL_CORE", VDDL_CORE_IR_I2C_BUS, VDDL_CORE_IR_PMBUS_ADDR, margin_cmd, margin_apply, hex(int(v)))

    return



def ir_voltage_set_newport(arg_ir):

    IR_I2C_BUS = "0x1"
    IR_PMBUS_ADDR = {1: "0x40", 2: "0x42", 3: "0x44", 4:"0x44", 5:"0x46"}
    PAGE_ADDR = "0"
    string = {1: "VDD_CORE", 2: "VDDT_0.9V", 3: "VDDA_1.7V", 4:"VDDA_AGC_1.8V", 5:"QSFP"}

    IR_MARGIN_LOW_AOF_OP = "0x98"
    IR_MARGIN_HIGH_AOF_OP = "0xA8"
    IR_MARGIN_OFF = "0x80"
    IR_OPERATION = "0x1"

    IR_VOUT_MARGIN_HIGH = "0x25"
    IR_VOUT_MARGIN_LOW = "0x26"
    IR_VOUT_CMD = "0x21"

    if arg_ir[2] == "VDDA_1.7V":

      # set page register in IR
      set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(3), "0")
      # margin voltage +3% -3%  0x1b6=>438
      # VOLT_NORMAL = "0x180" for 1.5V
      # VOLT_MARGIN_HIGH = "0x18C" for 15V
      # VOLT_MARGIN_LOW = "0x174" for 1.5V
      VOLT_MARGIN_HIGH = "0x1C0"
      VOLT_MARGIN_LOW = "0x1A8"
      VOLT_NORMAL = "0x1B4"
      i2c_addr = IR_PMBUS_ADDR.get(3)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    elif arg_ir[2] == "VDD_CORE":

      # set page register in IR
      set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(1), "0")
      # voltage +3% -3%
      VOLT_MARGIN_HIGH = "0xC5C"
      VOLT_MARGIN_LOW = "0xBA4"
      VOLT_NORMAL =  "0xC00"
      i2c_addr = IR_PMBUS_ADDR.get(1)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    elif arg_ir[2] == "QSFP":

      # set page register in IR +/- 5%
      set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(5), "0")
      VOLT_MARGIN_HIGH = "0x1BC"
      VOLT_MARGIN_LOW = "0x191"
      VOLT_NORMAL =  "0x1A6"
      i2c_addr = IR_PMBUS_ADDR.get(5)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    elif arg_ir[2] == "VDDT_0.9V":

      set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(2), "0")
      # set page register in IR +/- 3%
      VOLT_MARGIN_HIGH = "0xED"
      VOLT_MARGIN_LOW = "0xDF"
      VOLT_NORMAL =  "0xE6"
      i2c_addr = IR_PMBUS_ADDR.get(2)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    elif arg_ir[2] == "VDDA_AGC_1.8V":
      # set page register in IR +/- 3%
      set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(4), "1")
      VOLT_MARGIN_HIGH = "0x1DB"
      VOLT_MARGIN_LOW = "0x1BF"
      VOLT_NORMAL =  "0x1CD"

      #set VDD
      i2c_addr = IR_PMBUS_ADDR.get(4)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    else:
        error_ir_usage()

    return

def ir_voltage_set_stinson(arg_ir):

    IR_I2C_BUS = "0x1"
    IR_PMBUS_ADDR = {1: "0x40", 2: "0x40", 3: "0x47", 4:"0x47", 5:"0x58"}
    PAGE_ADDR = "0"
    string = {1: "VDD_CORE", 2: "VDDAH", 3: "VDDL_CORE", 4:"VDDAL", 5:"QSFP"}

    IR_MARGIN_LOW_AOF_OP = "0x98"
    IR_MARGIN_HIGH_AOF_OP = "0xA8"
    IR_MARGIN_OFF = "0x80"
    IR_OPERATION = "0x1"

    IR_VOUT_MARGIN_HIGH = "0x25"
    IR_VOUT_MARGIN_LOW = "0x26"
    IR_VOUT_CMD = "0x21"

    if arg_ir[2] == "VDDL_CORE":

      # set page register in IR
      set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(3), "0")
      # margin voltage +3% -3%  0x1b6=>438
      # VOLT_NORMAL = "0x180" for 1.5V
      # VOLT_MARGIN_HIGH = "0x18C" for 15V
      # VOLT_MARGIN_LOW = "0x174" for 1.5V
      VOLT_MARGIN_HIGH = "0x1C0"
      VOLT_MARGIN_LOW = "0x1A8"
      VOLT_NORMAL = "0x1B4"
      i2c_addr = IR_PMBUS_ADDR.get(3)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    elif arg_ir[2] == "VDD_CORE":

      # set page register in IR
      set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(1), "0")
      # voltage +3% -3%
      VOLT_MARGIN_HIGH = "0xC5C"
      VOLT_MARGIN_LOW = "0xBA4"
      VOLT_NORMAL =  "0xC00"
      i2c_addr = IR_PMBUS_ADDR.get(1)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    elif arg_ir[2] == "QSFP":

      # set page register in IR +/- 5%
      set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(5), "0")
      VOLT_MARGIN_HIGH = "0x1BC"
      VOLT_MARGIN_LOW = "0x191"
      VOLT_NORMAL =  "0x1A6"
      i2c_addr = IR_PMBUS_ADDR.get(5)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    elif arg_ir[2] == "VDDAH":

      set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(2), "0")
      # set page register in IR +/- 3%
      VOLT_MARGIN_HIGH = "0xED"
      VOLT_MARGIN_LOW = "0xDF"
      VOLT_NORMAL =  "0xE6"
      i2c_addr = IR_PMBUS_ADDR.get(2)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    elif arg_ir[2] == "VDDAL":
      # set page register in IR +/- 3%
      set_ir_page(IR_I2C_BUS, IR_PMBUS_ADDR.get(4), "1")
      VOLT_MARGIN_HIGH = "0x1DB"
      VOLT_MARGIN_LOW = "0x1BF"
      VOLT_NORMAL =  "0x1CD"

      #set VDD
      i2c_addr = IR_PMBUS_ADDR.get(4)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    else:
        error_ir_usage()

    return

def ir_voltage_set_mavericks(arg_ir, p0c):

    #a = ir_open_i2c_switch()

    UPPER_IR_I2C_BUS = "0x9"
    UPPER_IR_PMBUS_ADDR = {1: "0x40", 2: "0x74", 3: "0x71"}
    string_upper = {1: "VDD_CORE", 2: "AVDD", 3: "QSFP_UPPER"}
    LOWER_IR_I2C_BUS = "0x1"
    if (p0c == 1):
      LOWER_IR_PMBUS_ADDR = {1: "0x71", 2: "0x72", 3: "0x70"}
      lower_string = {1: "QSFP_LOWER", 2: "RETIMER_VDDA", 3: "RETIMER_VDD"}
    else :
      LOWER_IR_PMBUS_ADDR = {1: "0x71", 2: "0x72"}
      lower_string = {1: "QSFP_LOWER", 2: "REPEATER"}

    IR_MARGIN_LOW_AOF_OP = "0x98"
    IR_MARGIN_HIGH_AOF_OP = "0xA8"
    IR_MARGIN_OFF = "0x80"
    IR_OPERATION = "0x1"

    IR_VOUT_MARGIN_HIGH = "0x25"
    IR_VOUT_MARGIN_LOW = "0x26"
    IR_VOUT_CMD = "0x21"


    if arg_ir[2] == "AVDD":

      # voltage +3% -3%  0x1b6=>438
      VOLT_MARGIN_HIGH = "0x1DB"
      VOLT_MARGIN_LOW = "0x1BF"
      VOLT_NORMAL = "0x1CE"
      i2c_addr = UPPER_IR_PMBUS_ADDR.get(2)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], UPPER_IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    elif arg_ir[2] == "VDD_CORE":

      # voltage +2% -2%  0x1AE=>430
      VOLT_MARGIN_HIGH = "0x0DB"
      VOLT_MARGIN_LOW = "0x0D3"
      VOLT_NORMAL =  "0x0D7"
      i2c_addr = UPPER_IR_PMBUS_ADDR.get(1)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], UPPER_IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    elif arg_ir[2] == "QSFP_UPPER":

      VOLT_MARGIN_HIGH = "0x361"
      VOLT_MARGIN_LOW = "0x323"
      VOLT_NORMAL =  "0x34D"
      i2c_addr = UPPER_IR_PMBUS_ADDR.get(3)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], UPPER_IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    elif arg_ir[2] == "QSFP_LOWER":

      VOLT_MARGIN_HIGH = "0x361"
      VOLT_MARGIN_LOW = "0x323"
      VOLT_NORMAL =  "0x34D"
      i2c_addr = LOWER_IR_PMBUS_ADDR.get(1)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], LOWER_IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    elif arg_ir[2] == "RETIMER_VDD":
      if (p0c != 1):
         error_ir_usage()
         return
      VOLT_MARGIN_HIGH = "0x20F"
      VOLT_MARGIN_LOW = "0x1F1"
      VOLT_NORMAL =  "0x200"

      #set VDD
      i2c_addr = LOWER_IR_PMBUS_ADDR.get(3)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], LOWER_IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    #set VDDA
    elif arg_ir[2] == "RETIMER_VDDA":
      if (p0c != 1):
         error_ir_usage()
         return
      i2c_addr = LOWER_IR_PMBUS_ADDR.get(2)
      VOLT_A_MARGIN_HIGH = "0x3B5"
      VOLT_A_MARGIN_LOW = "0x37E"
      VOLT_A_NORMAL =  "0x39A"

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_A_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_A_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_A_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], LOWER_IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    elif arg_ir[2] == "REPEATER":
      if (p0c != 0):
         error_ir_usage()
         return

      VOLT_MARGIN_HIGH = "0x2A0"
      VOLT_MARGIN_LOW = "0x260"
      VOLT_NORMAL =  "0x280"
      i2c_addr = LOWER_IR_PMBUS_ADDR.get(2)

      if arg_ir[1] == "l":
        margin_cmd = IR_VOUT_MARGIN_LOW
        margin_apply = IR_MARGIN_LOW_AOF_OP
        voltage = VOLT_MARGIN_LOW

      elif arg_ir[1] == "h":
        margin_cmd = IR_VOUT_MARGIN_HIGH
        margin_apply = IR_MARGIN_HIGH_AOF_OP
        voltage = VOLT_MARGIN_HIGH

      elif arg_ir[1] == "n":
        margin_cmd = IR_VOUT_CMD
        margin_apply = IR_MARGIN_OFF
        voltage = VOLT_NORMAL

      else:
        error_ir_usage()
        return

      set_ir_voltage(arg_ir[2], LOWER_IR_I2C_BUS, i2c_addr, margin_cmd, margin_apply, voltage)

    else:
        error_ir_usage()

    #ir_restore_i2c_switch(a)

    return

def ir(argv):
    global nolimit_ir_vdd_core

    arg_ir = argv[2:]

    if arg_ir[0] == "help" or arg_ir[0] == "h" or len(arg_ir) <= 0:
        error_ir_usage()
        return

    # ./btools.py --IR sh v [%s]
    if arg_ir[0] == "sh":
        if len(arg_ir) == 3:
            platform = arg_ir[2]
        elif len(arg_ir) == 2:
            platform = get_project()
        else:
            error_ir_usage()
            return
    # ./btools.py --IR set [%s] <margin> <voltage rail>
    elif arg_ir[0] == "set":
        if len(arg_ir) == 4:
            platform = arg_ir[1]
            arg_ir = arg_ir[0:1]+arg_ir[2:]
        elif len(arg_ir) == 3:
            platform = get_project()
        else:
            error_ir_usage()
            return
    # ./btools.py --IR set_vdd_core [%s] <voltage>
    elif arg_ir[0] == "set_vdd_core" or arg_ir[0] == "set_vddl_core":
        if len(arg_ir) == 3:
            platform = arg_ir[1]
            arg_ir = arg_ir[0:1]+arg_ir[2:]
        elif len(arg_ir) == 2:
            platform = get_project()
        else:
            error_ir_usage()
            return
    # ./btools.py --IR set_any_vdd_core [%s] <voltage>
    elif arg_ir[0] == "set_any_vdd_core":
        if len(arg_ir) == 3:
            platform = arg_ir[1]
            arg_ir = arg_ir[0:1]+arg_ir[2:]
        elif len(arg_ir) == 2:
            platform = get_project()
        else:
            error_ir_usage()
            return

    if arg_ir[0] == "sh":
        if platform == "mavericks":
            ir_voltage_show_mavericks(0)
        elif platform == "mavericks-p0c":
            ir_voltage_show_mavericks(1)
        elif platform == "montara":
            ir_voltage_show_montara()
        elif platform == "newport":
            ir_voltage_show_newport(arg_ir)
        elif platform == "davenport":
            ir_voltage_show_davenport(arg_ir);
        elif platform == "stinson":
            ir_voltage_show_stinson(arg_ir)
        else :
            error_ir_usage()
            return
    elif arg_ir[0] == "set":
        if platform == "mavericks":
            ir_voltage_set_mavericks(arg_ir, 0)
        elif platform == "mavericks-p0c":
            ir_voltage_set_mavericks(arg_ir, 1)
        elif platform == "montara":
            ir_voltage_set_montara(arg_ir)
        elif platform == "newport":
            ir_voltage_set_newport(arg_ir)
        elif platform == "stinson":
            ir_voltage_set_stinson(arg_ir)
        else :
            error_ir_usage()
            return
    elif arg_ir[0] == "set_vdd_core":
        nolimit_ir_vdd_core = 0
        if platform == "mavericks" or platform == "mavericks-p0c":
            ir_set_vdd_core_dynamic_range_mavericks(arg_ir)
        elif platform == "montara":
            ir_set_vdd_core_dynamic_range_montara(arg_ir)
        elif platform == "newport":
            ir_set_vdd_core_dynamic_range_newport(arg_ir)
        elif platform == "stinson":
            ir_set_vdd_core_dynamic_range_stinson(arg_ir)
        elif platform == "davenport":
            ir_set_vdd_core_dynamic_range_davenport(arg_ir)
        else :
            error_ir_usage()
            return
    elif arg_ir[0] == "set_vddl_core":
        nolimit_ir_vdd_core = 0
        if platform == "stinson":
            ir_set_vddl_core_dynamic_range_stinson(arg_ir)
        elif platform == "davenport":
            ir_set_vddl_core_dynamic_range_davenport(arg_ir)
        else :
            error_ir_usage()
            return
    elif arg_ir[0] == "set_any_vdd_core":
        nolimit_ir_vdd_core = 1
        if platform == "newport":
            syslog.syslog(syslog.LOG_WARNING, "btools.py IR: Set VDD_CORE without limitation")
            ir_set_vdd_core_dynamic_range_newport(arg_ir)
        else :
            error_ir_usage()
            return
    else:
        error_ir_usage()
        return

    return

def ucd_ir_voltage_margin(argv):

    return
#
# Temperature utility usage
#
def error_tmp_usage():

    print " "
    print "Usage:"
    print "./btools.py --TMP [%s] sh   => Show Temp" % h_platforms_with_p0c
    print " "
    print "Eg."
    print "btools.py --TMP sh"
    print " "

    return

#
# Temperature Convert Formula
#
TMP_MAC_MANU_ID_MAX6658=0x4D
TMP_MAC_MANU_ID_TMP43x=0x55
def convert_tmp(manu_id,tmp_o):
    manu_id=int(manu_id, 16)
    if manu_id == TMP_MAC_MANU_ID_MAX6658:
        if tmp_o&0x80 > 0:
            tmp = tmp_o-256
        else:
            tmp = tmp_o
    elif manu_id == TMP_MAC_MANU_ID_TMP43x:
        if tmp_o&0x80 > 0:
            tmp = tmp_o-64
        else:
            tmp = tmp_o
    else:
        syslog.syslog(syslog.LOG_INFO, "btools.py Warning: MANU_ID %02X is not in the supported list" % (manu_id))
        tmp = tmp_o

    return tmp

#
# Lower board temperature sensors. Board exists on Montara, Mavericks and Newport
#
def tmp_lower(board):

    i2c_dev = "/sys/class/i2c-adapter/i2c-3/3-00"

    tmp_sensor = {1: "48/temp1_input",
                  2: "49/temp1_input",
                  3: "4a/temp1_input",
                  4: "4b/temp1_input",
                  5: "4c/temp1_input"}

    if board == "Mavericks":
        x = 6
    else:
        x = 5

    for i in range(1, x):

        path = i2c_dev + tmp_sensor.get(i)
        try:
            cmd = "cat"
            output = subprocess.check_output([cmd, path])
            print " TMP SENSOR %.2d                  %.3f C" % (i,
                                                          float(output) / 1000)

        except subprocess.CalledProcessError as e:
            print e
            print "Error occured while processing TMP SENSOR %d" % i

    if board == "Montara" or board == "Newport" or board == "Stinson" or board == "Davenport":

        # Restore thermal sensor access from Newport R0B
        # System Assembly Part Number xxx-000004-02 is for R0A
        # System Assembly Part Number xxx-000004-03 is for R0B. - Aug. 14, 2020
        sys_pn = get_sys_assembly_pn()
        if board == "Newport" and sys_pn.find("000004-02") > 0:
            syslog.syslog(syslog.LOG_INFO, "btools.py TMP: Turn On for PVT reading (%s)" % (sys_pn.rstrip()))
            np_tvp_workaround = 1
        else:
            syslog.syslog(syslog.LOG_INFO, "btools.py TMP: Turn Off for PVT reading (%s)" % (sys_pn.rstrip()))
            np_tvp_workaround = 0

        try:
            cmd = "i2cget"
            #TMP SENSORS 05:
            output = subprocess.check_output([cmd, "-f", "-y", "3", "0x4d",
                                             "0x00", "w"])
            output = int(output, 16)
            t = (output & 0xff) << 8
            d = (output & 0xff00) >> 8
            swp_output = t+d
            resolution = 12
            t1 = ((swp_output >> (16-resolution)) * 1000) >> (resolution-8)
            if (output & 0xff) > 127:
                t1 = t1-256000
            print " TMP SENSOR %.2d                  %.3f C" % (5, float(t1)/1000)

            # Destinguish the 0x4C, MAX6658/TMP431/TMP432
            manu_id = subprocess.check_output([cmd, "-f", "-y", "3",
                                               "0x4c", "0xFE"])
            TMP_MAC_MANU_ID = hex(int(manu_id, 16))

            #TMP SENSORS LOCAL:
            output = subprocess.check_output([cmd, "-f", "-y", "3",
                                             "0x4c", "0x00"])
            output = int(output, 16)
            output=convert_tmp(TMP_MAC_MANU_ID, output)
            print " TMP SENSOR LOCAL               %.3f C" % output

            if np_tvp_workaround == 0:

              if board == "Stinson" :
                #TMP SENSORS REMOTE1:
                output = subprocess.check_output([cmd, "-f", "-y", "3",
                                             "0x4c", "0x01"])
                output = int(output, 16)
                output=convert_tmp(TMP_MAC_MANU_ID, output)
                print " TMP SENSOR TOFINO DIE0         %.3f C" % (output)
                #TMP SENSORS REMOTE2:
                output = subprocess.check_output([cmd, "-f", "-y", "3",
                                               "0x4c", "0x23"])
                output = int(output, 16)
                output=convert_tmp(TMP_MAC_MANU_ID, output)
                print " TMP SENSOR TOFINO DIE1         %.3f C" % (output)
                return
              if board == "Davenport" :
                #TMP SENSORS REMOTE1:
                output = subprocess.check_output([cmd, "-f", "-y", "3",
                                             "0x4c", "0x01"])
                output = int(output, 16)
                output=convert_tmp(TMP_MAC_MANU_ID, output)
                print " TMP SENSOR TOFINO DIE0         %.3f C" % (output)
                return
              if board == "Newport" :
                #TMP SENSORS REMOTE1:
                output = subprocess.check_output([cmd, "-f", "-y", "3",
                                             "0x4c", "0x01"])
                output = int(output, 16)
                output=convert_tmp(TMP_MAC_MANU_ID, output)
                print " TMP SENSOR TOFINO              %.3f C" % (output)
                return
              #TMP SENSORS REMOTE1:
              output = subprocess.check_output([cmd, "-f", "-y", "3",
                                             "0x4c", "0x01"])
              output = int(output, 16)
              output=convert_tmp(TMP_MAC_MANU_ID, output)
              print " TMP SENSOR REMOTE TOFINO       %.3f C" % (output)

            else:  # read PVT register thru BMC i2c instead
              cmd = "/usr/local/bin/i2c_set_get"
              open_upper_PCA9548_lock() # using the same lock mechanism though the name is not quite right
              output = subprocess.check_output([cmd, "11", "0x58", "5", "4",
                              "0xa0", "0xfc", "0x01", "0x08", "0x00"])
              close_upper_PCA9548_lock()
              oplist = output.split(" ", 3)
              lower = int(oplist[0], 0)
              upper = int(oplist[1], 0) & 0x3
              valid = int(oplist[1], 0) & 0x10
              if valid == 0: # reading is not valid
                print " TMP SENSOR TOFINO 0.0 C"
                return
              upper = (upper << 8) | lower
              x = float(upper)
              x2 = x * x
              temperature = (x2 * (-0.000011677)) + (x * 0.28031) - 66.599
              print " TMP SENSOR TOFINO              %.3f C" % (temperature)

        except subprocess.CalledProcessError as e:
            print e
            print "Error occured while processing i2cget for TMP SENSOR 05/LOCAL/REMOTE"

    return

#
# Upper board temperature sensors. It only exists in Mavericks
#
def tmp_upper(p0c):

    if (p0c == 1):
      TMP75_I2C_ADDR = {1: "0x48", 2: "0x49", 3: "0x4a", 4: "0x4b"}
      #TMP75_I2C_ADDR = {1: "0x4d", 2: "0x4e", 3: "0x4a", 4: "0x4b"}
    else:
      TMP75_I2C_ADDR = {1: "0x48", 2: "0x49", 3: "0x4a", 4: "0x4b"}

    for i in range(1, 5):

        try:
            cmd = "i2cget"
            a = tmp_open_i2c_switch()
            while (a < 0):
               time.sleep(0.010) # 10ms
               a = tmp_open_i2c_switch()
            output = subprocess.check_output([cmd, "-f", "-y", "9",
                                             TMP75_I2C_ADDR.get(i),
                                             "0x00", "w"])
            tmp_restore_i2c_switch(a)
            output = int(output, 16)

            t = output & 0xff
            d = output & 0xfff00
            t1 = float(t)
            if t > 127:
              t1 = float(257 - t) * (-1.0)
              if d == 0x8000:
                t1 = t1 - 0.5
            # if d is 0x80 means .0625 * 8(consider only fourth nibble 2 ^ 3)
            if d == 0x8000:
                t1 = float(t) + .500

            print " TMP SENSOR UPPER %.2d            %.3f C" % (i, t1)

        except subprocess.CalledProcessError as e:
            print e
            print "Error occured while processing i2cget for Tmp75 %.2d " % (i)
            tmp_restore_i2c_switch(a)

    try:
        cmd = "i2cget"
        a = tmp_open_i2c_switch()
        while (a < 0):
          time.sleep(0.010) # 10ms
          a = tmp_open_i2c_switch()

        # Destinguish the 0x4C, MAX6658/TMP431/TMP432
        manu_id = subprocess.check_output([cmd, "-f", "-y", "9",
                                           "0x4c", "0xFE"])
        TMP_MAC_MANU_ID = hex(int(manu_id, 16))

        # TMP UPPER SENSORS LOCAL:
        output = subprocess.check_output([cmd, "-f", "-y", "9",
                                          "0x4c", "0x00"])
        output = int(output, 16)
        output=convert_tmp(TMP_MAC_MANU_ID, output)
        print " TMP SENSOR UPPER LOCAL     %.2d.000 C" % (output)

        # TMP UPPER SENSORS REMOTE:
        output = subprocess.check_output([cmd, "-f", "-y", "9",
                                          "0x4c", "0x01"])
        tmp_restore_i2c_switch(a)
        output = int(output, 16)
        output=convert_tmp(TMP_MAC_MANU_ID, output)
        print " TMP SENSOR UPPER REMOTE Tofino    %.2d.000 C" % (output)

    except subprocess.CalledProcessError as e:
        print e
        print "Error occured while processing i2cget for TMP SENSOR UPPER LOCAL/REMOTE"
        tmp_restore_i2c_switch(a)

    return

#
# Mavericks need i2c switch to be opened for reading temp sensors
#
def tmp_open_i2c_switch():

    open_upper_PCA9548_lock()

    TMP_I2C_SW_BUS = "9"
    TMP_I2C_SW_ADDR = "0x70"

    try:
        get_cmd = "i2cget"
        output = subprocess.check_output([get_cmd, "-f", "-y",
                                          TMP_I2C_SW_BUS,
                                          TMP_I2C_SW_ADDR])

        output = int(output, 16)

        # opening i2c switch for 0x4a, 0x4b, 0x4c
        res = output | 0x10
        set_cmd = "i2cset"
        o = subprocess.check_output([set_cmd, "-f", "-y",
                                    TMP_I2C_SW_BUS,
                                    TMP_I2C_SW_ADDR, str(res)])
        #close_upper_PCA9548_lock()

    except subprocess.CalledProcessError as e:
        print e
        print "Error occured while processing opening i2c switch" \
              " on mavericks upper board"
        output = -1
        #close_upper_PCA9548_lock()

    return output

#
# Restoring the i2c switch state
#
def tmp_restore_i2c_switch(res):
    output = 0
    #open_upper_PCA9548_lock()

    TMP_I2C_SW_BUS = "9"
    TMP_I2C_SW_ADDR = "0x70"

    try:
        set_cmd = "i2cset"
        o = subprocess.check_output([set_cmd, "-f", "-y",
                                    TMP_I2C_SW_BUS,
                                    TMP_I2C_SW_ADDR, str(res)])
        close_upper_PCA9548_lock()

    except subprocess.CalledProcessError as e:
        print e
        print "Error occured while processing restoring i2c switch" \
              " on mavericks upper board"
        close_upper_PCA9548_lock()
        output = -1

    return output

#
# Dispatching temperature sensor requests
#
def tmp(argv):

    arg_tmp = argv[2:]

    if arg_tmp[0] == "help" or arg_tmp[0] == "h" or len(arg_tmp) <= 0:
        error_tmp_usage()
        return

    # ./btools.py --TMP [%s] sh
    if arg_tmp[0] == "sh":
        if len(arg_tmp) != 1:
            error_tmp_usage()
            return
        else:
            platform = get_project()
    else:
        if len(arg_tmp) == 2:
            platform = arg_tmp[0]
            arg_tmp = arg_tmp[1:]
        else:
            error_tmp_usage()
            return

    if arg_tmp[0] == "sh":
        if platform == "montara":
            tmp_lower("Montara")
        elif platform == "newport":
            tmp_lower("Newport")
        elif platform == "stinson":
            tmp_lower("Stinson")
        elif platform == "davenport":
            tmp_lower("Davenport")
        elif platform == "mavericks":
            tmp_lower("Mavericks")
            #a = tmp_open_i2c_switch()
            tmp_upper(0)
            #tmp_restore_i2c_switch(a)
        elif platform == "mavericks-p0c":
            #a = tmp_open_i2c_switch()
            tmp_lower("Mavericks")
            tmp_upper(1)
            #tmp_restore_i2c_switch(a)
        else:
            error_tmp_usage()
            return
    else:
        error_tmp_usage()
        return

    return

def error_usage():
    print "Error in arguments passed. Please look at usage."
    usage()
    return

# Main function parses command line argument and call appropiate tool
def main(argv):
    os.system("touch /tmp/btools_lock")
    try:
        opts, args = getopt.getopt(argv[1:], "hP:U:I:T:", ["help", "PSU=", "UCD=", "IR=", "TMP="])

        # No standard identifier.print the usage
        if len(opts) == 0:
            print "Number of invalid arguments %d " % len(args)
            error_usage()
            return

    except getopt.GetoptError:
        error_usage()
        return

    lock_file = "/tmp/btools_lock"
    fd = open(lock_file,'r')
    fcntl.flock(fd, fcntl.LOCK_EX)

    for opt, arg in opts:
        if opt in ("-h", "--help"):
            usage()
        elif opt in ("-P", "--PSU"):
            psu(argv)
        elif opt in ("-U", "--UCD"):
            ucd(argv)
        elif opt in ("-I", "--IR"):
            ir(argv)
        elif opt in ("-T", "--TMP"):
            tmp(argv)
        else:
            error_usage()

    fcntl.flock(fd, fcntl.LOCK_UN)
    fd.close()

    return

if __name__ == '__main__':
    sys.exit(main(sys.argv))

