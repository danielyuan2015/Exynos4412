/* drivers/i2c/chips/kxud9.c - KXUD9 accelerometer driver
 *
 * Copyright (C) 2010 Kionix, Inc.
 * Written by Kuching Tan <kuchingtan@kionix.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/kxud9.h>
#include <linux/miscdevice.h>

#define NAME			"kxud9"
#define G_MAX			8000
/* OUTPUT REGISTERS */
#define XOUT_H			0x00
#define AUXOUT_H		0x06
#define RESET_WRITE		0x0A
/* RESET_WRITE BITS */
#define RESET 			0xCA
/* CONTROL REGISTERS */
#define CTRL_REGC		0x0C
#define CTRL_REGB		0x0D
/* INPUT_ABS CONSTANTS */
#define FUZZ			32
#define FLAT			32
/* RESUME STATE INDICES */
#define RES_CTRL_REGC		0
#define RES_CTRL_REGB		1
#define RESUME_ENTRIES		2
/* OFFSET and SENSITIVITY */
#define	OFFSET		2048
#define SENS_2G		819
#define SENS_4G		410
#define SENS_6G		273
#define SENS_8G		205

#define IOCTL_BUFFER_SIZE	64

/*
 * The following table lists the maximum appropriate poll interval for each
 * available output data rate.
 */
struct {
	unsigned int cutoff;
	u8 mask;
} kxud9_odr_table[] = {
	{
	1,	ODR2000D}, {
	2,	ODR1000D}, {
	10,	ODR500D}, {
	20,	ODR100D}, {
	0,	ODR50D},};

struct kxud9_data {
	struct i2c_client *client;
	struct kxud9_platform_data *pdata;
	struct mutex lock;
	struct delayed_work input_work;
	struct input_dev *input_dev;

	int sensitivity;
	int hw_initialized;
	atomic_t enabled;
	u8 resume[RESUME_ENTRIES];
	int res_interval;
};

static struct kxud9_data *ud9 = NULL;
static atomic_t kxud9_dev_open_count;

// Debug Flag, set to 1 to turn on debugging output
#define DEBUG 0

