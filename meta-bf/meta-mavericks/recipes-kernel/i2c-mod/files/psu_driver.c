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
#include <linux/platform_device.h>

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
#define DELAY_MS 10
#define RETRY_TIMES 20
#define RETRY_TIMES1 10
#define PSU_PRESENT 0x8
#define PSU_PWOK 0x10
#define PSU1_ADDR 0x5a
#define PSU2_ADDR 0x59
enum {
    psu1_present,
    psu2_present,
    psu_allpresent,
};

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

struct _psu_info_cache{
    int vin;
    int iin;
    int vout;
    int iout;
    int temp;
    int fan;
    int fan_status;
    int power1;
    int power2;
    int vstby;
    int istby;
    int pstby;
    char model[I2C_SMBUS_BLOCK_MAX + 1];
    char serial[I2C_SMBUS_BLOCK_MAX + 1];
    char revision[I2C_SMBUS_BLOCK_MAX + 1];
};
static struct _psu_info_cache  psu_info_cache[2];

extern struct i2c_client * syscpld_client_get();

static int psu_status_get(int addr)
{
    struct i2c_client *client = syscpld_client_get();
    i2c_dev_data_st *data = i2c_get_clientdata(client);
    int val;
    uint8_t psu_status;

    mutex_lock(&data->idd_lock);

    val = i2c_smbus_read_byte_data(client, PSU_PRESENT);
    switch(val & 0x3)
    {
      case 1:
          psu_status = psu1_present;
          break;
      case 2:
          psu_status = psu2_present;
          break;
      default:
          psu_status = psu_allpresent;
          break;
    }

    mutex_unlock(&data->idd_lock);

    //now client is PSU2 , but PSU2 absent , so not need to read any psu info
    if(psu1_present == psu_status && PSU2_ADDR == addr)
    {
        return -1;
    }
    else if(psu2_present == psu_status && PSU1_ADDR == addr)
    {
        return -1;
    }
    else
    {
        return 0;
    }
        
}

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

static int psu_check_power_input(int addr)
{
    struct i2c_client *client = syscpld_client_get();
    i2c_dev_data_st *data = i2c_get_clientdata(client);
    int val;
    uint8_t psu1_pwok, psu2_pwok;

    mutex_lock(&data->idd_lock);

    val = i2c_smbus_read_byte_data(client, PSU_PWOK);
    psu2_pwok = (val >> 2) & 0x1;
    psu1_pwok = (val >> 6) & 0x1;

    mutex_unlock(&data->idd_lock);

    //0: PSU  power input is bad
    //1: PSU  power input is OK
    if(0 == psu1_pwok && PSU1_ADDR == addr) //psu1 canble not plug
    {
        return -1;
    }
    else if (0 == psu2_pwok && PSU2_ADDR == addr)//psu2 canble not plug
    {
        return -1;
    }
    else
    {
        return 0;
    }
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
  count = RETRY_TIMES;
  while((ret < 0 || length > 32) && count--) {
    ret = i2c_smbus_read_block_data(client, PMBUS_MFR_MODEL, block_buffer);
    length = ret & 0xff;
    mdelay(DELAY_MS);
  }

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


  count = RETRY_TIMES;
  mutex_lock(&data->idd_lock);
  while((value < 0 || value == 0xffff) && count--) {
     value = i2c_smbus_read_word_data(client, (dev_attr->ida_reg));
     mdelay(DELAY_MS);
  }
  mutex_unlock(&data->idd_lock);
  if (value < 0 || value == 0xffff) {
	/* error case */
	PSU_DEBUG("I2C read error, value: %d\n", value);
	return -1;
  }

  //PSU_DEBUG("reg:[0x%x] value:[0x%x] [%d]\n", dev_attr->ida_reg, value, value);
  return value;
}

