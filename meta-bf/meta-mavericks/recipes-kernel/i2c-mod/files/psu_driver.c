/*
 * psu_driver.c - The i2c driver to get following PSU information:
 * 
 * PFE1100-12-054NA, PFE1500-12-054NAC, DDM1500BH12AXF
 *
 * Copyright 2018-present Facebook. All Rights Reserved.
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
#include <linux/delay.h>

#include <i2c_dev_sysfs.h>

#ifdef DEBUG
#define PSU_DEBUG(fmt, ...) do {                   \
  printk(KERN_DEBUG "%s:%d " fmt "\n",            \
         __FUNCTION__, __LINE__, ##__VA_ARGS__);  \
} while (0)

#else /* !DEBUG */

#define PSU_DEBUG(fmt, ...)
#endif

#define POW2(x) (1 << (x))

#define PSU_DELAY 15
#define PMBUS_MFR_MODEL  0x9a
#define PMBUS_POWER 0x96
typedef enum {
  DELTA_1500 = 0,
  BELPOWER_600_NA,
  BELPOWER_1100_NA,
  BELPOWER_1100_ND,
  BELPOWER_1100_NAS,
  BELPOWER_1500_NAC,
  MURATA_1500,
  LITEON_1500,
  UNKNOWN
} model_name;

model_name model = UNKNOWN;

enum {
  LINEAR_11,
  LINEAR_16
};

/*
 * PMBus Linear-11 Data Format
 * X = Y*2^N
 * X is the "real world" value;
 * Y is an 11 bit, two's complement integer;
 * N is a 5 bit, two's complement integer.
 *
 * PMBus Linear-16 Data Format
 * X = Y*2^N
 * X is the "real world" value;
 * Y is a 16 bit unsigned binary integer;
 * N is a 5 bit, two's complement integer.
 */
static int linear_convert(int type, int value, int n)
{
  int msb_y, msb_n, data_y, data_n;
  int value_x = 0;

  if (type == LINEAR_11) {
    msb_y = (value >> 10) & 0x1;
    data_y = msb_y ? -1 * ((~value & 0x3ff) + 1)
                   : value & 0x3ff;

    if (n != 0) {
      value_x = (n < 0) ? (data_y * 1000) / POW2(abs(n))
                        : (data_y * 1000) * POW2(n);
    } else {
      msb_n = (value >> 15) & 0x1;

      if (msb_n) {
        data_n = (~(value >> 11) & 0xf) + 1;
        value_x = (data_y * 1000) / POW2(data_n);
      } else {
        data_n = ((value >> 11) & 0xf);
        value_x = (data_y * 1000) * POW2(data_n);
      }
    }
  } else {
    if (n != 0) {
      value_x = (n < 0) ? (value * 1000) / POW2(abs(n))
                        : (value * 1000) * POW2(n);
    }
  }

  return value_x;
}

#ifdef DEBUG_TIME
static inline void print_time(char *s)
{
	struct timeval tv;
	do_gettimeofday(&tv);
	printk("%s: %d.%d\n", s,(int)tv.tv_sec, (int)tv.tv_usec);
}
#endif

static int result_linear_convert(int value)
{
  int result = value;
  switch (model) {
    case DELTA_1500:
    case LITEON_1500:
      result = linear_convert(LINEAR_11, result, 0);
      break;
    case BELPOWER_600_NA:
    case BELPOWER_1100_NA:
    case BELPOWER_1500_NAC:
    case BELPOWER_1100_NAS:
    case BELPOWER_1100_ND:
    case MURATA_1500:
      result = linear_convert(LINEAR_11, result, 1);
      break;
    default:
      break;
  }
  return result;
}