static int kxud9_i2c_read(u8 addr, u8 *data, int len)
{
	int err;

	struct i2c_msg msgs[] = {
		{
		 .addr = ud9->client->addr,
		 .flags = ud9->client->flags & I2C_M_TEN,
		 .len = 1,
		 .buf = &addr,
		 },
		{
		 .addr = ud9->client->addr,
		 .flags = (ud9->client->flags & I2C_M_TEN) | I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};
	err = i2c_transfer(ud9->client->adapter, msgs, 2);

	if(err != 2)
		dev_err(&ud9->client->dev, "read transfer error\n");
	else
		err = 0;

	return err;
}

static int kxud9_i2c_write(u8 addr, u8 *data, int len)
{
	int err;
	int i;
	u8 buf[len + 1];

	struct i2c_msg msgs[] = {
		{
		 .addr = ud9->client->addr,
		 .flags = ud9->client->flags & I2C_M_TEN,
		 .len = len + 1,
		 .buf = buf,
		 },
	};

	buf[0] = addr;
	for (i = 0; i < len; i++)
		buf[i + 1] = data[i];

	err = i2c_transfer(ud9->client->adapter, msgs, 1);

	if(err != 1)
		dev_err(&ud9->client->dev, "write transfer error\n");
	else
		err = 0;

	return err;
}

int kxud9_get_bits(u8 reg_addr, u8* bits_value, u8 bits_mask)
{
	int err;
	u8 reg_data;

	err = kxud9_i2c_read(reg_addr, &reg_data, 1);
	if(err < 0)
		return err;

	*bits_value = reg_data & bits_mask;

	return 1;
}

int kxud9_get_byte(u8 reg_addr, u8* reg_value)
{
	int err;
	u8 reg_data;

	err = kxud9_i2c_read(reg_addr, &reg_data, 1);
	if(err < 0)
		return err;

	*reg_value = reg_data;

	return 1;
}

int kxud9_set_bits(int res_index, u8 reg_addr, u8 bits_value, u8 bits_mask)
{
	int err=0, retval=0;
	u8 reg_data = 0x00, reg_bits = 0x00, bits_set = 0x00;

	// Read from device register
	err = kxud9_i2c_read(reg_addr, &reg_data, 1);
	if(err < 0)
		goto exit0;

	// Apply mask to device register;
	reg_bits = reg_data & bits_mask;

	// Update resume state data
	bits_set = bits_mask & bits_value;
	ud9->resume[res_index] &= ~bits_mask;
	ud9->resume[res_index] |= bits_set;

	// Return 0 if value in device register and value to be written is the same
	if(reg_bits == bits_set)
		retval = 0;
	// Else, return 1
	else
		retval = 1;

	// Write to device register
	err = kxud9_i2c_write(reg_addr, &ud9->resume[res_index], 1);
	if(err < 0)
		goto exit0;

exit0:
	if(err < 0)
		return err;

	return retval;
}

int kxud9_set_byte(int res_index, u8 reg_addr, u8 reg_value)
{
	int err, retval=0;
	u8 reg_data;

	// Read from device register
	err = kxud9_i2c_read(reg_addr, &reg_data, 1);
	if(err < 0)
		goto exit0;

	// Update resume state data
	ud9->resume[res_index] = reg_value;

	// Return 0 if value in device register and value to be written is the same
	if(reg_data == reg_value)
		retval = 0;
	// Else, return 1
	else
		retval = 1;

	// Write to device register
	err = kxud9_i2c_write(reg_addr, &ud9->resume[res_index], 1);
	if(err < 0)
		goto exit0;

exit0:
	if(err < 0)
		return err;

	return retval;
}

int kxud9_reset(void)
{
	u8 reg_data = RESET;

	return kxud9_i2c_write(RESET_WRITE, &reg_data, 1);
}

static int kxud9_hw_init(void)
{
	int err;
	err = kxud9_i2c_write(CTRL_REGC, &ud9->resume[RES_CTRL_REGC], 1);
	if(err < 0)
		return err;
	err = kxud9_i2c_write(CTRL_REGB, &ud9->resume[RES_CTRL_REGB], 1);
	if(err < 0)
		return err;

	ud9->hw_initialized = 1;

	return 0;
}

static void kxud9_device_power_off(void)
{
	int err;
	u8 buf = ~ENABLE;

	err = kxud9_i2c_write(CTRL_REGB, &buf, 1);
	if(err < 0)
		dev_err(&ud9->client->dev, "soft power off failed\n");
	if(ud9->pdata->power_off)
		ud9->pdata->power_off();
	ud9->hw_initialized = 0;
}

static int kxud9_device_power_on(void)
{
	int err;

	if(ud9->pdata->power_on) {
		err = ud9->pdata->power_on();
		if(err < 0)
			return err;
	}
	if(!ud9->hw_initialized) {
		mdelay(100);
		err = kxud9_hw_init();
		if(err < 0) {
			kxud9_device_power_off();
			return err;
		}
	}

	return 0;
}

int kxud9_update_g_range(u8 new_g_range)
{
	int err;
	u8 buf;

	switch (new_g_range) {
	case KXUD9_G_2G:
		ud9->sensitivity = SENS_2G;
		break;
	case KXUD9_G_4G:
		ud9->sensitivity = SENS_4G;
		break;
	case KXUD9_G_6G:
		ud9->sensitivity = SENS_6G;
		break;
	case KXUD9_G_8G:
		ud9->sensitivity = SENS_8G;
		break;
	default:
		dev_err(&ud9->client->dev, "invalid g range request\n");
		return -EINVAL;
	}
	buf = (ud9->resume[RES_CTRL_REGC] & ~KXUD9_G_2G) | new_g_range;
	if(atomic_read(&ud9->enabled)) {
		err = kxud9_i2c_write(CTRL_REGC, &buf, 1);
		if(err < 0)
			return err;
	}
	ud9->resume[RES_CTRL_REGC] = buf;

	return 0;
}

static int kxud9_update_odr(int poll_interval)
{
	int err = -1;
	int i;
	u8 config;

	/*  Convert the poll interval into an output data rate configuration
	 *  that is as low as possible.  The ordering of these checks must be
	 *  maintained due to the cascading cut off values - poll intervals are
	 *  checked from shortest to longest.  At each check, if the next lower
	 *  ODR cannot support the current poll interval, we stop searching */
	for (i = 0; i < ARRAY_SIZE(kxud9_odr_table); i++) {
		config = kxud9_odr_table[i].mask;
		if(poll_interval < kxud9_odr_table[i].cutoff)
			break;
	}

	config |= (ud9->resume[RES_CTRL_REGC] & ~ODR50D);
	ud9->resume[RES_CTRL_REGC] = config;
	if(atomic_read(&ud9->enabled)) {
		err = kxud9_set_byte(RES_CTRL_REGC, CTRL_REGC, config);
		if(err < 0)
			return err;
	}
	#if defined DEBUG && DEBUG == 1
	dev_info(&ud9->client->dev, "%s: poll_interval is %dms, ODR set to 0x%02X\n", __FUNCTION__, poll_interval, config);
	#endif

	return 0;
}

static int kxud9_get_acceleration_data(int *xyz)
{
	const char ACC_REG_SIZE = 6;
	int err;
	/* Data bytes from hardware xH, xL, yH, yL, zH, zL */
	u8 acc_data[ACC_REG_SIZE];
	/* x,y,z hardware values */
	int hw_d[3];

	err = kxud9_i2c_read(XOUT_H, acc_data, ACC_REG_SIZE);
	if(err < 0)
		return err;

	hw_d[0] = (int) (((acc_data[0] << 4) | (acc_data[1] >> 4)) - OFFSET);
	hw_d[1] = (int) (((acc_data[2] << 4) | (acc_data[3] >> 4)) - OFFSET);
	hw_d[2] = (int) (((acc_data[4] << 4) | (acc_data[5] >> 4)) - OFFSET);

	xyz[0] = ((ud9->pdata->negate_x) ? (-hw_d[ud9->pdata->axis_map_x])
		  : (hw_d[ud9->pdata->axis_map_x]));
	xyz[1] = ((ud9->pdata->negate_y) ? (-hw_d[ud9->pdata->axis_map_y])
		  : (hw_d[ud9->pdata->axis_map_y]));
	xyz[2] = ((ud9->pdata->negate_z) ? (-hw_d[ud9->pdata->axis_map_z])
		  : (hw_d[ud9->pdata->axis_map_z]));

	#if defined DEBUG && DEBUG == 1
	dev_info(&ud9->client->dev, "%s: x:%5d y:%5d z:%5d\n", __FUNCTION__, xyz[0], xyz[1], xyz[2]);
	#endif

	return err;
}

static void kxud9_report_values(int *xyz)
{
	input_report_abs(ud9->input_dev, ABS_X, xyz[0]);
	input_report_abs(ud9->input_dev, ABS_Y, xyz[1]);
	input_report_abs(ud9->input_dev, ABS_Z, xyz[2]);
	input_sync(ud9->input_dev);
}

static int kxud9_enable(void)
{
	int err;

	if(!atomic_cmpxchg(&ud9->enabled, 0, 1)) {
		err = kxud9_device_power_on();
		if(err < 0) {
			dev_err(&ud9->client->dev,
					"error powering on: %d\n", err);
			atomic_set(&ud9->enabled, 0);
			return err;
		}

		schedule_delayed_work(&ud9->input_work, msecs_to_jiffies(ud9->
						res_interval));

		#if defined DEBUG && DEBUG == 1
		dev_info(&ud9->client->dev, "%s: Enabled\n", __FUNCTION__);
		#endif
	}

	return 0;
}

static int kxud9_disable(void)
{
	if(atomic_cmpxchg(&ud9->enabled, 1, 0)) {
		cancel_delayed_work_sync(&ud9->input_work);
		kxud9_device_power_off();
		#if defined DEBUG && DEBUG == 1
		dev_info(&ud9->client->dev, "%s: Disabled\n", __FUNCTION__);
		#endif
	}

	return 0;
}

static void kxud9_input_work_func(struct work_struct *work)
{
	int xyz[3] = { 0 };
	int err;

	mutex_lock(&ud9->lock);
	err = kxud9_get_acceleration_data(xyz);
	if(err < 0)
		dev_err(&ud9->client->dev, "get_acceleration_data failed\n");
	else
		kxud9_report_values(xyz);
	schedule_delayed_work(&ud9->input_work,
			      msecs_to_jiffies(ud9->res_interval));
	mutex_unlock(&ud9->lock);
}

int kxud9_input_open(struct input_dev *input)
{
	return kxud9_enable();
}

void kxud9_input_close(struct input_dev *dev)
{
	kxud9_disable();
}

static int kxud9_input_init(void)
{
	int err;

	INIT_DELAYED_WORK(&ud9->input_work, kxud9_input_work_func);
	ud9->input_dev = input_allocate_device();
	if(!ud9->input_dev) {
		err = -ENOMEM;
		dev_err(&ud9->client->dev, "input device allocate failed\n");
		goto err0;
	}
	ud9->input_dev->open = kxud9_input_open;
	ud9->input_dev->close = kxud9_input_close;

	input_set_drvdata(ud9->input_dev, ud9);

	set_bit(EV_ABS, ud9->input_dev->evbit);
	set_bit(ABS_MISC, ud9->input_dev->absbit);

	input_set_abs_params(ud9->input_dev, ABS_X, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(ud9->input_dev, ABS_Y, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(ud9->input_dev, ABS_Z, -G_MAX, G_MAX, FUZZ, FLAT);

	ud9->input_dev->name = INPUT_NAME_ACC;

	err = input_register_device(ud9->input_dev);
	if(err) {
		dev_err(&ud9->client->dev,
			"unable to register input polled device %s: %d\n",
			ud9->input_dev->name, err);
		goto err1;
	}

	return 0;
err1:
	input_free_device(ud9->input_dev);
err0:
	return err;
}

static void kxud9_input_cleanup(void)
{
	input_unregister_device(ud9->input_dev);
}

/* sysfs */
static ssize_t kxud9_delay_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ud9->res_interval);
}

static ssize_t kxud9_delay_store(struct device *dev,
					struct device_attribute *attr,
						const char *buf, size_t count)
{
	int val = simple_strtoul(buf, NULL, 10);

	ud9->res_interval = max(val, ud9->pdata->min_interval);
	kxud9_update_odr(ud9->res_interval);

	return count;
}

static ssize_t kxud9_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&ud9->enabled));
}

