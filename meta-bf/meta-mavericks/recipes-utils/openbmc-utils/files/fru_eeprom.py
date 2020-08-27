#!/usr/bin/python
#
# Accton Technology Corporation
#
# @Subject: To program FRU EEPROM (a.k.a. Board ID EEPROM) via BMC
# @Project: Montara, Maverick and Newport
# @Version: root@bmc:~# python -V
#           Python 2.7.9
#
# How to use
# 1. Read the fru eeprom by
#    root@bmc:~# ./fru_eeprom.py r
#
# 2. Program the fru eeprom by
#    root@bmc:~# ./fru_eeprom.py w
#    Hint: While filled with each one EEPROM fields, pressing 'enter' without any input
#                       will preserve in original data.
#
# Created by Jeremy Chen, 19/01/28
#
import binascii
import time
import sys
import subprocess

class DiagError(Exception):
    def __init__(self, value):
        self.returncode = value

    def __str__(self):
        return repr(self.value)

# Execute cmd on BMC shell
def write_cmd(cmd):
    return subprocess.check_output(cmd,
        stderr=subprocess.STDOUT, shell=True)

g_result = []
g_fru_board = ""
crc8_calc = ""
crc8_eeprom = ""
CRC = 0

eeprom_size = 0
ee_info = ""
i2c_offset_cmd = ""
i2c_read_cmd = ""

program_enable = 0
brief_enable = 0

x_bmc_fru_eeprom_cmd_prompt = 0
x_bmc_fru_eeprom_size = 1
x_bmc_fru_eeprom_is_hex = 2
x_bmc_fru_eeprom_value = 3
x_bmc_fru_eeprom_need_update = 4
x_bmc_fru_eeprom_fru_only = 5
x_bmc_fru_eeprom_fru_na = 6

bmc_fru_eeprom_data = {
    1: ["magic word(2 bytes): 0x",
        2, True, "", False, False, False],
    2: ["format version(1 byte): 0x",
        1, True, "", False, False, False],
    3: ["product name(12 bytes): ",
        12, False, "", False, False, False],
    4: ["product part number(8 bytes): ",
        8, False, "", False, True, False],
    5: ["system assembly part number(12 bytes): ",
        12, False, "", False, False, False],
    6: ["Facebook PCBA part number(12 bytes): ",
        12, False, "", False, False, False],
    7: ["Facebook PCB part number(12 bytes): ",
        12, False, "", False, False, False],
    8: ["ODM PCBA part number(13 bytes): ",
        13, False, "", False, False, False],
    9: ["ODM PCBA serial number(12 bytes): ",
        12, False, "", False, False, False],
    10: ["product production state(1 byte): 0x",
         1, True, "", False, False, False],
    11: ["product version(1 byte): 0x",
         1, True, "", False, False, False],
    12: ["product sub version(1 byte): 0x",
         1, True, "", False, False, False],
    13: ["product serial number(12 bytes): ",
         12, False, "", False, False, False],
    14: ["product asset tag(12 bytes): ",
         12, False, "", False, True, False],
    15: ["system manufacturer(8 bytes): ",
         8, False, "", False, False, False],
    16: ["system manufacturing date(4 bytes: mmddyyyy): ",
         4, True, "", False, False, False],
    17: ["PCB manufacturer(8 bytes): ",
         8, False, "", False, False, False],
    18: ["assembled at(8 bytes): ",
         8, False, "", False, False, False],
    19: ["local MAC address(12 bytes): ",
         12, False, "", False, True, False],
    20: ["extended Mac address base(12 bytes): ",
         12, False, "", False, True, False],
    21: ["extended MAC address size(2 bytes): 0x",
         2, True, "", False, True, False],
    22: ["EEPROM location on fabric(8 bytes): ",
         8, False, "", False, True, False],
}
set_ques_no = []