static int psu_convert(struct device *dev, struct device_attribute *attr)
{
  struct i2c_client *client = to_i2c_client(dev);
  i2c_dev_data_st *data = i2c_get_clientdata(client);
  i2c_sysfs_attr_st *i2c_attr = TO_I2C_SYSFS_ATTR(attr);
  const i2c_dev_attr_st *dev_attr = i2c_attr->isa_i2c_attr;
  int value = -1;
  int count;
  int ret = -1;
  u8 length, model_chr;
  uint8_t block_buffer[I2C_SMBUS_BLOCK_MAX + 1] = {0};

  mutex_lock(&data->idd_lock);

  /*
   * If read block length byte > 32, it will cause kernel panic.
   * Using read word to replace read block to identifer PSU model.
   */
  count=10;
  while((ret < 0 || length > 32) && count--) {
    ret = i2c_smbus_read_block_data(client, PMBUS_MFR_MODEL, block_buffer);
    length = ret & 0xff;
    mdelay(10);
  }
  //PSU_DEBUG("1st char+length = %d + %d\n", ((ret >> 8) & 0xff), (ret & 0xff));
  if (ret < 0 || length > 32) {
    PSU_DEBUG("Failed to read Manufacturer Model\n");
  } else {
    count=10;
    while((value < 0 || value == 0xffff) && count--) {
      value = i2c_smbus_read_word_data(client, PMBUS_POWER);
    }
  }

  mutex_unlock(&data->idd_lock);
  //printk("%d\n", value);
  if (value < 0 || value == 0xffff) {
    /* error case */
    PSU_DEBUG("I2C read error, value: %d\n", value);
    return -1;
  }

  if (strncmp(block_buffer, "PFE600-12-054NA", 15)== 0)
  {
      /* PSU model name: PFE600-12-054NA */
      model = BELPOWER_600_NA;
  }
  else if (strncmp(block_buffer, "PFE1100-12-054NA", 16)== 0)
  {
      /* PSU model name: PFE1100-12-054NA */
      model = BELPOWER_1100_NA;
  }
  else if (strncmp(block_buffer, "PFE1100-12-054ND", 16)== 0)
  {
      /* PSU model name: PFE1100-12-054NA */
      model = BELPOWER_1100_ND;
  }
  else if (strncmp(block_buffer, "PFE1100-12-NAS435", 17)== 0)
  {
      /* PSU model name: PFE1100-12-NAS435 */
      model = BELPOWER_1100_NAS;
  }
  else if (strncmp(block_buffer, "ECDD1500120", 11)== 0)
  {
      /* PSU model name: ECDD1500120 */
      model = DELTA_1500;
  }
  else if (strncmp(block_buffer, "PS-2152-5L", 10)== 0)
  {
      /* PSU model name: PS-2152-5L */
      model = LITEON_1500;
  }
  else if (strncmp(block_buffer, "PFE1500-12-054NACS457", 21)== 0 ||
           strncmp(block_buffer, "PFE1500-12-054NACS439", 21)== 0)
  {
      /* PSU model name: PFE1500-12-054NACS457 */
      /* PSU model name: PFE1500-12-054NACS439 for newport*/
      model = BELPOWER_1500_NAC;
  }
  else if (strncmp(block_buffer, "D1U54P-W-1500-12-HC4TC-AF", 25)== 0)
  {
      /* PSU model name: D1U54P-W-1500-12-HC4TC-AF */
      model = MURATA_1500;
  }
  else
  {
      model = UNKNOWN;
  }

  if(result_linear_convert(value) > 0) {
	  value = -1;
	  count=10;
	  mutex_lock(&data->idd_lock);
	  while((value < 0 || value == 0xffff) && count--) {
		value = i2c_smbus_read_word_data(client, (dev_attr->ida_reg));
		mdelay(10);
	  }
	  mutex_unlock(&data->idd_lock);
	  if (value < 0 || value == 0xffff) {
		/* error case */
		PSU_DEBUG("I2C read error, value: %d\n", value);
		return -1;
	  }

  }

  PSU_DEBUG("reg:[0x%x] value:[0x%x] [%d]\n", dev_attr->ida_reg, value, value);
  return value;
}