static ssize_t kxud9_enable_store(struct device *dev,
					struct device_attribute *attr,
						const char *buf, size_t count)
{
	int val = simple_strtoul(buf, NULL, 10);
	if(val)
		kxud9_enable();
	else
		kxud9_disable();
	return count;
}

static ssize_t kxud9_selftest_store(struct device *dev,
					struct device_attribute *attr,
						const char *buf, size_t count)
{
	int val = simple_strtoul(buf, NULL, 10);
	u8 ctrl;
	if(val)
		ud9->resume[RES_CTRL_REGB] |= (ST);
	else
		ud9->resume[RES_CTRL_REGB] &= (~ST);
	ctrl = ud9->resume[RES_CTRL_REGB];
	kxud9_i2c_write(CTRL_REGB, &ctrl, 1);
	return count;
}

static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR, kxud9_delay_show, kxud9_delay_store);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR, kxud9_enable_show,
						kxud9_enable_store);
static DEVICE_ATTR(selftest, S_IWUSR, NULL, kxud9_selftest_store);

static struct attribute *kxud9_attributes[] = {
	&dev_attr_delay.attr,
	&dev_attr_enable.attr,
	&dev_attr_selftest.attr,
	NULL
};

static struct attribute_group kxud9_attribute_group = {
	.attrs = kxud9_attributes
};
/* /sysfs */

