/*
 * com_e_driver.c - The i2c driver for COMe
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

static ssize_t i2c_dev_show_mac(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
  uint8_t values[6];
  int ret_val;

  ret_val = i2c_dev_read_nbytes(dev, attr, values, 6);
  if (ret_val < 0) {
    return ret_val;
  }
  //values[] : mac address
  return scnprintf(buf, PAGE_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x\n", values[0],
                   values[1], values[2], values[3], values[4], values[5]);
}

static ssize_t i2c_dev_show_date(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
  uint8_t values[3];
  int ret_val;

  ret_val = i2c_dev_read_nbytes(dev, attr, values, 3);
  if (ret_val < 0) {
    return ret_val;
  }
  //values[0] : year
  //values[1] : month
  //values[2] : day
  return scnprintf(buf, PAGE_SIZE, "%x/%x/%x\n", values[1],
                   values[2], values[0]);
}

static ssize_t i2c_dev_show_version(struct device *dev,
                                    struct device_attribute *attr,
                                    char *buf)
{
  uint8_t values[3];
  int ret_val;

  ret_val = i2c_dev_read_nbytes(dev, attr, values, 3);
  if (ret_val < 0) {
    return ret_val;
  }
  //values[0] : Version_R
  //values[1] : Version_E
  //values[2] : Version_T
  return scnprintf(buf, PAGE_SIZE, "EC Version R=0x%02x E=0x%02x T=0x%02x\n",
                   values[0], values[1], values[2]);

}

static ssize_t i2c_dev_show_cpu_temp(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  uint8_t temp_value;
  int value;
  int ret_val;

  ret_val = i2c_dev_read_byte(dev, attr);

  if (ret_val < 0) {
    return ret_val;
  }
  temp_value = (uint8_t)(ret_val & 0xff);
  
  // For lm_sensors multiply by 1000
  value = temp_value * 1000;
  return scnprintf(buf, PAGE_SIZE, "%d\n", value);
}

static ssize_t i2c_dev_show_mem_temp(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  int ret_val;

  // For memory temp follow : Module_EC_Function_Manual_050817.pdf
  /*
   * DIMM temperature is encoded in 16b as the following:
   * b15-b12: TCRIT HIGH LOW SIGN
   * b11-b08: 128 64 32 16
   * b07-b04: 8 4 2 1
   * b03-b00: 0.5 0.25 0.125 0.0625
   */
  ret_val = i2c_dev_read_word_bigendian(dev, attr);
  if (ret_val < 0) {
    return ret_val;
  }

  ret_val &= 0x1FFF ;
  if ((ret_val & 0x1000)) {
    /* signed */
    ret_val = -((ret_val - 1) ^ 0x1FFF);
  }
  /*
   * now val holds the value as a number of 1/16, times 1000 for lm-sensors */
  ret_val *= 1000 / 16;
  return scnprintf(buf, PAGE_SIZE, "%d\n", ret_val);
}

static ssize_t i2c_dev_show_voltage0(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  int result;

  result = i2c_dev_read_word_littleendian(dev, attr);
  if (result < 0) {
    return result;
  }
  result = (result * 1000) / 341;
  return scnprintf(buf, PAGE_SIZE, "%u\n", result);
}

static ssize_t i2c_dev_show_voltage1(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  int result;

  result = i2c_dev_read_word_littleendian(dev, attr);
  if (result < 0) {
    return result;
  }
  result = (result * 2000) / 341;
  return scnprintf(buf, PAGE_SIZE, "%u\n", result);
}

static ssize_t i2c_dev_show_voltage2(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  int result;

  result = i2c_dev_read_word_littleendian(dev, attr);
  if (result < 0) {
    return result;
  }
  result = (result * 3200) / 341;
  return scnprintf(buf, PAGE_SIZE, "%u\n", result);
}