static int psu_convert_model(struct device *dev, struct device_attribute *attr)
{
  struct i2c_client *client = to_i2c_client(dev);
  i2c_dev_data_st *data = i2c_get_clientdata(client);
  i2c_sysfs_attr_st *i2c_attr = TO_I2C_SYSFS_ATTR(attr);
  const i2c_dev_attr_st *dev_attr = i2c_attr->isa_i2c_attr;
  int count = 10;
  int ret = -1;
  u8 length, model_chr;
  uint8_t block_buffer[I2C_SMBUS_BLOCK_MAX + 1] = {0};

  mutex_lock(&data->idd_lock);
  /*
   * If read block length byte > 32, it will cause kernel panic.
   * Using read word to replace read block to identifer PSU model.
   */

  while((ret < 0 || length > 32) && count--) {
    ret = i2c_smbus_read_block_data(client, PMBUS_MFR_MODEL, block_buffer);
    length = ret & 0xff;
    mdelay(10);
  }
  //PSU_DEBUG("1st char+length = %d + %d\n", ((ret >> 8) & 0xff), (ret & 0xff));
  mutex_unlock(&data->idd_lock);
  if (ret < 0 || length > 32) {
    PSU_DEBUG("Failed to read Manufacturer Model\n");
	return -1;
  }

  if (strncmp(block_buffer, "PFE600-12-054NA", 15)== 0)
  {
      /* PSU model name: PFE600-12-054NA */
      model = BELPOWER_600_NA;
  }
  else if (strncmp(block_buffer, "PFE1100-12-054NA", 16)== 0)
  {
      /* PSU model name: PFE1100-12-054NA */
      model = BELPOWER_1100_NA;
  }
  else if (strncmp(block_buffer, "PFE1100-12-054ND", 16)== 0)
  {
      /* PSU model name: PFE1100-12-054NA */
      model = BELPOWER_1100_ND;
  }
  else if (strncmp(block_buffer, "PFE1100-12-NAS435", 17)== 0)
  {
      /* PSU model name: PFE1100-12-NAS435 */
      model = BELPOWER_1100_NAS;
  }
  else if (strncmp(block_buffer, "ECDD1500120", 11)== 0)
  {
      /* PSU model name: ECDD1500120 */
      model = DELTA_1500;
  }
  else if (strncmp(block_buffer, "PS-2152-5L", 10)== 0)
  {
      /* PSU model name: PS-2152-5L */
      model = LITEON_1500;
  }
  else if (strncmp(block_buffer, "PFE1500-12-054NACS457", 21)== 0 ||
           strncmp(block_buffer, "PFE1500-12-054NACS439", 21)== 0)
  {
      /* PSU model name: PFE1500-12-054NACS457 */
      /* PSU model name: PFE1500-12-054NACS439 for newport*/
      model = BELPOWER_1500_NAC;
  }
  else if (strncmp(block_buffer, "D1U54P-W-1500-12-HC4TC-AF", 25)== 0)
  {
      /* PSU model name: D1U54P-W-1500-12-HC4TC-AF */
      model = MURATA_1500;
  }
  else
  {
      model = UNKNOWN;
  }

  return 0;
}

static int psu_update_device(struct device *dev, struct device_attribute *attr)
{
    struct i2c_client *client = to_i2c_client(dev);
    i2c_dev_data_st *data = i2c_get_clientdata(client);
    i2c_sysfs_attr_st *i2c_attr = TO_I2C_SYSFS_ATTR(attr);
    const i2c_dev_attr_st *dev_attr = i2c_attr->isa_i2c_attr;
    int value = -1;
    int count = 10;

    mutex_lock(&data->idd_lock);
    while((value < 0 || value == 0xffff) && count--)
    {
        value = i2c_smbus_read_word_data(client, (dev_attr->ida_reg));
        mdelay(10);
    }
    mutex_unlock(&data->idd_lock);

    if ((value < 0) || (value == 0xffff))
    {
        /* error case */
        PSU_DEBUG("I2C read error, value: %d\n", value);
        return -1;
    }

    return value;
}

static ssize_t psu_vin_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
  int result = psu_convert(dev, attr);

  if (result < 0) {
    /* error case */
    return -EINVAL;
  }

  switch (model) {
    case DELTA_1500:
    case LITEON_1500:
      result = linear_convert(LINEAR_11, result, 0);
      break;
    case BELPOWER_600_NA:
    case BELPOWER_1100_NA:
    case BELPOWER_1100_NAS:
    case BELPOWER_1100_ND:
    case BELPOWER_1500_NAC:
    case MURATA_1500:
      result = linear_convert(LINEAR_11, result, -1);
      break;
    default:
      break;
  }

  return scnprintf(buf, PAGE_SIZE, "%d\n", result);
}