static int psu_convert_model(struct device *dev, struct device_attribute *attr)
{
  struct i2c_client *client = to_i2c_client(dev);
  i2c_dev_data_st *data = i2c_get_clientdata(client);
  i2c_sysfs_attr_st *i2c_attr = TO_I2C_SYSFS_ATTR(attr);
  const i2c_dev_attr_st *dev_attr = i2c_attr->isa_i2c_attr;
  int count = RETRY_TIMES;
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
    mdelay(DELAY_MS);
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

    mutex_lock(&data->idd_lock);
    value = i2c_smbus_read_word_data(client, (dev_attr->ida_reg));
    mutex_unlock(&data->idd_lock);
    mdelay(DELAY_MS);

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
  int val,result;
  uint8_t retry = RETRY_TIMES1;
  struct i2c_client *client = to_i2c_client(dev);
  int psu_status, psu_pwok;
  psu_status = psu_status_get(client->addr);
  if(psu_status < 0)
  {
    return -EINVAL;
  }

  //workaround: if convert fail, diaplay cache info
  val = psu_convert(dev, attr);
  if (val < 0) {
    /* error case */
    if(PSU1_ADDR == client->addr)
    {
        printk(KERN_DEBUG "%s:use PSU1 cache vin\n", __FUNCTION__);
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].vin);
    }
    else
    {
        printk(KERN_DEBUG "%s:use PSU2 cache vin\n", __FUNCTION__);
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].vin);
    }
  }

  // rework bug: Psu sensors show all 0  when no voltage, commit 1d62fca
  psu_pwok = psu_check_power_input(client->addr);
  if(psu_pwok < 0)
  {
      if(PSU1_ADDR == client->addr)
      {
          psu_info_cache[0].vin = 0;
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].vin);
      }
      else
      {
          psu_info_cache[1].vin = 0;
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].vin);
      }
  }

  while(retry)
  {
      switch (model) {
        case DELTA_1500:
        case LITEON_1500:
          result = linear_convert(LINEAR_11, val, 0);
          break;
        case BELPOWER_1100_ND:
        case BELPOWER_600_NA:
        case BELPOWER_1100_NA:
        case BELPOWER_1100_NAS:
        case BELPOWER_1500_NAC:
        case MURATA_1500:
          result = linear_convert(LINEAR_11, val, -1);
          break;
        default:
          break;
      }

      if(result >= 0)
      {
          retry = 0;
      }
      else
      {
          val = psu_update_device(dev, attr);
          retry--;
      }
  }

  if(PSU1_ADDR == client->addr)
  {
      if(result >= 0)
      {
          psu_info_cache[0].vin = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU1 cache vin\n", __FUNCTION__);
      }
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].vin);
  }
  else
  {
      if(result >= 0)
      {
          psu_info_cache[1].vin = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU2 cache vin\n", __FUNCTION__);
      }
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].vin);
  }

}

static ssize_t psu_iin_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
  int val,result;
  uint8_t retry = RETRY_TIMES1;
  struct i2c_client *client = to_i2c_client(dev);
  int psu_status, psu_pwok;
  psu_status = psu_status_get(client->addr);
  if(psu_status < 0)
  {
    return -EINVAL;
  }

//workaround: if convert fail, diaplay cache info
  val = psu_convert(dev, attr);
  if (val < 0) {
    /* error case */
    if(PSU1_ADDR == client->addr)
    {
        printk(KERN_DEBUG "%s:use PSU1 cache iin\n", __FUNCTION__);
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].iin);
    }
    else
    {
        printk(KERN_DEBUG "%s:use PSU2 cache iin\n", __FUNCTION__);
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].iin);
    }
  }

  // rework bug: Psu sensors show all 0  when no voltage, commit 1d62fca
  psu_pwok = psu_check_power_input(client->addr);
  if(psu_pwok < 0)
  {
      if(PSU1_ADDR == client->addr)
      {
          psu_info_cache[0].iin = 0;
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].iin);
      }
      else
      {
          psu_info_cache[1].iin = 0;
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].iin);
      }
  }


  while(retry)
  {
        switch (model) {
        case DELTA_1500:
        case LITEON_1500:
          result = linear_convert(LINEAR_11, val, 0);
          break;
        case BELPOWER_600_NA:
        case BELPOWER_1100_NA:
        case BELPOWER_1500_NAC:
        case BELPOWER_1100_NAS:
          result = linear_convert(LINEAR_11, val, -6);
          break;
        case BELPOWER_1100_ND:
        case MURATA_1500:
          result = linear_convert(LINEAR_11, val, -5);
          break;
        default:
          break;
        }

        if(result >= 0)
        {
          retry = 0;
        }
        else
        {
          val = psu_update_device(dev, attr);
          retry--;
        }
  }


  if(PSU1_ADDR == client->addr)
  {
      if(result >= 0)
      {
          psu_info_cache[0].iin = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU1 cache iin\n", __FUNCTION__);
      }
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].iin);
  }
  else
  {
      if(result >= 0)
      {
          psu_info_cache[1].iin = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU2 cache iin\n", __FUNCTION__);
      }
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].iin);
  }

}