fru_list = {
    'SMB':(6, 0x51), 
    'SCM':(18, 0x52),
    'FCM-T':(65, 0x51), 'FCM-B':(73, 0x51), 
    'FAN-1':(71, 0x52), 'FAN-2':(79, 0x52), 
    'FAN-3':(70, 0x52), 'FAN-4':(78, 0x52), 
    'FAN-5':(69, 0x52), 'FAN-6':(77, 0x52), 
    'FAN-7':(68, 0x52), 'FAN-8':(76, 0x52),
    'PIM-1':(81, 0x56), 'PIM-2':(89, 0x56),
    'PIM-3':(97, 0x56), 'PIM-4':(105, 0x56),
    'PIM-5':(113, 0x56),'PIM-6':(121, 0x56),
    'PIM-7':(129, 0x56),'PIM-8':(137, 0x56),
}

fru_na_list = {
    'SMB':(5),
    'SCM':(19,20,21), 
    'FCM':(4,5,14,19,20,21),
    'FAN':(4,14,19,20,21),
    'PIM':(19,20,21)
}

def usage():
    print " "
    print "USAGE: "
    print "./fru_eeprom.py <r|w>"
    print "Eg:"
    print "./fru_eeprom.py r   => Read FRU EEPROM"
    print "./fru_eeprom.py w   => Write FRU EEPROM"
    print " "
    return

def get_board_ee_info():
    global i2c_read_cmd, i2c_offset_cmd
    global ee_info

    i2c_read_cmd = 'i2cget -f -y %d 0x%02x' % fru_list[g_fru_board]
    if g_fru_board[0:3] == 'FCM':
        i2c_offset_cmd = 'i2cset -f -y %d 0x%02x 0x0' % fru_list[g_fru_board]
        ee_info = '24C02'
    else:
        i2c_offset_cmd = 'i2cset -f -y %d 0x%02x 0x0 0x0' % (fru_list[g_fru_board])
        ee_info = '24C64'
    return

def calc_eeprom_size():
    global eeprom_size
    eeprom_size = 0

    for entryno in bmc_fru_eeprom_data:
        eeprom_size += bmc_fru_eeprom_data[entryno][x_bmc_fru_eeprom_size]
    eeprom_size += 1  # 1 byte for crc
    print ("Total EEPROM size is %d" % eeprom_size)
    return

def calculate_crc8_checksum(crc, data):
    i = data ^ crc
    crc = 0
    if (i & 1):
        crc ^= 0x5e
    if (i & 2):
        crc ^= 0xbc
    if (i & 4):
        crc ^= 0x61
    if (i & 8):
        crc ^= 0xc2
    if (i & 0x10):
        crc ^= 0x9d
    if (i & 0x20):
        crc ^= 0x23
    if (i & 0x40):
        crc ^= 0x46
    if (i & 0x80):
        crc ^= 0x8c
    return crc

def calculate_crc8_checksum_of_buffer(list):
    global crc
    crc = 0
    for str in list:
        crc = calculate_crc8_checksum(crc, int(str, 16))
    return crc

def print_result_in_program_command_byte():
    idx = 0
    line = ""
    line = 'echo -e "\\'
    print ("\r\n%s" % line);
    line = ""
    for i in g_result:
        if idx % 16 == 15:
            line = line + r'\x' + i
            print ("%s\\" % line)
            line = ""
        elif idx == len(g_result)-1:
            line = line + r'\x' + i
            print ("%s\\" % line)
        else:
            line = line + r'\x' + i
        idx += 1
    print ("\" > /sys/class/i2c-adapter/i2c-6/6-0051/eeprom");
    print ("\r\n")

def print_result_in_byte():
    idx = 0
    print ("Board %s %s(0x%02x) under bus %d:" % 
            (g_fru_board, ee_info, 
            fru_list[g_fru_board][1], fru_list[g_fru_board][0]))
    print ("Offset: 00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F")
    print ("======================================================================"),
    
    for i in g_result:
        if idx % 16 == 0:
            print ("\r\n0x%04x: %s " % (idx, i)), 
        else:
            print ("%s " % i),
        idx += 1
    print ("\r\n")