static ssize_t psu_iin_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
  int result = psu_convert(dev, attr);

  if (result < 0) {
    /* error case */
    return -EINVAL;
  }

  switch (model) {
    case DELTA_1500:
    case LITEON_1500:
      result = linear_convert(LINEAR_11, result, 0);
      break;
    case BELPOWER_600_NA:
    case BELPOWER_1100_NA:
    case BELPOWER_1500_NAC:
      result = linear_convert(LINEAR_11, result, -6);
      break;
    case BELPOWER_1100_NAS:
      result = linear_convert(LINEAR_11, result, -6);
      break;
    case BELPOWER_1100_ND:
      result = linear_convert(LINEAR_11, result, -5);
      break;
    case MURATA_1500:
      result = linear_convert(LINEAR_11, result, -5);
      break;
    default:
      break;
  }

  return scnprintf(buf, PAGE_SIZE, "%d\n", result);
}

static ssize_t psu_vout_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
  int result = psu_convert(dev, attr);

  if (result < 0) {
    /* error case */
    return -EINVAL;
  }

  switch (model) {
    case DELTA_1500:
      result = linear_convert(LINEAR_11, result, 0);
      break;
    case LITEON_1500:
      result = linear_convert(LINEAR_16, result, -9);
      break;
    case BELPOWER_600_NA:
    case BELPOWER_1100_NA:
    case BELPOWER_1100_NAS:
    case BELPOWER_1100_ND:
    case BELPOWER_1500_NAC:
    case MURATA_1500:
      result = linear_convert(LINEAR_11, result, -6);
      break;
    default:
    break;
  }


  return scnprintf(buf, PAGE_SIZE, "%d\n", result);
}

static ssize_t psu_iout_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
  int result = psu_convert(dev, attr);

  if (result < 0) {
    /* error case */
    return -EINVAL;
  }

  switch (model) {
    case DELTA_1500:
    case LITEON_1500:
      result = linear_convert(LINEAR_11, result, 0);
      break;
    case BELPOWER_600_NA:
    case BELPOWER_1100_NAS:
    case BELPOWER_1100_ND:
    case BELPOWER_1100_NA:
      result = linear_convert(LINEAR_11, result, -3);
      break;
    case BELPOWER_1500_NAC:
    case MURATA_1500:
      result = linear_convert(LINEAR_11, result, -2);
      break;
    default:
      break;
  }

  return scnprintf(buf, PAGE_SIZE, "%d\n", result);
}

static ssize_t psu_temp_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
  i2c_sysfs_attr_st *i2c_attr = TO_I2C_SYSFS_ATTR(attr);
  const i2c_dev_attr_st *dev_attr = i2c_attr->isa_i2c_attr;
  
  int result = psu_convert(dev, attr);

  if (result < 0) {
    /* error case */
    return -EINVAL;
  }

  switch (model) {
    case DELTA_1500:
    case LITEON_1500:
      result = linear_convert(LINEAR_11, result, 0);
      break;
    case BELPOWER_600_NA:
    case BELPOWER_1100_NA:
    case BELPOWER_1100_NAS:
    case BELPOWER_1100_ND:
    case BELPOWER_1500_NAC:
      result = linear_convert(LINEAR_11, result, -3);
      break;
    case MURATA_1500:
      result = linear_convert(LINEAR_11, result, 0);
      break;
    default:
      break;
  }
  
  if(strcmp(dev_attr->ida_name,"temp3_input") == 0 && (model == BELPOWER_600_NA 
  || model == BELPOWER_1100_NA || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND)){
      return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
  }else{
      return scnprintf(buf, PAGE_SIZE, "%d\n", result);
  }
}

static ssize_t psu_fan_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
  int result = psu_convert(dev, attr);

  if (result < 0) {
    /* error case */
    return -EINVAL;
  }

  switch (model) {
    case DELTA_1500:
    case LITEON_1500:
      result = linear_convert(LINEAR_11, result, 0) / 1000;
      break;
    case BELPOWER_600_NA:
    case BELPOWER_1100_NA:
    case BELPOWER_1100_NAS:
    case BELPOWER_1100_ND:
    case BELPOWER_1500_NAC:
    case MURATA_1500:
      result = linear_convert(LINEAR_11, result, 5) / 1000;
      break;
    default:
      break;
  }

  return scnprintf(buf, PAGE_SIZE, "%d\n", result);
}