static ssize_t psu_vout_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
  int val,result;
  uint8_t retry = RETRY_TIMES1;
  struct i2c_client *client = to_i2c_client(dev);
  
  int psu_status, psu_pwok;
  psu_status = psu_status_get(client->addr);
  if(psu_status < 0)
  {
    return -EINVAL;
  }
  
  //workaround: if convert fail, diaplay cache info
  val = psu_convert(dev, attr);
  if (val < 0) {
    /* error case */
    if(PSU1_ADDR == client->addr)
    {
      printk(KERN_DEBUG "%s:use PSU1 cache vout\n", __FUNCTION__);
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].vout);
    }
    else
    {
      printk(KERN_DEBUG "%s:use PSU2 cache vout\n", __FUNCTION__);
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].vout);
    }
  }

  // rework bug: Psu sensors show all 0  when no voltage, commit 1d62fca
  psu_pwok = psu_check_power_input(client->addr);
  if(psu_pwok < 0)
  {
      if(PSU1_ADDR == client->addr)
      {
        psu_info_cache[0].vout = 0;
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].vout);
      }
      else
      {
        psu_info_cache[1].vout = 0;
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].vout);
      }

  }

  while(retry)
  {
        switch (model) {
        case DELTA_1500:
          result = linear_convert(LINEAR_11, val, 0);
          break;
        case LITEON_1500:
          result = linear_convert(LINEAR_16, val, -9);
          break;
        case BELPOWER_1100_ND:
        case BELPOWER_600_NA:
        case BELPOWER_1100_NA:
        case BELPOWER_1100_NAS:
        case BELPOWER_1500_NAC:
        case MURATA_1500:
          result = linear_convert(LINEAR_11, val, -6);
          break;
        default:
        break;
        }
          
        if(result >= 0)
        {
          retry = 0;
        }
        else
        {
          val = psu_update_device(dev, attr);
          retry--;
        }

  }

  if(PSU1_ADDR == client->addr)
  {
      if(result >= 0)
      {
          psu_info_cache[0].vout = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU1 cache vout\n", __FUNCTION__);
      }
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].vout);
  }
  else
  {
      if(result >= 0)
      {
          psu_info_cache[1].vout = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU2 cache vout\n", __FUNCTION__);
      }
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].vout);
  }

}

static ssize_t psu_iout_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
  int val,result;
  uint8_t retry = RETRY_TIMES1;
  struct i2c_client *client = to_i2c_client(dev);

  int psu_status, psu_pwok;
  psu_status = psu_status_get(client->addr);
  if(psu_status < 0)
  {
    return -EINVAL;
  }
  
  //workaround: if convert fail, diaplay cache info
  val = psu_convert(dev, attr);
  if (val < 0) {
    /* error case */
  
    if(PSU1_ADDR == client->addr)
    {
        printk(KERN_DEBUG "%s:use PSU1 cache iout\n", __FUNCTION__);
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].iout);
    }
    else
    {
        printk(KERN_DEBUG "%s:use PSU2 cache iout\n", __FUNCTION__);
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].iout);
    }
  }

  // rework bug: Psu sensors show all 0  when no voltage, commit 1d62fca
  psu_pwok = psu_check_power_input(client->addr);
  if(psu_pwok < 0)
  {
      if(PSU1_ADDR == client->addr)
      {
          psu_info_cache[0].iout = 0;
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].iout);
      }
      else
      {
          psu_info_cache[1].iout = 0;
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].iout);
      }
  }

  while(retry)
  {
        switch (model) {
        case DELTA_1500:
        case LITEON_1500:
          result = linear_convert(LINEAR_11, val, 0);
          break;
        case BELPOWER_1100_ND:
        case BELPOWER_600_NA:
        case BELPOWER_1100_NAS:
        case BELPOWER_1100_NA:
          result = linear_convert(LINEAR_11, val, -3);
          break;
        case BELPOWER_1500_NAC:
        case MURATA_1500:
          result = linear_convert(LINEAR_11, val, -2);
          break;
        default:
          break;
        }

        if(result >= 0)
        {
          retry = 0;
        }
        else
        {
          val = psu_update_device(dev, attr);
          retry--;
        }
  }

  if(PSU1_ADDR == client->addr)
  {
      if(result >= 0)
      {
          psu_info_cache[0].iout = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU1 cache iout\n", __FUNCTION__);
      }
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].iout);
  }
  else
  {
      if(result >= 0)
      {
          psu_info_cache[1].iout = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU2 cache iout\n", __FUNCTION__);
      }
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].iout);
  }

}