static int kxud9_get_count(char *buf, int bufsize)
{
	const char ACC_REG_SIZE = 6;
	int err;
	/* Data bytes from hardware xH, xL, yH, yL, zH, zL */
	u8 acc_data[ACC_REG_SIZE];
	/* x,y,z hardware values */
	int hw_d[3], xyz[3];

	if((!buf)||(bufsize<=(sizeof(xyz)*3)))
		return -1;

	err = kxud9_i2c_read(XOUT_H, acc_data, ACC_REG_SIZE);
	if(err < 0)
		return err;

	hw_d[0] = (int) (((acc_data[0] << 4) | (acc_data[1] >> 4)) - OFFSET);
	hw_d[1] = (int) (((acc_data[2] << 4) | (acc_data[3] >> 4)) - OFFSET);
	hw_d[2] = (int) (((acc_data[4] << 4) | (acc_data[5] >> 4)) - OFFSET);

	xyz[0] = ((ud9->pdata->negate_x) ? (-hw_d[ud9->pdata->axis_map_x])
		  : (hw_d[ud9->pdata->axis_map_x]));
	xyz[1] = ((ud9->pdata->negate_y) ? (-hw_d[ud9->pdata->axis_map_y])
		  : (hw_d[ud9->pdata->axis_map_y]));
	xyz[2] = ((ud9->pdata->negate_z) ? (-hw_d[ud9->pdata->axis_map_z])
		  : (hw_d[ud9->pdata->axis_map_z]));

	sprintf(buf, "%d %d %d %d %d %d %d %d %d",\
			xyz[0], xyz[1], xyz[2],\
			0, 0, 0,\
			ud9->sensitivity, ud9->sensitivity, ud9->sensitivity);

	#if defined DEBUG && DEBUG == 1
	dev_info(&ud9->client->dev, "%s: [%5d] [%5d] [%5d] [%5d] [%5d] [%5d] [%5d] [%5d] [%5d]\n", __FUNCTION__,\
			xyz[0], xyz[1], xyz[2],\
			0, 0, 0,\
			ud9->sensitivity, ud9->sensitivity, ud9->sensitivity);
	#endif

	return err;
}