static ssize_t psu_fan_status_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
  int result = -1;
  u8 length = 33;
  int count = 10;
  uint8_t values = 0xff;
  
  //if(UNKNOWN == model) {
    result = psu_convert_model(dev, attr);
    if (result < 0) {
       /* error case */
       return -EINVAL;
    }
  //}

  switch (model) {
    case DELTA_1500:
    case BELPOWER_600_NA:
    case BELPOWER_1100_NA:
    case BELPOWER_1100_NAS:
    case BELPOWER_1100_ND:
    case BELPOWER_1500_NAC:
      while(((result < 0) || (values == 0xff)) && count--)
      {
          result = i2c_dev_read_nbytes(dev, attr, &values, 1);
          mdelay(10);
      }
      if ((result < 0) || (values == 0xff))
          return -EINVAL;

      break;

    default:
      break;
  }
  return scnprintf(buf, PAGE_SIZE, "%d\n", values);
}

static ssize_t psu_power_show(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
  int result = psu_convert(dev, attr);

  if (result < 0) {
    /* error case */
    return -EINVAL;
  }

  switch (model) {
    case DELTA_1500:
    case LITEON_1500:
      result = linear_convert(LINEAR_11, result, 0);
      break;
    case BELPOWER_600_NA:
    case BELPOWER_1100_NA:
    case BELPOWER_1500_NAC:
    case BELPOWER_1100_NAS:
    case BELPOWER_1100_ND:
    case MURATA_1500:
      result = linear_convert(LINEAR_11, result, 1);
      break;
    default:
      break;
  }

  return scnprintf(buf, PAGE_SIZE, "%d\n", result);
}

static ssize_t psu_vstby_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
  int result = psu_convert(dev, attr);

  if (result < 0) {
    /* error case */
    return -EINVAL;
  }

  switch (model) {
    case DELTA_1500:
      result = linear_convert(LINEAR_11, result, 0);
      break;
    case LITEON_1500:
      result = linear_convert(LINEAR_16, result, -9);
      break;
    case BELPOWER_600_NA:
    case BELPOWER_1100_NA:
    case BELPOWER_1100_NAS:
    case BELPOWER_1100_ND:
    case BELPOWER_1500_NAC:
      result = linear_convert(LINEAR_11, result, -6);
      break;
    case MURATA_1500:
      result = linear_convert(LINEAR_11, result, -7);
      break;
    default:
      break;
  }

  return scnprintf(buf, PAGE_SIZE, "%d\n", result);
}

static ssize_t psu_istby_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
  int result = psu_convert(dev, attr);

  if (result < 0) {
    /* error case */
    return -EINVAL;
  }

  switch (model) {
    case DELTA_1500:
    case LITEON_1500:
      result = linear_convert(LINEAR_11, result, 0);
      break;
    case BELPOWER_600_NA:
    case BELPOWER_1100_NAS:
    case BELPOWER_1100_ND:
    case BELPOWER_1100_NA:
      result = linear_convert(LINEAR_11, result, -3);
      break;
    case BELPOWER_1500_NAC:
      result = linear_convert(LINEAR_11, result, -2);
      break;
    case MURATA_1500:
      result = linear_convert(LINEAR_11, result, -7);
      break;
    default:
      break;
  }

  if(model == BELPOWER_600_NA || model == BELPOWER_1100_NA 
    || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND){
    return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
  }else{
    return scnprintf(buf, PAGE_SIZE, "%d\n", result);
  }
}

static ssize_t psu_pstby_show(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
  int result = psu_convert(dev, attr);

  if (result < 0) {
    /* error case */
    return -EINVAL;
  }

  switch (model) {
    case DELTA_1500:
    case LITEON_1500:
      result = linear_convert(LINEAR_11, result, 0);
      break;
    case BELPOWER_600_NA:
    case BELPOWER_1100_NA:
    case BELPOWER_1100_NAS:
    case BELPOWER_1100_ND:
    case BELPOWER_1500_NAC:
      result = linear_convert(LINEAR_11, result, 1);
      break;
    case MURATA_1500:
      result = linear_convert(LINEAR_11, result, -5);
      break;
    default:
      break;
  }
  
  if(model == BELPOWER_600_NA || model == BELPOWER_1100_NA
    || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND){
    return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
  }else{
    return scnprintf(buf, PAGE_SIZE, "%d\n", result);
  }
}