def bmc_read_fru_eeprom():
    global g_result
    global program_enable
    global brief_enable
    g_result = []
    retry_count = 0    # Retry if fail to read EEPROM
    ret = 0

    try:
        rx_data = ""
        # set offset to 0
        write_cmd(i2c_offset_cmd)

        for i in range(eeprom_size):
            rx_data = write_cmd(i2c_read_cmd)
            if rx_data is None:
                if retry_count < 3 :
                    print("Read Nothing from EEPROM(%d)\r\n",
                        retry_count+1)
                    time.sleep(10)
                    i = i-1
                    retry_count = retry_count + 1
                    continue
                else:
                    print("Read Nothing from EEPROM and Retry Failure\r\n")
                    raise DiagError("Nothing read")
            g_result = g_result + ["%02x" % ord(binascii.a2b_hex(rx_data[2:4]))]

        # print read bytes here.
        if brief_enable == 0:
            print_result_in_byte()
        if program_enable == 1:
            print_result_in_program_command_byte()
        ret = parse_result_to_struct()

    except DiagError as err:
        return -1
    except Exception as e:
        print (repr(e))
        print (rx_data)
        return -1
    return ret

def parse_result_to_struct():
    ret = 0
    return ret

def print_result_in_format():
    return

def bmc_update_fru_eeprom():
    global g_fru_board

    idx = 0
    try:
        for entryno in bmc_fru_eeprom_data:
            entry = bmc_fru_eeprom_data[entryno]
            # update the modify part.
            if entry[x_bmc_fru_eeprom_need_update] is True:
                print (".")
                for i in range(entry[x_bmc_fru_eeprom_size]):
                    if ee_info == '24C64':
                        write_cmd("i2cset -f -y %d 0x%02x 0x%02x 0x%02x 0x%s i"
                                       % (fru_list[g_fru_board][0],
                                         fru_list[g_fru_board][1],
                                         ((i+idx) >> 8) & 0xff,
                                         (i+idx) & 0xff, g_result[i+idx]))
                    else:
                        write_cmd("i2cset -f -y %d 0x%02x 0x%02x 0x%s"
                                       % (fru_list[g_fru_board][0],
                                        fru_list[g_fru_board][1],
                                        (i+idx) & 0xff, g_result[i+idx]))
                    time.sleep(0.2)
            idx += entry[x_bmc_fru_eeprom_size]
        # write crc
        if ee_info == '24C64':
            write_cmd("i2cset -f -y %d 0x%02x 0x%02x 0x%02x 0x%s i" %
                (fru_list[g_fru_board][0], fru_list[g_fru_board][1],
                (idx >> 8) & 0xff, idx & 0xff, g_result[idx]))
        else:
            write_cmd("i2cset -f -y %d 0x%02x 0x%02x 0x%s" %
                (fru_list[g_fru_board][0], fru_list[g_fru_board][1],
                idx & 0xff, g_result[idx]))
    except Exception as e:
        print(repr(e))
        return -1
    return 0

