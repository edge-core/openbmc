/*
 * fancpld.c - The i2c driver for FANBOARDCPLD
 *
 * Copyright 2015-present Facebook. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

//#define DEBUG

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <i2c_dev_sysfs.h>

#ifdef DEBUG

#define PP_DEBUG(fmt, ...) do {                   \
  printk(KERN_DEBUG "%s:%d " fmt "\n",            \
         __FUNCTION__, __LINE__, ##__VA_ARGS__);  \
} while (0)

#else /* !DEBUG */

#define PP_DEBUG(fmt, ...)

#endif

static ssize_t fancpld_fan_rpm_show(struct device *dev,
                                    struct device_attribute *attr,
                                    char *buf)
{
  int val;

  val = i2c_dev_read_byte(dev, attr);
  if (val < 0) {
    return val;
  }
  /* Multiply by 150 to get the RPM */
  val *= 150;

  return scnprintf(buf, PAGE_SIZE, "%u\n", val);
}


static ssize_t fancpld_fan_rpm_show_newport(struct device *dev,
                                    struct device_attribute *attr,
                                    char *buf)
{
  int val;

  val = i2c_dev_read_byte(dev, attr);
  if (val < 0) {
    return val;
  }
  /* Multiply by 100 to get the RPM */
  val *= 100;

  return scnprintf(buf, PAGE_SIZE, "%u\n", val);
}


static ssize_t show_fan_min_value(struct device *dev,
                                    struct device_attribute *attr,
                                    char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%u\n", 0);
}

/* Convert the percentage to our 1/32th unit (0-31). */
#define FANTRAY_PWM_HELP                        \
  "each value represents 1/32 duty cycle"
/*For Newport, Convert the percentage to our 1/16th unit (0-15). */
#define FANTRAY_PWM_HELP_NEWPORT                \
	  "each value represents 1/16 duty cycle"
#define FANTRAY_LED_CTRL_HELP                   \
  "0x0: Under HW control\n"                     \
  "0x1: Red off, Blue on\n"                     \
  "0x2: Red on, Blue off\n"                     \
  "0x3: Red off, Blue off"
#define FANTRAY_LED_BLINK_HELP                  \
  "0: no blink\n"                               \
  "1: blink"