static ssize_t psu_model_show(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
  struct i2c_client *client = to_i2c_client(dev);
  i2c_dev_data_st *data = i2c_get_clientdata(client);
  i2c_sysfs_attr_st *i2c_attr = TO_I2C_SYSFS_ATTR(attr);
  const i2c_dev_attr_st *dev_attr = i2c_attr->isa_i2c_attr;
  uint8_t block[I2C_SMBUS_BLOCK_MAX + 1]={0};
  int result = -1;
  u8 length = 33;
  int count = 10;

  if(UNKNOWN == model) {
    result = psu_convert_model(dev, attr);
    if (result < 0) {
       /* error case */
       return -EINVAL;
    }
  }

    switch (model) {
    case DELTA_1500:
    case BELPOWER_600_NA:
    case BELPOWER_1100_NA:
    case BELPOWER_1100_NAS:
    case BELPOWER_1100_ND:
    case BELPOWER_1500_NAC:
      mutex_lock(&data->idd_lock);
      while((result < 0 || length > 32) && count--) {
        result = i2c_smbus_read_block_data(client, dev_attr->ida_reg, block);
        length = result & 0xff;
        mdelay(10);
      }
      mutex_unlock(&data->idd_lock);  
      if (result < 0 || length > 32) {
        return -EINVAL;
      }
      break;
    default:
      break;
  }

  return scnprintf(buf, PAGE_SIZE, "%s\n", block);
}

static ssize_t psu_serial_show(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
  struct i2c_client *client = to_i2c_client(dev);
  i2c_dev_data_st *data = i2c_get_clientdata(client);
  i2c_sysfs_attr_st *i2c_attr = TO_I2C_SYSFS_ATTR(attr);
  const i2c_dev_attr_st *dev_attr = i2c_attr->isa_i2c_attr;
  uint8_t block[I2C_SMBUS_BLOCK_MAX + 1]={0};
  int result = -1;
  u8 length = 33;
  int count = 10;

  if(UNKNOWN == model) {
    result = psu_convert_model(dev, attr);
    if (result < 0) {
       /* error case */
       return -EINVAL;
    }
  }

  switch (model) {
    case DELTA_1500:
    case BELPOWER_600_NA:
    case BELPOWER_1100_NA:
    case BELPOWER_1100_NAS:
    case BELPOWER_1100_ND:
    case BELPOWER_1500_NAC:
      mutex_lock(&data->idd_lock);
      while((result < 0 || length > 32) && count--) {
        result = i2c_smbus_read_block_data(client, dev_attr->ida_reg, block);
        length = result & 0xff;
        mdelay(10);
      }
      mutex_unlock(&data->idd_lock); 
      if (result < 0 || length > 32) {
        return -EINVAL;
      }
      break;
    default:
      break;
  }

  return scnprintf(buf, PAGE_SIZE, "%s\n", block);
}

static ssize_t psu_revision_show(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
  struct i2c_client *client = to_i2c_client(dev);
  i2c_dev_data_st *data = i2c_get_clientdata(client);
  i2c_sysfs_attr_st *i2c_attr = TO_I2C_SYSFS_ATTR(attr);
  const i2c_dev_attr_st *dev_attr = i2c_attr->isa_i2c_attr;
  uint8_t block[I2C_SMBUS_BLOCK_MAX + 1]={0};
  int result = -1;
  u8 length = 33;
  int count = 10;

  if(UNKNOWN == model) {
    result = psu_convert_model(dev, attr);
    if (result < 0) {
       /* error case */
       return -EINVAL;
    }
  }

  switch (model) {
    case DELTA_1500:
    case BELPOWER_600_NA:
    case BELPOWER_1100_NA:
    case BELPOWER_1100_NAS:
    case BELPOWER_1100_ND:
    case BELPOWER_1500_NAC:
      mutex_lock(&data->idd_lock);
      while((result < 0 || length > 32) && count--) {
        result = i2c_smbus_read_block_data(client, dev_attr->ida_reg, block);
        length = result & 0xff;
        mdelay(10);
      }
      mutex_unlock(&data->idd_lock); 
      if (result < 0 || length > 32) {
        return -EINVAL;
      }
      break;
    default:
      break;
  }

  return scnprintf(buf, PAGE_SIZE, "%s\n", block);
}