static int kxud9_get_mg(char *buf, int bufsize)
{
	const char ACC_REG_SIZE = 6;
	int err;
	/* Data bytes from hardware xH, xL, yH, yL, zH, zL */
	u8 acc_data[ACC_REG_SIZE];
	/* x,y,z hardware values */
	int hw_d[3], xyz[3], mg[3];

	if((!buf)||(bufsize<=(sizeof(mg))))
		return -1;

	err = kxud9_i2c_read(XOUT_H, acc_data, ACC_REG_SIZE);
	if(err < 0)
		return err;

	hw_d[0] = (int) (((acc_data[0] << 4) | (acc_data[1] >> 4)) - OFFSET);
	hw_d[1] = (int) (((acc_data[2] << 4) | (acc_data[3] >> 4)) - OFFSET);
	hw_d[2] = (int) (((acc_data[4] << 4) | (acc_data[5] >> 4)) - OFFSET);

	xyz[0] = ((ud9->pdata->negate_x) ? (-hw_d[ud9->pdata->axis_map_x])
		  : (hw_d[ud9->pdata->axis_map_x]));
	xyz[1] = ((ud9->pdata->negate_y) ? (-hw_d[ud9->pdata->axis_map_y])
		  : (hw_d[ud9->pdata->axis_map_y]));
	xyz[2] = ((ud9->pdata->negate_z) ? (-hw_d[ud9->pdata->axis_map_z])
		  : (hw_d[ud9->pdata->axis_map_z]));

	mg[0] = xyz[0] * 1000 / (ud9->sensitivity);
	mg[1] = xyz[1] * 1000 / (ud9->sensitivity);
	mg[2] = xyz[2] * 1000 / (ud9->sensitivity);

	sprintf(buf, "%d %d %d",mg[0], mg[1], mg[2]);

	#if defined DEBUG && DEBUG == 1
	dev_info(&ud9->client->dev, "%s: [%5d] [%5d] [%5d]\n", __FUNCTION__, mg[0], mg[1], mg[2]);
	#endif

	return err;
}