static ssize_t psu_temp_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
  i2c_sysfs_attr_st *i2c_attr = TO_I2C_SYSFS_ATTR(attr);
  const i2c_dev_attr_st *dev_attr = i2c_attr->isa_i2c_attr;
  struct i2c_client *client = to_i2c_client(dev);

  uint8_t retry = RETRY_TIMES1;
  int val,result;

  int psu_status, psu_pwok;
  psu_status = psu_status_get(client->addr);
  if(psu_status < 0)
  {
    return -EINVAL;
  }
  
  //workaround: if convert fail, diaplay cache info
  val = psu_convert(dev, attr);
  if (val < 0) {
    /* error case */
    if(PSU1_ADDR == client->addr)
    {
        printk(KERN_DEBUG "%s:use PSU1 cache temp\n", __FUNCTION__);
        if(strcmp(dev_attr->ida_name,"temp3_input") == 0 && (model == BELPOWER_600_NA 
        || model == BELPOWER_1100_NA || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND)){
            return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
        }else{
            return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].temp);
        }
    }
    else
    {
        printk(KERN_DEBUG "%s:use PSU2 cache temp\n", __FUNCTION__);
        if(strcmp(dev_attr->ida_name,"temp3_input") == 0 && (model == BELPOWER_600_NA 
        || model == BELPOWER_1100_NA || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND)){
            return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
        }else{
            return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].temp);
        }
    }
  }

  // rework bug: Psu sensors show all 0  when no voltage, commit 1d62fca
  psu_pwok = psu_check_power_input(client->addr);
  if(psu_pwok < 0)
  {
      if(PSU1_ADDR == client->addr)
      {
          psu_info_cache[0].temp = 0;
          if(strcmp(dev_attr->ida_name,"temp3_input") == 0 && (model == BELPOWER_600_NA 
          || model == BELPOWER_1100_NA || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND)){
              return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
          }else{
              return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].temp);
          }
      }
      else
      {
          psu_info_cache[1].temp = 0;
          if(strcmp(dev_attr->ida_name,"temp3_input") == 0 && (model == BELPOWER_600_NA 
          || model == BELPOWER_1100_NA || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND)){
              return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
          }else{
              return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].temp);
          }
      }
  }

  while(retry)
  {
        switch (model) {
        case DELTA_1500:
        case LITEON_1500:
          result = linear_convert(LINEAR_11, val, 0);
          break;
        case BELPOWER_1100_ND:
        case BELPOWER_600_NA:
        case BELPOWER_1100_NA:
        case BELPOWER_1100_NAS:
        case BELPOWER_1500_NAC:
          result = linear_convert(LINEAR_11, val, -3);
          break;
        case MURATA_1500:
          result = linear_convert(LINEAR_11, val, 0);
          break;
        default:
          break;
        }

        if(result >= 0)
        {
          retry = 0;
        }
        else
        {
          val = psu_update_device(dev, attr);
          retry--;
        }
  }


  if(PSU1_ADDR == client->addr)
  {
      if(result >= 0)
      {
          psu_info_cache[0].temp = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU1 cache temp\n", __FUNCTION__);
      }
      if(strcmp(dev_attr->ida_name,"temp3_input") == 0 && (model == BELPOWER_600_NA 
      || model == BELPOWER_1100_NA || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND)){
          return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
      }else{
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].temp);
      }
  }
  else
  {
      if(result >= 0)
      {
          psu_info_cache[1].temp = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU2 cache temp\n", __FUNCTION__);
      }
      if(strcmp(dev_attr->ida_name,"temp3_input") == 0 && (model == BELPOWER_600_NA 
      || model == BELPOWER_1100_NA || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND)){
          return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
      }else{
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].temp);
      }
  }

}

static ssize_t psu_fan_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{

  uint8_t retry = RETRY_TIMES1;
  struct i2c_client *client = to_i2c_client(dev);
  int val,result;

  int psu_status, psu_pwok;
  psu_status = psu_status_get(client->addr);
  if(psu_status < 0)
  {
    return -EINVAL;
  }
  
  //workaround: if convert fail, diaplay cache info
  val = psu_convert(dev, attr);
  if (val < 0) {
    /* error case */
    if(PSU1_ADDR == client->addr)
    {
      printk(KERN_DEBUG "%s:use PSU1 cache fan\n", __FUNCTION__);
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].fan);
    }
    else
    {
      printk(KERN_DEBUG "%s:use PSU2 cache fan\n", __FUNCTION__);
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].fan);
    }
  }


  // rework bug: Psu sensors show all 0  when no voltage, commit 1d62fca
  psu_pwok = psu_check_power_input(client->addr);
  if(psu_pwok < 0)
  {
      if(PSU1_ADDR == client->addr)
      {
        psu_info_cache[0].fan = 0;
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].fan);
      }
      else
      {
        psu_info_cache[1].fan = 0;
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].fan);
      }

  }

  while(retry)
  {
        switch (model) {
        case DELTA_1500:
        case LITEON_1500:
          result = linear_convert(LINEAR_11, val, 0) / 1000;
          break;
        case BELPOWER_1100_ND:
        case BELPOWER_600_NA:
        case BELPOWER_1100_NA:
        case BELPOWER_1100_NAS:
        case BELPOWER_1500_NAC:
        case MURATA_1500:
          result = linear_convert(LINEAR_11, val, 5) / 1000;
          break;
        default:
          break;
        }

        if(result >= 0)
        {
          retry = 0;
        }
        else
        {
          val = psu_update_device(dev, attr);
          retry--;
        }
  }

  if(PSU1_ADDR == client->addr)
  {
      if(result >= 0)
      {
          psu_info_cache[0].fan = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU1 cache fan\n", __FUNCTION__);
      }
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].fan);
  }
  else
  {
      if(result >= 0)
      {
          psu_info_cache[1].fan = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU2 cache fan\n", __FUNCTION__);
      }
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].fan);
  }

}