static const i2c_dev_attr_st psu_attr_table[] = {
  {
    "in0_input",
    NULL,
    psu_vin_show,
    NULL,
    0x88, 0, 8,
  },
  {
    "curr1_input",
    NULL,
    psu_iin_show,
    NULL,
    0x89, 0, 8,
  },
  {
    "in1_input",
    NULL,
    psu_vout_show,
    NULL,
    0x8b, 0, 8,
  },
  {
    "curr2_input",
    NULL,
    psu_iout_show,
    NULL,
    0x8c, 0, 8,
  },
  {
    "temp1_input",
    NULL,
    psu_temp_show,
    NULL,
    0x8d, 0, 8,
  },
  {
    "temp2_input",
    NULL,
    psu_temp_show,
    NULL,
    0x8e, 0, 8,
  },
  {
    "temp3_input",
    NULL,
    psu_temp_show,
    NULL,
    0x8f, 0, 8,
  },
  {
    "fan1_input",
    NULL,
    psu_fan_show,
    NULL,
    0x90, 0, 8,
  },
  {
    "fan1_fault",
    NULL,
    psu_fan_status_show,
    NULL,
    0x81, 0, 8,
  },
  {
    "power2_input",
    NULL,
    psu_power_show,
    NULL,
    0x96, 0, 8,
  },
  {
    "power1_input",
    NULL,
    psu_power_show,
    NULL,
    0x97, 0, 8,
  },
  {
    "in2_input",
    NULL,
    psu_vstby_show,
    NULL,
    0xd0, 0, 8,
  },
  {
    "curr3_input",
    NULL,
    psu_istby_show,
    NULL,
    0xd1, 0, 8,
  },
  {
    "power3_input",
    NULL,
    psu_pstby_show,
    NULL,
    0xd2, 0, 8,
  },
  {
    "mfr_model_label",
    NULL,
    psu_model_show,
    NULL,
    0x9a, 0, 128,
  },
  {
    "mfr_revision",
    NULL,
    psu_revision_show,
    NULL,
    0x9b, 0, 24,
  },
  {
    "mfr_serial_label",
    NULL,
    psu_serial_show,
    NULL,
    0x9e, 0, 144,
  },
};


/*
 * psu i2c addresses.
 */
#if 0
 static const unsigned short normal_i2c[] = {
  0x58, 0x59, I2C_CLIENT_END
};
#endif

/* psu_driver id */
static const struct i2c_device_id psu_id[] = {
  {"psu_driver", 0},
  { },
};
MODULE_DEVICE_TABLE(i2c, psu_id);

/* Return 0 if detection is successful, -ENODEV otherwise */
static int psu_detect(struct i2c_client *client, int kind,
                         struct i2c_board_info *info)
{
  /*
   * We don't currently do any detection of the driver
   */
  strlcpy(info->type, "psu_driver", I2C_NAME_SIZE);
  return 0;
}

static int psu_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
  int n_attrs = sizeof(psu_attr_table) / sizeof(psu_attr_table[0]);
  struct device *dev = &client->dev;
  i2c_dev_data_st *data;

  data = devm_kzalloc(dev, sizeof(i2c_dev_data_st), GFP_KERNEL);
  if (!data) {
    return -ENOMEM;
  }

  return i2c_dev_sysfs_data_init(client, data,
                                 psu_attr_table, n_attrs);
}

static int psu_remove(struct i2c_client *client)
{
  i2c_dev_data_st *data = i2c_get_clientdata(client);
  i2c_dev_sysfs_data_clean(client, data);

  return 0;
}

static struct i2c_driver psu_driver = {
  .class    = I2C_CLASS_HWMON,
  .driver = {
    .name = "psu_driver",
  },
  .probe    = psu_probe,
  .remove   = psu_remove,
  .id_table = psu_id,
  .detect   = psu_detect,
#if 0
  .address_list = normal_i2c,
#endif
};

static int __init psu_mod_init(void)
{
  return i2c_add_driver(&psu_driver);
}

static void __exit psu_mod_exit(void)
{
  i2c_del_driver(&psu_driver);
}

MODULE_AUTHOR("Mickey Zhan");
MODULE_DESCRIPTION("PSU Driver");
MODULE_LICENSE("GPL");

module_init(psu_mod_init);
module_exit(psu_mod_exit);