static int kxud9_open(struct inode *inode, struct file *file)
{
	int ret = -1;

	if(kxud9_enable() < 0)
		return ret;

	atomic_inc(&kxud9_dev_open_count);

	#if defined DEBUG && DEBUG == 1
	dev_info(&ud9->client->dev, "%s: opened %d times\n",\
			__FUNCTION__, atomic_read(&kxud9_dev_open_count));
	#endif

	return 0;
}

static int kxud9_release(struct inode *inode, struct file *file)
{
	int open_count;

	atomic_dec(&kxud9_dev_open_count);
	open_count = (int)atomic_read(&kxud9_dev_open_count);

	if(open_count == 0)
		kxud9_disable();

	#if defined DEBUG && DEBUG == 1
	dev_info(&ud9->client->dev, "%s: opened %d times\n",\
			__FUNCTION__, atomic_read(&kxud9_dev_open_count));
	#endif

	return 0;
}

static long kxud9_ioctl(struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	char buffer[IOCTL_BUFFER_SIZE];
	void __user *data;
	u8 reg_buffer = 0x00;
	int retval=0, val_int=0;
	short val_short=0;

	switch (cmd) {
		case KXUD9_IOCTL_GET_COUNT:
			data = (void __user *) arg;
			if(data == NULL){
				retval = -EFAULT;
				goto err_out;
			}
			retval = kxud9_get_count(buffer, sizeof(buffer));
			if(retval < 0)
				goto err_out;

			if(copy_to_user(data, buffer, sizeof(buffer))) {
				retval = -EFAULT;
				goto err_out;
			}
			break;

		case KXUD9_IOCTL_GET_MG:
			data = (void __user *) arg;
			if(data == NULL){
				retval = -EFAULT;
				goto err_out;
			}
			retval = kxud9_get_mg(buffer, sizeof(buffer));
			if(retval < 0)
				goto err_out;

			if(copy_to_user(data, buffer, sizeof(buffer))) {
				retval = -EFAULT;
				goto err_out;
			}
			break;

		case KXUD9_IOCTL_ENABLE_OUTPUT:
			retval = kxud9_enable();
			if(retval < 0)
				goto err_out;
			break;

		case KXUD9_IOCTL_DISABLE_OUTPUT:
			retval = kxud9_disable();
			if(retval < 0)
				goto err_out;
			break;

		case KXUD9_IOCTL_GET_ENABLE:
			data = (void __user *) arg;
			if(data == NULL){
				retval = -EFAULT;
				goto err_out;
			}
			retval = kxud9_get_bits(CTRL_REGB, &reg_buffer, ENABLE);
			if(retval < 0)
				goto err_out;

			val_short = (short)reg_buffer;

			if(copy_to_user(data, &val_short, sizeof(val_short))) {
				retval = -EFAULT;
				goto err_out;
			}
			break;

		case KXUD9_IOCTL_RESET:
			retval = kxud9_reset();
			if(retval < 0)
				goto err_out;
			break;

		case KXUD9_IOCTL_UPDATE_ODR:
			data = (void __user *) arg;
			if(data == NULL){
				retval = -EFAULT;
				goto err_out;
			}
			if(copy_from_user(&val_int, data, sizeof(val_int))) {
				retval = -EFAULT;
				goto err_out;
			}

			mutex_lock(&ud9->lock);
			ud9->res_interval = max(val_int, ud9->pdata->min_interval);
			mutex_unlock(&ud9->lock);

			retval = kxud9_update_odr(ud9->res_interval);
			if(retval < 0)
				goto err_out;
			break;

		default:
			retval = -ENOIOCTLCMD;
			break;
	}

err_out:
	return retval;
}

static struct file_operations kxud9_fops = {
	.owner = THIS_MODULE,
	.open = kxud9_open,
	.release = kxud9_release,
	.unlocked_ioctl = kxud9_ioctl,
	.compat_ioctl = kxud9_ioctl,
};

static struct miscdevice kxud9_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = NAME_DEV,
	.fops = &kxud9_fops,
};