static ssize_t psu_fan_status_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
  u8 length = 33;
  int count = RETRY_TIMES;
  uint8_t values = 0xff;
  struct i2c_client *client = to_i2c_client(dev);
  int psu_status;
  psu_status = psu_status_get(client->addr);
  if(psu_status < 0)
  {
  return -EINVAL;
  }
  
  //workaround: if convert fail, diaplay cache info
  int result = psu_convert_model(dev, attr);
  if (result < 0) {
    /* error case */
    if(PSU1_ADDR == client->addr)
    {
      printk(KERN_DEBUG "%s:use PSU1 cache fan_status\n", __FUNCTION__);
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].fan_status);
    }
    else
    {
      printk(KERN_DEBUG "%s:use PSU2 cache fan_status\n", __FUNCTION__);
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].fan_status);
    }
  }


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
          mdelay(DELAY_MS);
      }
      if ((result < 0) || (values == 0xff))
          return -EINVAL;

      break;

    default:
      break;
  }

  if(PSU1_ADDR == client->addr)
  {
      if(result >= 0)
      {
          psu_info_cache[0].fan_status = values;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU1 cache fan_status\n", __FUNCTION__);
      }
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].fan_status);
  }
  else
  {
      if(result >= 0)
      {
          psu_info_cache[1].fan_status = values;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU2 cache fan_status\n", __FUNCTION__);
      }
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].fan_status);
  }

}

static ssize_t psu_power_show(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{

  uint8_t retry = RETRY_TIMES1;
  i2c_sysfs_attr_st *i2c_attr = TO_I2C_SYSFS_ATTR(attr);
  const i2c_dev_attr_st *dev_attr = i2c_attr->isa_i2c_attr;
  struct i2c_client *client = to_i2c_client(dev);
  int val,result;
  int psu_status, psu_pwok;
  psu_status = psu_status_get(client->addr);
  if(psu_status < 0)
  {
    return -EINVAL;
  }
  
  //workaround: if convert fail, diaplay cache info
  val = psu_convert(dev, attr);
  if (val < 0) {
    /* error case */
    if(0x97 == dev_attr->ida_reg)
    {
        if(PSU1_ADDR == client->addr)
        {
          printk(KERN_DEBUG "%s:use PSU1 cache power1\n", __FUNCTION__);
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].power1);
        }
        else
        {
          printk(KERN_DEBUG "%s:use PSU2 cache power1\n", __FUNCTION__);
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].power1);
        }
    }
    else
    {
        if(PSU1_ADDR == client->addr)
        {
          printk(KERN_DEBUG "%s:use PSU1 cache power2\n", __FUNCTION__);
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].power2);
        }
        else
        {
          printk(KERN_DEBUG "%s:use PSU2 cache power2\n", __FUNCTION__);
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].power2);
        }
    }
  }


  // rework bug: Psu sensors show all 0  when no voltage, commit 1d62fca
  psu_pwok = psu_check_power_input(client->addr);
  if(psu_pwok < 0)
  {
      if(0x97 == dev_attr->ida_reg)
      {
          if(PSU1_ADDR == client->addr)
          {
            psu_info_cache[0].power1 = 0;
            return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].power1);
          }
          else
          {
            psu_info_cache[1].power1 = 0;
            return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].power1);
          }
      }
      else
      {
          if(PSU1_ADDR == client->addr)
          {
            psu_info_cache[0].power2 = 0;
            return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].power2);
          }
          else
          {
            psu_info_cache[1].power2 = 0;
            return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].power2);
          }
      }

  }

  while(retry)
  {
      switch (model) {
        case DELTA_1500:
        case LITEON_1500:
          result = linear_convert(LINEAR_11, val, 0);
          break;
        case BELPOWER_1100_ND:
        case BELPOWER_600_NA:
        case BELPOWER_1100_NA:
        case BELPOWER_1500_NAC:
        case BELPOWER_1100_NAS:
        case MURATA_1500:
          result = linear_convert(LINEAR_11, val, 1);
          break;
        default:
          break;
      }
      if(result >= 0)
      {
        retry = 0;
      }
      else
      {
        val = psu_update_device(dev, attr);
        retry--;
      }
  }

  if(0x97 == dev_attr->ida_reg)
  {
      if(PSU1_ADDR == client->addr)
      {
        if(result >= 0)
        {
            psu_info_cache[0].power1 = result;
        }
        else
        {
            printk(KERN_DEBUG "%s:use PSU1 cache power1\n", __FUNCTION__);
        }
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].power1);
      }
      else
      {
        if(result >= 0)
        {
            psu_info_cache[1].power1 = result;
        }
        else
        {
            printk(KERN_DEBUG "%s:use PSU2 cache power1\n", __FUNCTION__);
        }
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].power1);
      }
  }
  else
  {
      if(PSU1_ADDR == client->addr)
      {
        if(result >= 0)
        {
            psu_info_cache[0].power2 = result;
        }
        else
        {
            printk(KERN_DEBUG "%s:use PSU1 cache power2\n", __FUNCTION__);
        }
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].power2);
      }
      else
      {
        if(result >= 0)
        {
            psu_info_cache[1].power2 = result;
        }
        else
        {
            printk(KERN_DEBUG "%s:use PSU2 cache power2\n", __FUNCTION__);
        }
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].power2);
      }
  }

}