static ssize_t i2c_dev_show_voltage3(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  int result;

  result = i2c_dev_read_word_littleendian(dev, attr);
  if (result < 0) {
    return result;
  }
  result = (result * 6600) / 341;
  return scnprintf(buf, PAGE_SIZE, "%u\n", result);
}

#define in0_min (1710)  //CPU Vcore
#define in0_max (1890)
#define in1_min (3000)  //+3V
#define in1_max (3600)
#define in2_min (4500)  //+5V
#define in2_max (5500)
#define in3_min (10800) //+12V
#define in3_max (13200)
#define in4_min (1140)  //VDIMM
#define in4_max (1260)
#define temp1_min (0)   //CPU Temp
#define temp1_max (85000)
#define temp2_min (0)   //Memory Temp
#define temp2_max (85000)

static ssize_t i2c_dev_show_in0_min(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%d\n", in0_min);
}

static ssize_t i2c_dev_show_in0_max(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%d\n", in0_max);
}

static ssize_t i2c_dev_show_in1_min(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%d\n", in1_min);
}

static ssize_t i2c_dev_show_in1_max(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%d\n", in1_max);
}

static ssize_t i2c_dev_show_in2_min(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%d\n", in2_min);
}

static ssize_t i2c_dev_show_in2_max(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%d\n", in2_max);
}

static ssize_t i2c_dev_show_in3_min(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%d\n", in3_min);
}

static ssize_t i2c_dev_show_in3_max(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%d\n", in3_max);
}

static ssize_t i2c_dev_show_in4_min(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%d\n", in4_min);
}

static ssize_t i2c_dev_show_in4_max(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%d\n", in4_max);
}

static ssize_t i2c_dev_show_temp2_min(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%d\n", temp2_min);
}

static ssize_t i2c_dev_show_temp2_max(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%d\n", temp2_max);
}

static ssize_t i2c_dev_show_temp1_min(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%d\n", temp1_min);
}

static ssize_t i2c_dev_show_temp1_max(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%d\n", temp1_max);
}

static const i2c_dev_attr_st com_e_attr_table[] = {
  {
    "temp1_input", // cpu_temp : fand uses this temp
    NULL,
    i2c_dev_show_cpu_temp,
    NULL,
    0x0, 0, 8,
  },
  {
    "temp2_min",
    NULL,
    i2c_dev_show_temp2_min,
    NULL,
    0, 0, 0,
  },
  {
    "temp2_max",
    NULL,
    i2c_dev_show_temp2_max,
    NULL,
    0, 0, 0,
  },
  {
    "temp2_input", // mem_temp
    NULL,
    i2c_dev_show_mem_temp,
    NULL,
    0x04, 0, 16,
  },
  {
    "temp1_min",
    NULL,
    i2c_dev_show_temp1_min,
    NULL,
    0, 0, 0,
  },
  {
    "temp1_max",
    NULL,
    i2c_dev_show_temp1_max,
    NULL,
    0, 0, 0,
  },
  {
    "version", // version_r,_e,_t
    NULL,
    i2c_dev_show_version,
    NULL,
    0x1D, 0, 24,
  },
  {
    "in0_input", // CPU_vcore
    NULL,
    i2c_dev_show_voltage0,
    NULL,
    0x20, 0, 16,
  },
  {
    "in0_min",
    NULL,
    i2c_dev_show_in0_min,
    NULL,
    0, 0, 0,
  },
  {
    "in0_max",
    NULL,
    i2c_dev_show_in0_max,
    NULL,
    0, 0, 0,
  },
  {
    "in1_input",// 3V
    NULL,
    i2c_dev_show_voltage1,
    NULL,
    0x22, 0, 16,
  },
  {
    "in1_min",
    NULL,
    i2c_dev_show_in1_min,
    NULL,
    0, 0, 0,
  },
  {
    "in1_max",
    NULL,
    i2c_dev_show_in1_max,
    NULL,
    0, 0, 0,
  },
  {
    "in2_input",// 5V
    NULL,
    i2c_dev_show_voltage2,
    NULL,
    0x24, 0, 16,
  },
  {
    "in2_min",
    NULL,
    i2c_dev_show_in2_min,
    NULL,
    0, 0, 0,
  },
  {
    "in2_max",
    NULL,
    i2c_dev_show_in2_max,
    NULL,
    0, 0, 0,
  },
  {
    "date",
    NULL,
    i2c_dev_show_date,
    NULL,
    0x2D, 0, 24,
  },
  {
    "in3_input",// 12V
    NULL,
    i2c_dev_show_voltage3,
    NULL,
    0x30, 0, 16,
  },
  {
    "in3_min",
    NULL,
    i2c_dev_show_in3_min,
    NULL,
    0, 0, 0,
  },
  {
    "in3_max",
    NULL,
    i2c_dev_show_in3_max,
    NULL,
    0, 0, 0,
  },
  {
    "in4_input",// VDIMM
    NULL,
    i2c_dev_show_voltage0,
    NULL,
    0x32, 0, 16,
  },
  {
    "in4_min",
    NULL,
    i2c_dev_show_in4_min,
    NULL,
    0, 0, 0,
  },
  {
    "in4_max",
    NULL,
    i2c_dev_show_in4_max,
    NULL,
    0, 0, 0,
  },
  {
    "product_name",
    NULL,
    i2c_dev_show_ascii,
    NULL,
    0x3C, 0, 32,
  },
  {
    "customize_name",
    NULL,
    i2c_dev_show_ascii,
    NULL,
    0x4D, 0, 24,
  },
  {
    "mac",
    NULL,
    i2c_dev_show_mac,
    NULL,
    0x50, 0, 48,
  },
  {
    "serial_number",
    NULL,
    i2c_dev_show_ascii,
    NULL,
    0x60, 0, 256,
  },
};

