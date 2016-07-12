/*
 * (C) 2011-2013 by xboot.org
 * Author: jianjun jiang <jerryjianjun@gmail.com>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <asm/gpio.h>
#include <asm/delay.h>
#include <linux/clk.h>
#include <plat/gpio-cfg.h>

/*
 * BEEP -> PWM1 -> GPD0(1)
 */
static int __x4412_beep_status = 0;

static void __x4412_beep_probe(void)
{
	int ret;

	ret = gpio_request(EXYNOS4_GPD0(1), "GPD0(1)");
	if(ret)
		printk("x4412-beep: request gpio GPD0(1) fail\n");
	s3c_gpio_setpull(EXYNOS4_GPD0(1), S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(EXYNOS4_GPD0(1), S3C_GPIO_SFN(1));
	gpio_set_value(EXYNOS4_GPD0(1), 0);

	__x4412_beep_status = 0;
}

static void __x4412_beep_remove(void)
{
	gpio_free(EXYNOS4_GPD0(1));
}

static ssize_t x4412_beep_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(!strcmp(attr->attr.name, "state"))
	{
		if(__x4412_beep_status != 0)
			return strlcpy(buf, "1\n", 3);
		else
			return strlcpy(buf, "0\n", 3);
	}
	return strlcpy(buf, "\n", 3);
}

static ssize_t x4412_beep_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long on = simple_strtoul(buf, NULL, 10);

	if(!strcmp(attr->attr.name, "state"))
	{
		if(on)
		{
			gpio_set_value(EXYNOS4_GPD0(1), 1);
			__x4412_beep_status = 1;
		}
		else
		{
			gpio_set_value(EXYNOS4_GPD0(1), 0);
			__x4412_beep_status = 0;
		}
	}

	return count;
}

static DEVICE_ATTR(state, 0666, x4412_beep_read, x4412_beep_write);

static struct attribute * x4412_beep_sysfs_entries[] = {
	&dev_attr_state.attr,
	NULL,
};

static struct attribute_group x4412_beep_attr_group = {
	.name	= NULL,
	.attrs	= x4412_beep_sysfs_entries,
};

static int x4412_beep_probe(struct platform_device *pdev)
{
	__x4412_beep_probe();

	return sysfs_create_group(&pdev->dev.kobj, &x4412_beep_attr_group);
}

static int x4412_beep_remove(struct platform_device *pdev)
{
	__x4412_beep_remove();

	sysfs_remove_group(&pdev->dev.kobj, &x4412_beep_attr_group);
	return 0;
}

#ifdef CONFIG_PM
static int x4412_beep_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int x4412_beep_resume(struct platform_device *pdev)
{
	return 0;
}

#else
#define x4412_beep_suspend	NULL
#define x4412_beep_resume	NULL
#endif

static struct platform_driver x4412_beep_driver = {
	.probe		= x4412_beep_probe,
	.remove		= x4412_beep_remove,
	.suspend	= x4412_beep_suspend,
	.resume		= x4412_beep_resume,
	.driver		= {
		.name	= "x4412-beep",
	},
};

static struct platform_device x4412_beep_device = {
	.name      = "x4412-beep",
	.id        = -1,
};

static int __devinit x4412_beep_init(void)
{
	int ret;

	printk("x4412 beep driver\r\n");

	ret = platform_device_register(&x4412_beep_device);
	if(ret)
		printk("failed to register x4412 beep device\n");

	ret = platform_driver_register(&x4412_beep_driver);
	if(ret)
		printk("failed to register x4412 beep driver\n");

	return ret;
}

static void x4412_beep_exit(void)
{
	platform_driver_unregister(&x4412_beep_driver);
}

module_init(x4412_beep_init);
module_exit(x4412_beep_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jianjun jiang <jerryjianjun@gmail.com>");
MODULE_DESCRIPTION("x4412 beep driver");