static ssize_t psu_vstby_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{

  uint8_t retry = RETRY_TIMES1;
  struct i2c_client *client = to_i2c_client(dev);
  int val,result;
  int psu_status, psu_pwok;
  psu_status = psu_status_get(client->addr);
  if(psu_status < 0)
  {
    return -EINVAL;
  }
  
  //workaround: if convert fail, diaplay cache info
  val = psu_convert(dev, attr);
  if (val < 0) {
    /* error case */
    if(PSU1_ADDR == client->addr)
    {
        printk(KERN_DEBUG "%s:use PSU1 cache vstby\n", __FUNCTION__);
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].vstby);
    }
    else
    {
        printk(KERN_DEBUG "%s:use PSU2 cache vstby\n", __FUNCTION__);
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].vstby);
    }
  }

  // rework bug: Psu sensors show all 0  when no voltage, commit 1d62fca
  psu_pwok = psu_check_power_input(client->addr);
  if(psu_pwok < 0)
  {
      if(PSU1_ADDR == client->addr)
      {
          psu_info_cache[0].vstby = 0;
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].vstby);
      }
      else
      {
          psu_info_cache[1].vstby = 0;
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].vstby);
      }
  }


  while(retry)
  {
      switch (model) {
        case DELTA_1500:
          result = linear_convert(LINEAR_11, val, 0);
          break;
        case LITEON_1500:
          result = linear_convert(LINEAR_16, val, -9);
          break;
        case BELPOWER_1100_ND:
        case BELPOWER_600_NA:
        case BELPOWER_1100_NA:
        case BELPOWER_1100_NAS:
        case BELPOWER_1500_NAC:
          result = linear_convert(LINEAR_11, val, -6);
          break;
        case MURATA_1500:
          result = linear_convert(LINEAR_11, val, -7);
          break;
        default:
          break;
      }

      if(result >= 0)
      {
        retry = 0;
      }
      else
      {
        val = psu_update_device(dev, attr);
        retry--;
      }
  }

  if(PSU1_ADDR == client->addr)
  {
      if(result >= 0)
      {
          psu_info_cache[0].vstby = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU1 cache vstby\n", __FUNCTION__);
      }
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].vstby);
  }
  else
  {
      if(result >= 0)
      {
          psu_info_cache[1].vstby = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU2 cache vstby\n", __FUNCTION__);
      }
      return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].vstby);
  }


}