static i2c_dev_data_st com_e_data;

/*
 * COMe i2c addresses.
 * normal_i2c is used in I2C_CLIENT_INSMOD_1()
 */
static const unsigned short normal_i2c[] = {
  0x33, I2C_CLIENT_END
};

/*
 * Insmod parameters
 */
I2C_CLIENT_INSMOD_1(com_e);

/* COMe id */
static const struct i2c_device_id com_e_id[] = {
  { "com_e_driver", com_e_id },
  { },
};
MODULE_DEVICE_TABLE(i2c, com_e_id);

/* Return 0 if detection is successful, -ENODEV otherwise */
static int com_e_detect(struct i2c_client *client, int kind,
                          struct i2c_board_info *info)
{
  /*
   * We don't currently do any detection of the COMe
   */
  strlcpy(info->type, "com_e_driver", I2C_NAME_SIZE);
  return 0;
}

static int com_e_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
  int n_attrs = sizeof(com_e_attr_table) / sizeof(com_e_attr_table[0]);
  return i2c_dev_sysfs_data_init(client, &com_e_data,
                                 com_e_attr_table, n_attrs);
}

static int com_e_remove(struct i2c_client *client)
{
  i2c_dev_sysfs_data_clean(client, &com_e_data);
  return 0;
}

static struct i2c_driver com_e_driver = {
  .class    = I2C_CLASS_HWMON,
  .driver = {
    .name = "com_e_driver",
  },
  .probe    = com_e_probe,
  .remove   = com_e_remove,
  .id_table = com_e_id,
  .detect   = com_e_detect,
  /* addr_data is defined through I2C_CLIENT_INSMOD_1() */
  .address_data = &addr_data,
};

static int __init com_e_mod_init(void)
{
  return i2c_add_driver(&com_e_driver);
}

static void __exit com_e_mod_exit(void)
{
  i2c_del_driver(&com_e_driver);
}

MODULE_AUTHOR("Vineela Kukkadapu <vineelak@fb.com>");
MODULE_DESCRIPTION("COM_E Driver");
MODULE_LICENSE("GPL");

module_init(com_e_mod_init);
module_exit(com_e_mod_exit);