static int __devinit kxud9_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	int err = -1;
	ud9 = kzalloc(sizeof(*ud9), GFP_KERNEL);
	if(ud9 == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		err = -ENOMEM;
		goto err0;
	}
	if(client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		goto err0;
	}
	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto err0;
	}
	mutex_init(&ud9->lock);
	mutex_lock(&ud9->lock);
	ud9->client = client;
	i2c_set_clientdata(client, ud9);

	ud9->pdata = kmalloc(sizeof(*ud9->pdata), GFP_KERNEL);
	if(ud9->pdata == NULL)
		goto err1;

	err = sysfs_create_group(&client->dev.kobj, &kxud9_attribute_group);
	if(err)
		goto err1;

	memcpy(ud9->pdata, client->dev.platform_data, sizeof(*ud9->pdata));
	if(ud9->pdata->init) {
		err = ud9->pdata->init();
		if(err < 0)
			goto err2;
	}

	memset(ud9->resume, 0, ARRAY_SIZE(ud9->resume));
	ud9->resume[RES_CTRL_REGC]	= ud9->pdata->ctrl_regc_init;
	ud9->resume[RES_CTRL_REGB]	= ud9->pdata->ctrl_regb_init | ENABLE;
	ud9->res_interval		= ud9->pdata->poll_interval;

	err = kxud9_device_power_on();
	if(err < 0)
		goto err3;
	atomic_set(&ud9->enabled, 1);

	err = kxud9_update_g_range(ud9->pdata->g_range);
	if(err < 0) {
		dev_err(&client->dev, "update_g_range failed\n");
		goto err4;
	}

	err = kxud9_update_odr(ud9->res_interval);
	if(err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto err4;
	}

	err = kxud9_input_init();
	if(err < 0)
		goto err4;

	err = misc_register(&kxud9_device);
	if(err) {
		dev_err(&client->dev, "misc. device failed to register.\n");
		goto err5;
	}

	dev_info(&client->dev, "Registered %s\n", DIR_DEV);

	kxud9_device_power_off();
	atomic_set(&ud9->enabled, 0);

	mutex_unlock(&ud9->lock);

	return 0;

err5:
	kxud9_input_cleanup();
err4:
	kxud9_device_power_off();
err3:
	if(ud9->pdata->exit)
		ud9->pdata->exit();
err2:
	kfree(ud9->pdata);
	sysfs_remove_group(&client->dev.kobj, &kxud9_attribute_group);
err1:
	mutex_unlock(&ud9->lock);
	kfree(ud9);
err0:
	return err;
}

static int __devexit kxud9_remove(struct i2c_client *client)
{
	kxud9_input_cleanup();
	misc_deregister(&kxud9_device);
	kxud9_device_power_off();
	if(ud9->pdata->exit)
		ud9->pdata->exit();
	kfree(ud9->pdata);
	sysfs_remove_group(&client->dev.kobj, &kxud9_attribute_group);
	kfree(ud9);

	return 0;
}

#ifdef CONFIG_PM
static int kxud9_resume(struct i2c_client *client)
{
	return kxud9_enable();
}

static int kxud9_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return kxud9_disable();
}
#endif

static const struct i2c_device_id kxud9_id[] = {
	{NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, kxud9_id);

static struct i2c_driver kxud9_driver = {
	.driver = {
		   .name = NAME,
		   },
	.probe = kxud9_probe,
	.remove = __devexit_p(kxud9_remove),
	.resume = kxud9_resume,
	.suspend = kxud9_suspend,
	.id_table = kxud9_id,
};

static int __init kxud9_init(void)
{
	atomic_set(&kxud9_dev_open_count, 0);

	return i2c_add_driver(&kxud9_driver);
}

static void __exit kxud9_exit(void)
{
	atomic_set(&kxud9_dev_open_count, 0);

	i2c_del_driver(&kxud9_driver);
}

module_init(kxud9_init);
module_exit(kxud9_exit);

MODULE_DESCRIPTION("KXUD9 accelerometer driver");
MODULE_AUTHOR("Kuching Tan <kuchingtan@kionix.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION_DEV);