static ssize_t psu_istby_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{

  uint8_t retry = RETRY_TIMES1;
  struct i2c_client *client = to_i2c_client(dev);
  int val,result;
  int psu_status, psu_pwok;
  psu_status = psu_status_get(client->addr);
  if(psu_status < 0)
  {
    return -EINVAL;
  }
  
  //workaround: if convert fail, diaplay cache info
  val = psu_convert(dev, attr);
  if (val < 0) {
    /* error case */
    if(PSU1_ADDR == client->addr)
    {
        printk(KERN_DEBUG "%s:use PSU1 cache istby\n", __FUNCTION__);
        if(model == BELPOWER_600_NA || model == BELPOWER_1100_NA 
          || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND){
          return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
        }else{
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].istby);
        }
    }
    else
    {
        printk(KERN_DEBUG "%s:use PSU2 cache istby\n", __FUNCTION__);
        if(model == BELPOWER_600_NA || model == BELPOWER_1100_NA 
          || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND){
          return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
        }else{
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].istby);
        }
    }
  }

  // rework bug: Psu sensors show all 0  when no voltage, commit 1d62fca
  psu_pwok = psu_check_power_input(client->addr);
  if(psu_pwok < 0)
  {
      if(PSU1_ADDR == client->addr)
      {
          psu_info_cache[0].istby = 0;
          if(model == BELPOWER_600_NA || model == BELPOWER_1100_NA 
            || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND){
            return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
          }else{
            return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].istby);
          }
      }
      else
      {
          psu_info_cache[1].istby = 0;
          if(model == BELPOWER_600_NA || model == BELPOWER_1100_NA 
            || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND){
            return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
          }else{
            return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].istby);
          }
      }
  }


  while(retry)
  {
        switch (model) {
        case DELTA_1500:
        case LITEON_1500:
          result = linear_convert(LINEAR_11, val, 0);
          break;
        case BELPOWER_600_NA:
        case BELPOWER_1100_NAS:
        case BELPOWER_1100_ND:
        case BELPOWER_1100_NA:
          result = linear_convert(LINEAR_11, val, -3);
          break;
        case BELPOWER_1500_NAC:
          result = linear_convert(LINEAR_11, val, -2);
          break;
        case MURATA_1500:
          result = linear_convert(LINEAR_11, val, -7);
          break;
        default:
          break;
        }
        if(result >= 0)
        {
        retry = 0;
        }
        else
        {
        val = psu_update_device(dev, attr);
        retry--;
        }
  }


  if(PSU1_ADDR == client->addr)
  {
      if(result >= 0)
      {
          psu_info_cache[0].istby = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU1 cache istby\n", __FUNCTION__);
      }
      if(model == BELPOWER_600_NA || model == BELPOWER_1100_NA 
        || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND){
        return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
      }else{
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].istby);
      }
  }
  else
  {
      if(result >= 0)
      {
          psu_info_cache[1].istby = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU2 cache istby\n", __FUNCTION__);
      }
      if(model == BELPOWER_600_NA || model == BELPOWER_1100_NA 
        || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND){
        return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
      }else{
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].istby);
      }
  }

}