def bmc_set_fru_eeprom_prompt(entryno, entry, idx, need_to_update):
    global g_result
    global set_ques_no
    try:
        if entry[x_bmc_fru_eeprom_is_hex]:
            max_input_size = entry[x_bmc_fru_eeprom_size] * 2
        else:
            max_input_size = entry[x_bmc_fru_eeprom_size]

        if len(set_ques_no) > 0:
            try:
                if entryno == 20: # extended Mac address base
                    set_ques_no.index(19) # 19 and 20 are dependent on barefoot
                else:
                    set_ques_no.index(entryno)
                line = raw_input("Please enter %s" % entry[x_bmc_fru_eeprom_cmd_prompt])
            except Exception as e:
                return need_to_update
        else:
            line = raw_input("Please enter %s" % entry[x_bmc_fru_eeprom_cmd_prompt])

        if len(line) > max_input_size:
            print ("Out of range for string length!")
            return
        elif len(line) == 0:
            return need_to_update

        entry[x_bmc_fru_eeprom_need_update] = True
        need_to_update = 1
        tmp_result = []
        do_reverse_byte = 0
        if entry[x_bmc_fru_eeprom_is_hex]:
            if entryno == 21: # Just for extended MAC address size format
                do_reverse_byte = 1
            line = line.zfill(max_input_size)
            if entryno == 16: # Just for manufacturing date format
                date = time.strptime(line, "%m%d%Y")
                year = date[0]                 # this is an integer, ex. 2018
                year_hex = "%04x" % year       # this is a string with hex format, ex. 07E2
                year_r = "".join(reversed([year_hex[i:i+2] for i in range(0, len(year_hex), 2)]))
                month = date[1]
                day = date[2]
                line = "%s" % year_r + "%02x" % month + "%02x" % day
            line = binascii.a2b_hex(line)
            for line_hex in line:
                tmp_result = tmp_result + ["%02x" % ord(line_hex)]
            if do_reverse_byte == 1:
                tmp_result.reverse()
        else:
            tmp_result = tmp_result + ["%02x" % ord(line[i]) for i in range(0, len(line))]
            tmp_result = tmp_result + ["%02x" % 0 for i in range(0, entry[x_bmc_fru_eeprom_size] - len(line))]

        for i in range(len(tmp_result)):
            g_result[idx+i] = tmp_result[i]

    except Exception as e:
        print (repr(e))
        return -1

    return need_to_update

def get_user_input_fru_data():
    global brief_enable
    idx = 0
    need_to_update = 0
    try:
        # user input
        for entryno in bmc_fru_eeprom_data:
            entry = bmc_fru_eeprom_data[entryno]
            entry[x_bmc_fru_eeprom_need_update] = False
            need_to_update = bmc_set_fru_eeprom_prompt(entryno, entry, idx, need_to_update)
            idx += entry[x_bmc_fru_eeprom_size]

        g_result.pop() # re-calculate crc
        crc8 = "%02x" % calculate_crc8_checksum_of_buffer(g_result)
        g_result.append(crc8)
        print ("CRC is %s\r\n" % crc8)

        # print user input bytes here
        if brief_enable == 0:
            print_result_in_byte()

    except Exception as e:
        print (repr(e))
        return -1
    return need_to_update

def bmc_write_fru_eeprom():
    if bmc_read_fru_eeprom() == 0:
        need_to_update = get_user_input_fru_data()
        if need_to_update:
            print ("PROGRAM EEPROM...")
            bmc_update_fru_eeprom()
    return 0

def main(argv):
    global g_fru_board
    global program_enable
    global brief_enable
    global set_ques_no

    # init
    g_fru_board = 'SMB' # for Wedge100BF-series
    get_board_ee_info()
    calc_eeprom_size()

    # option
    if argv[1] == 'r':
        print ("READ EEPROM on board...\r\n")
        if bmc_read_fru_eeprom() == 0:
            print_result_in_format()
    elif argv[1] == 'w':
        if bmc_write_fru_eeprom() == 0:
            print_result_in_format()
    elif argv[1] == 'p':
        print ("Output a write command...\r\n")
        program_enable = 1
        brief_enable = 1
        if bmc_read_fru_eeprom() == 0:
            print_result_in_format()
    elif argv[1] == 'q':
        brief_enable = 1
        for i in range(2, len(argv)):
            set_ques_no.append(int(argv[i]))
        if bmc_write_fru_eeprom() == 0:
            print_result_in_format()
    else:
        print ("\r\nERROR! Wrong parameters!")
        usage()

if len(sys.argv) >= 2:
    sys.exit(main(sys.argv))
else:
  print ("\r\nERROR! Wrong # of parameters!")
  usage()