static const i2c_dev_attr_st fancpld_attr_table[] = {
  {
    "board_rev",
    NULL,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    0, 0, 4,
  },
  {
    "model_id",
    "0x0: wedge100\n"
    "0x1: 6-pack100 linecard\n"
    "0x2: 6-pack100 fabric card\n"
    "0x3: reserved",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    0, 4, 2,
  },
  {
    "cpld_rev",
    NULL,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    1, 0, 6,
  },
  {
    "cpld_released",
    NULL,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    1, 6, 1,
  },
  {
    "cpld_sub_rev",
    NULL,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    2, 0, 8,
  },
  {
    "slotid",
    NULL,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    3, 0, 5,
  },
  {
    "jaybox_gpio",
    NULL,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    4, 0, 8,
  },
  {
    "jaybox_status",
    NULL,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    5, 0, 2,
  },
  {
    "fantray_failure",
    "bit value 0: fan tray has failure\n"
    "bit value 1: fan try is good and alive",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    9, 0, 5,
  },
  {
    "fan1_input",
    NULL,
    fancpld_fan_rpm_show,
    NULL,
    0x10, 0, 8,
  },
  {
    "fan2_input",
    NULL,
    fancpld_fan_rpm_show,
    NULL,
    0x11, 0, 8,
  },
  {
    "fan3_input",
    NULL,
    fancpld_fan_rpm_show,
    NULL,
    0x12, 0, 8,
  },
  {
    "fan4_input",
    NULL,
    fancpld_fan_rpm_show,
    NULL,
    0x13, 0, 8,
  },
  {
    "fan5_input",
    NULL,
    fancpld_fan_rpm_show,
    NULL,
    0x14, 0, 8,
  },
  {
    "fan6_input",
    NULL,
    fancpld_fan_rpm_show,
    NULL,
    0x15, 0, 8,
  },
  {
    "fan7_input",
    NULL,
    fancpld_fan_rpm_show,
    NULL,
    0x16, 0, 8,
  },
  {
    "fan8_input",
    NULL,
    fancpld_fan_rpm_show,
    NULL,
    0x17, 0, 8,
  },
  {
    "fan9_input",
    NULL,
    fancpld_fan_rpm_show,
    NULL,
    0x18, 0, 8,
  },
  {
    "fan10_input",
    NULL,
    fancpld_fan_rpm_show,
    NULL,
    0x19, 0, 8,
  },
  {
    "fantray_present",
    "0: present\n"
    "1: not present",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    0x1d, 0, 5,
  },
  {
    "fantray1_pwm",
    FANTRAY_PWM_HELP,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x20, 0, 5,
  },
  {
    "fantray2_pwm",
    FANTRAY_PWM_HELP,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x21, 0, 5,
  },
  {
    "fantray3_pwm",
    FANTRAY_PWM_HELP,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x22, 0, 5,
  },
  {
    "fantray4_pwm",
    FANTRAY_PWM_HELP,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x23, 0, 5,
  },
  {
    "fantray5_pwm",
    FANTRAY_PWM_HELP,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x24, 0, 5,
  },
  {
    "fantray1_led_ctrl",
    FANTRAY_LED_CTRL_HELP,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x25, 0, 2,
  },
  {
    "fantray1_led_blink",
    FANTRAY_LED_BLINK_HELP,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x25, 2, 1,
  },
  {
    "fantray2_led_ctrl",
    FANTRAY_LED_CTRL_HELP,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x25, 4, 2,
  },
  {
    "fantray2_led_blink",
    FANTRAY_LED_BLINK_HELP,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x25, 6, 1,
  },
  {
    "fantray3_led_ctrl",
    FANTRAY_LED_CTRL_HELP,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x26, 0, 2,
  },
  {
    "fantray3_led_blink",
    FANTRAY_LED_BLINK_HELP,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x26, 2, 1,
  },
  {
    "fantray4_led_ctrl",
    FANTRAY_LED_CTRL_HELP,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x26, 4, 2,
  },
  {
    "fantray4_led_blink",
    FANTRAY_LED_BLINK_HELP,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x26, 6, 1,
  },
  {
    "fantray5_led_ctrl",
    FANTRAY_LED_CTRL_HELP,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x27, 0, 2,
  },
  {
    "fantray5_led_blink",
    FANTRAY_LED_BLINK_HELP,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x27, 2, 1,
  },
  {
    "fan1_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan2_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan3_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan4_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan5_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan6_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan7_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan8_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan9_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan10_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
};

/* table for both newport, stinson and davenport */
static const i2c_dev_attr_st fancpld_attr_table_newport[] = {
  {
    "board_rev",
    "100: R0A\n"
    "101: R0B\n"
    "110: R01\n"
    "Others: Reserved",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    0, 5, 3,    //Offset 0x00, [7:5] Version_ID - Read only
  },
  {
    "model_id",
    "00: Reserved\n"
    "01: Reserved\n"
    "10: Reserved\n"
    "11: ZZ project",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    0, 0, 2,    //Offset 0x00, [1:0] Board_ID - Read only
  },
  {
    "cpld_rev",
    NULL,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    1, 0, 8,    //Offset 0x01, [7:0] CPLD_ver - Read only
  },
  {
    "cpld_reset",
    "1: CPLD is placed in normal operation state.\n"
    "0: CPLD is placed in reset state.",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    4, 7, 1,    //Offset 0x04, [7] Reset_CPLD - Read & Write
  },
  {
    "alarm_intb_cpu",
    "1: No interrupt\n"
    "0: There is INTR to CPU",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    5, 7, 1,    //Offset 0x05, [7] Alarm_INTB_CPU - Read only
  },
  {
    "alarm_intb_cpu_mask",
    "1: CPLD blocks incoming the interrupt\n"
    "0: CPLD passes the interrupt to CPU",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    6, 7, 1,    //Offset 0x06, [7] MASK*Fan_interrupt - Read & Write
  },
  {
    "fantray1_present",
    "0: present\n"
    "1: not present",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    0x0f, 0, 1,    //Offset 0x0F, [5]~[0] Fan_presentN - Read only
  },
  {
    "fantray2_present",
    "0: present\n"
    "1: not present",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    0x0f, 1, 1,
  },
  {
    "fantray3_present",
    "0: present\n"
    "1: not present",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    0x0f, 2, 1,
  },
  {
    "fantray4_present",
    "0: present\n"
    "1: not present",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    0x0f, 3, 1,
  },
  {
    "fantray5_present",
    "0: present\n"
    "1: not present",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    0x0f, 4, 1,
  },
  {
    "fantray6_present",
    "0: present\n"
    "1: not present",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    0x0f, 5, 1,
  },
  /* fan7 is only stinson specific */
  {
    "fantray7_present",
    "0: present\n"
    "1: not present",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    NULL,
    0x0f, 6, 1,
  },
  {
    "fantray_pwm",
    FANTRAY_PWM_HELP_NEWPORT,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x11, 0, 4,    //Offset 0x11, [3:0] Fan PWM - Read & Write
  },
  {
    "fan1_input",
    NULL,
    fancpld_fan_rpm_show_newport,
    NULL,
    0x12, 0, 8,    //Offset 0x12-0x17, [7:0] Front_Fan1_Tach - Read only
  },
  {
    "fan2_input",
    NULL,
    fancpld_fan_rpm_show_newport,
    NULL,
    0x22, 0, 8,    //Offset 0x22-0x27, [7:0] Rear_Fan6_Tach - Read only
  },
  {
    "fan3_input",
    NULL,
    fancpld_fan_rpm_show_newport,
    NULL,
    0x13, 0, 8,
  },
  {
    "fan4_input",
    NULL,
    fancpld_fan_rpm_show_newport,
    NULL,
    0x23, 0, 8,
  },
  {
    "fan5_input",
    NULL,
    fancpld_fan_rpm_show_newport,
    NULL,
    0x14, 0, 8,
  },
  {
    "fan6_input",
    NULL,
    fancpld_fan_rpm_show_newport,
    NULL,
    0x24, 0, 8,
  },
  {
    "fan7_input",
    NULL,
    fancpld_fan_rpm_show_newport,
    NULL,
    0x15, 0, 8,
  },
  {
    "fan8_input",
    NULL,
    fancpld_fan_rpm_show_newport,
    NULL,
    0x25, 0, 8,
  },
  {
    "fan9_input",
    NULL,
    fancpld_fan_rpm_show_newport,
    NULL,
    0x16, 0, 8,
  },
  {
    "fan10_input",
    NULL,
    fancpld_fan_rpm_show_newport,
    NULL,
    0x26, 0, 8,
  },
  {
    "fan11_input",
    NULL,
    fancpld_fan_rpm_show_newport,
    NULL,
    0x17, 0, 8,
  },
  {
    "fan12_input",
    NULL,
    fancpld_fan_rpm_show_newport,
    NULL,
    0x27, 0, 8,
  },
  /* fan13 fan14 _input is stinson specific only */
  {
    "fan13_input",
    NULL,
    fancpld_fan_rpm_show_newport,
    NULL,
    0x18, 0, 8,
  },
  {
    "fan14_input",
    NULL,
    fancpld_fan_rpm_show_newport,
    NULL,
    0x28, 0, 8,
  },
  {
    "fantray1_led_r",
    "0: Red\n"
    "1: off",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x1C, 7, 1,    //Offset 0x1C, [7]~[0] LED_FanN_R & LED_FanN_G - Read & Write
  },
  {
    "fantray1_led_g",
    "0: Green\n"
    "1: off",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x1C, 6, 1,
  },
  {
    "fantray2_led_r",
    "0: Red\n"
    "1: off",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x1C, 5, 1,
  },
  {
    "fantray2_led_g",
    "0: Green\n"
    "1: off",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x1C, 4, 1,
  },
  {
    "fantray3_led_r",
    "0: Red\n"
    "1: off",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x1C, 3, 1,
  },
  {
    "fantray3_led_g",
    "0: Green\n"
    "1: off",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x1C, 2, 1,
  },
  {
    "fantray4_led_r",
    "0: Red\n"
    "1: off",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x1C, 1, 1,
  },
  {
    "fantray4_led_g",
    "0: Green\n"
    "1: off",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x1C, 0, 1,
  },
  {
    "fantray5_led_r",
    "0: Red\n"
    "1: off",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x1D, 3, 1,    //Offset 0x1D, [3]~[0] Fan_Power N - Read & Write
  },
  {
    "fantray5_led_g",
    "0: Green\n"
    "1: off",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x1D, 2, 1,
  },
  {
    "fantray6_led_r",
    "0: Red\n"
    "1: off",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x1D, 1, 1,
  },
  {
    "fantray6_led_g",
    "0: Green\n"
    "1: off",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x1D, 0, 1,
  },
  /* fantray_7 is stinson specific only */
  {
    "fantray7_led_r",
    "0: Red\n"
    "1: off",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x1D, 5, 1,
  },
  {
    "fantray7_led_g",
    "0: Green\n"
    "1: off",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x1D, 4, 1,
  },
  {
    "fan1_power",
    "1: Enable\n"
    "0: Disable",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x30, 0, 1,    //Offset 0x30, [5]~[0] Fan_PowerN - Read & Write
  },
  {
    "fan2_power",
    "1: Enable\n"
    "0: Disable",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x30, 1, 1,
  },
  {
    "fan3_power",
    "1: Enable\n"
    "0: Disable",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x30, 2, 1,
  },
  {
    "fan4_power",
    "1: Enable\n"
    "0: Disable",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x30, 3, 1,
  },
  {
    "fan5_power",
    "1: Enable\n"
    "0: Disable",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x30, 4, 1,
  },
  {
    "fan6_power",
    "1: Enable\n"
    "0: Disable",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x30, 5, 1,
  },
  /* fan7 is stinson specific only */
  {
    "fan7_power",
    "1: Enable\n"
    "0: Disable",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x30, 6, 1,
  },
  {
    "watchdog_timer",
    NULL,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x31, 0, 8,    //Offset 0x31, [7:0] Watchdog timer - Read & Write
  },
  {
    "watchdog_max_pwm_value",
    NULL,
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x32, 0, 4,    //Offset 0x32, [3:0] Watchdog Maximum PWM value - Read & Write
  },
  {
    "watchdog_disable",
    "1: Enable\n"
    "0: Disable",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x33, 0, 1,    //Offset 0x33, [0] Watchdog disable - Read & Write
  },
  {
    "led_debug_mode",
    "1: on\n"
    "0: off",
    I2C_DEV_ATTR_SHOW_DEFAULT,
    I2C_DEV_ATTR_STORE_DEFAULT,
    0x42, 7, 1,    //Offset 0x42, [7] LED_Debug_mode - Read & Write
  },
  {
    "fan1_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan2_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan3_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan4_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan5_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan6_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan7_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan8_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan9_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan10_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan11_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
  {
    "fan12_min",
    NULL,
    show_fan_min_value,
    NULL,
    0, 0, 0,
  },
};

static i2c_dev_data_st fancpld_data;

/*
 * FANCPLD i2c addresses.
 * normal_i2c is used in I2C_CLIENT_INSMOD_1()
 */
static const unsigned short normal_i2c[] = {
  0x33, I2C_CLIENT_END
};

/*
 * Insmod parameters
 */
I2C_CLIENT_INSMOD_1(fancpld);

/* FANCPLD id */
static const struct i2c_device_id fancpld_id[] = {
  { "fancpld", fancpld },
  { },
};
MODULE_DEVICE_TABLE(i2c, fancpld_id);

/* Return 0 if detection is successful, -ENODEV otherwise */
static int fancpld_detect(struct i2c_client *client, int kind,
                          struct i2c_board_info *info)
{
  /*
   * We don't currently do any detection of the FANCPLD
   */
  strlcpy(info->type, "fancpld", I2C_NAME_SIZE);
  return 0;
}

extern s32 i2c_smbus_read_byte(struct i2c_client *client);
static int fancpld_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
  int n_attrs;

  if (i2c_smbus_read_byte(client) >= 0) {
    if(client->addr == 0x33) {
      n_attrs = sizeof(fancpld_attr_table) / sizeof(fancpld_attr_table[0]);
      return i2c_dev_sysfs_data_init(client, &fancpld_data, fancpld_attr_table, n_attrs);
    }
    /*For Newport*/
	else if(client->addr == 0x66) {
      n_attrs = sizeof(fancpld_attr_table_newport) / sizeof(fancpld_attr_table_newport[0]);
      return i2c_dev_sysfs_data_init(client, &fancpld_data, fancpld_attr_table_newport, n_attrs);
    }
  }

  return 0;
}

static int fancpld_remove(struct i2c_client *client)
{
  i2c_dev_sysfs_data_clean(client, &fancpld_data);
  return 0;
}

static struct i2c_driver fancpld_driver = {
  .class    = I2C_CLASS_HWMON,
  .driver = {
    .name = "fancpld",
  },
  .probe    = fancpld_probe,
  .remove   = fancpld_remove,
  .id_table = fancpld_id,
  .detect   = fancpld_detect,
  /* addr_data is defined through I2C_CLIENT_INSMOD_1() */
  .address_data = &addr_data,
};

static int __init fancpld_mod_init(void)
{
  return i2c_add_driver(&fancpld_driver);
}

static void __exit fancpld_mod_exit(void)
{
  i2c_del_driver(&fancpld_driver);
}

MODULE_AUTHOR("Tian Fang <tfang@fb.com>");
MODULE_DESCRIPTION("FANCPLD Driver");
MODULE_LICENSE("GPL");

module_init(fancpld_mod_init);
module_exit(fancpld_mod_exit);