static ssize_t psu_pstby_show(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{

  uint8_t retry = RETRY_TIMES1;
  struct i2c_client *client = to_i2c_client(dev);
  int val,result;

  int psu_status, psu_pwok;
  psu_status = psu_status_get(client->addr);
  if(psu_status < 0)
  {
    return -EINVAL;
  }
  
  //workaround: if convert fail, diaplay cache info
  val = psu_convert(dev, attr);
  if (val < 0) {
    /* error case */
    if(PSU1_ADDR == client->addr)
    {
        printk(KERN_DEBUG "%s:use PSU1 cache pstby\n", __FUNCTION__);
        if(model == BELPOWER_600_NA || model == BELPOWER_1100_NA
          || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND){
          return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
        }else{
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].pstby);
        }
    }
    else
    {
        printk(KERN_DEBUG "%s:use PSU2 cache pstby\n", __FUNCTION__);
        if(model == BELPOWER_600_NA || model == BELPOWER_1100_NA
          || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND){
          return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
        }else{
          return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].pstby);
        }
    }
  }

  // rework bug: Psu sensors show all 0  when no voltage, commit 1d62fca
  psu_pwok = psu_check_power_input(client->addr);
  if(psu_pwok < 0)
  {
      if(PSU1_ADDR == client->addr)
      {
          psu_info_cache[0].pstby = 0;
          if(model == BELPOWER_600_NA || model == BELPOWER_1100_NA
            || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND){
            return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
          }else{
            return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].pstby);
          }
      }
      else
      {
          psu_info_cache[1].pstby = 0;
          if(model == BELPOWER_600_NA || model == BELPOWER_1100_NA
            || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND){
            return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
          }else{
            return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].pstby);
          }
      }
  }

  while(retry)
  {
      switch (model) {
        case DELTA_1500:
        case LITEON_1500:
          result = linear_convert(LINEAR_11, val, 0);
          break;
        case BELPOWER_600_NA:
        case BELPOWER_1100_NA:
        case BELPOWER_1100_NAS:
        case BELPOWER_1100_ND:
        case BELPOWER_1500_NAC:
          result = linear_convert(LINEAR_11, val, 1);
          break;
        case MURATA_1500:
          result = linear_convert(LINEAR_11, val, -5);
          break;
        default:
          break;
        }

        if(result >= 0)
        {
        retry = 0;
        }
        else
        {
        val = psu_update_device(dev, attr);
        retry--;
        }
  }


  if(PSU1_ADDR == client->addr)
  {
      if(result >= 0)
      {
          psu_info_cache[0].pstby = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU1 cache pstby\n", __FUNCTION__);
      }
      if(model == BELPOWER_600_NA || model == BELPOWER_1100_NA
        || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND){
        return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
      }else{
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[0].pstby);
      }
  }
  else
  {
      if(result >= 0)
      {
          psu_info_cache[1].pstby = result;
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU2 cache pstby\n", __FUNCTION__);
      }
      if(model == BELPOWER_600_NA || model == BELPOWER_1100_NA
        || model == BELPOWER_1100_NAS || model == BELPOWER_1100_ND){
        return scnprintf(buf, PAGE_SIZE, "%s\n", "N/A");
      }else{
        return scnprintf(buf, PAGE_SIZE, "%d\n", psu_info_cache[1].pstby);
      }
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
  int count = RETRY_TIMES;

  int psu_status;
  psu_status = psu_status_get(client->addr);
  if(psu_status < 0)
  {
    return -EINVAL;
  }
  

  if(UNKNOWN == model) {
    //workaround: if convert fail, diaplay cache info
    result = psu_convert_model(dev, attr);
    if (result < 0) {
      /* error case */
      if(PSU1_ADDR == client->addr)
      {
          printk(KERN_DEBUG "%s:use PSU1 cache model\n", __FUNCTION__);
          return scnprintf(buf, PAGE_SIZE, "%s\n", psu_info_cache[0].model);
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU2 cache model\n", __FUNCTION__);
          return scnprintf(buf, PAGE_SIZE, "%s\n", psu_info_cache[1].model);
      }
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
        mdelay(DELAY_MS);
      }
      mutex_unlock(&data->idd_lock);  
      if (result < 0 || length > 32) {
        return -EINVAL;
      }
      break;
    default:
      break;
  }

  if(PSU1_ADDR == client->addr)
  {
      strcpy(psu_info_cache[0].model, block);
  }
  else
  {
      strcpy(psu_info_cache[1].model, block);
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
  int count = RETRY_TIMES;

  int psu_status;
  psu_status = psu_status_get(client->addr);
  if(psu_status < 0)
  {
    return -EINVAL;
  }
  

  if(UNKNOWN == model) {
    //workaround: if convert fail, diaplay cache info
    result = psu_convert_model(dev, attr);
    if (result < 0) {
      /* error case */
      if(PSU1_ADDR == client->addr)
      {
          printk(KERN_DEBUG "%s:use PSU1 cache serial\n", __FUNCTION__);
          return scnprintf(buf, PAGE_SIZE, "%s\n", psu_info_cache[0].serial);
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU2 cache serial\n", __FUNCTION__);
          return scnprintf(buf, PAGE_SIZE, "%s\n", psu_info_cache[1].serial);
      }
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
        mdelay(DELAY_MS);
      }
      mutex_unlock(&data->idd_lock); 
      if (result < 0 || length > 32) {
        return -EINVAL;
      }
      break;
    default:
      break;
  }

  if(PSU1_ADDR == client->addr)
  {
      strcpy(psu_info_cache[0].serial, block);
  }
  else
  {
      strcpy(psu_info_cache[1].serial, block);
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
  int count = RETRY_TIMES;

  int psu_status;
  psu_status = psu_status_get(client->addr);
  if(psu_status < 0)
  {
    return -EINVAL;
  }
  

  if(UNKNOWN == model) {
    //workaround: if convert fail, diaplay cache info
    result = psu_convert_model(dev, attr);
    if (result < 0) {
      /* error case */
      if(PSU1_ADDR == client->addr)
      {
          printk(KERN_DEBUG "%s:use PSU1 cache revision\n", __FUNCTION__);
          return scnprintf(buf, PAGE_SIZE, "%s\n", psu_info_cache[0].revision);
      }
      else
      {
          printk(KERN_DEBUG "%s:use PSU2 cache revision\n", __FUNCTION__);
          return scnprintf(buf, PAGE_SIZE, "%s\n", psu_info_cache[1].revision);
      }
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
        mdelay(DELAY_MS);
      }
      mutex_unlock(&data->idd_lock); 
      if (result < 0 || length > 32) {
        return -EINVAL;
      }
      break;
    default:
      break;
  }

  if(PSU1_ADDR == client->addr)
  {
      strcpy(psu_info_cache[0].revision, block);
  }
  else
  {
      strcpy(psu_info_cache[1].revision, block);
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
