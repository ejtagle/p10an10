/*
 * GPS low power control via GPIO
 *
 * Copyright (C) 2011 Eduardo José Tagle <ejtagle@tutopia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */ 

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include "board-n10.h"
#include "gpio-names.h"


struct n10_pm_gps_data {
#ifdef CONFIG_PM
	int pre_resume_state;
	int keep_on_in_suspend;
#endif
	int powered_up;
};

/* Power control */
static void __n10_pm_gps_toggle_radio(struct device *dev, unsigned int on)
{
	struct n10_pm_gps_data *gps_data = dev_get_drvdata(dev);

	/* Avoid turning it on or off if already in that state */
	if (gps_data->powered_up == on)
		return;
	
	if (on) {
	
		/* GPS power on sequence */
		gpio_set_value(N10_GPS_RESET_N, 0); /* Reset */
		msleep(300);
		gpio_set_value(N10_GPS_RESET_N, 1); /* Reset off */
		msleep(1);

	} else {
	
		gpio_set_value(N10_GPS_RESET_N, 0); /* Reset */
				
	}
	
	/* store new state */
	gps_data->powered_up = on;
}


static ssize_t n10_gps_read(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct n10_pm_gps_data *gps_data = dev_get_drvdata(dev);
	int ret = 0;

	if (!strcmp(attr->attr.name, "power_on") ||
	    !strcmp(attr->attr.name, "pwron")) {
		ret = gps_data->powered_up;
	}
#ifdef CONFIG_PM
	else if (!strcmp(attr->attr.name, "keep_on_in_suspend")) {
		ret = gps_data->keep_on_in_suspend;
	}
#endif

	return strlcpy(buf, (!ret) ? "0\n" : "1\n", 3);
}

static ssize_t n10_gps_write(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct n10_pm_gps_data *gps_data = dev_get_drvdata(dev);
	unsigned long on = simple_strtoul(buf, NULL, 10);

	if (!strcmp(attr->attr.name, "power_on") ||
	    !strcmp(attr->attr.name, "pwron")) {
		__n10_pm_gps_toggle_radio(dev,on);
	}
#ifdef CONFIG_PM
	else if (!strcmp(attr->attr.name, "keep_on_in_suspend")) {
		gps_data->keep_on_in_suspend = on;
	}
#endif

	return count;
}

static DEVICE_ATTR(power_on, 0666, n10_gps_read, n10_gps_write);
static DEVICE_ATTR(pwron, 0666, n10_gps_read, n10_gps_write);
#ifdef CONFIG_PM
static DEVICE_ATTR(keep_on_in_suspend, 0666, n10_gps_read, n10_gps_write);
#endif


#ifdef CONFIG_PM
static int n10_pm_gps_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	struct n10_pm_gps_data *gps_data = dev_get_drvdata(&pdev->dev);
	
	gps_data->pre_resume_state = gps_data->powered_up;
	if (!gps_data->keep_on_in_suspend)
		__n10_pm_gps_toggle_radio(&pdev->dev,0);
	else
		dev_warn(&pdev->dev, "keeping gps ON during suspend\n");
	return 0;
}

static int n10_pm_gps_resume(struct platform_device *pdev)
{
	struct n10_pm_gps_data *gps_data = dev_get_drvdata(&pdev->dev);
	__n10_pm_gps_toggle_radio(&pdev->dev,gps_data->pre_resume_state);
	return 0;
}

#else
#define n10_pm_gps_suspend	NULL
#define n10_pm_gps_resume	NULL
#endif


static struct attribute *n10_gps_sysfs_entries[] = {
	&dev_attr_power_on.attr,
	&dev_attr_pwron.attr,
#ifdef CONFIG_PM
	&dev_attr_keep_on_in_suspend.attr,
#endif
	NULL
};

static struct attribute_group n10_gps_attr_group = {
	.name	= NULL,
	.attrs	= n10_gps_sysfs_entries,
};

static int __init n10_pm_gps_probe(struct platform_device *pdev)
{
	/* start with gps enabled */
	int default_state = 1;
	
	struct n10_pm_gps_data *gps_data;
	
	gps_data = kzalloc(sizeof(*gps_data), GFP_KERNEL);
	if (!gps_data) {
		dev_err(&pdev->dev, "no memory for context\n");
		return -ENOMEM;
	}
	dev_set_drvdata(&pdev->dev, gps_data);
	gps_data->keep_on_in_suspend = 1; // We don't want to reset the gps on suspend

	gpio_request(N10_GPS_RESET_N, "gps_reset");
	gpio_direction_output(N10_GPS_RESET_N, 0);
	gpio_request(N10_GPS_DET, "gps_in");
	gpio_direction_input(N10_GPS_DET);

	/* Set the default state */
	__n10_pm_gps_toggle_radio(&pdev->dev, default_state);
	
	dev_info(&pdev->dev, "GPS power management driver loaded\n");
	
	return sysfs_create_group(&pdev->dev.kobj,
				  &n10_gps_attr_group);
}

static int n10_pm_gps_remove(struct platform_device *pdev)
{
	struct n10_pm_gps_data *gps_data = dev_get_drvdata(&pdev->dev);
	if (!gps_data)
		return 0;
	
	sysfs_remove_group(&pdev->dev.kobj, &n10_gps_attr_group);
	
	__n10_pm_gps_toggle_radio(&pdev->dev, 0);

	gpio_free(N10_GPS_DET);
	gpio_free(N10_GPS_RESET_N);

	kfree(gps_data);
	dev_set_drvdata(&pdev->dev, NULL);
	
	return 0;
}

static struct platform_driver n10_pm_gps_driver = {
	.probe		= n10_pm_gps_probe,
	.remove		= n10_pm_gps_remove,
	.suspend	= n10_pm_gps_suspend,
	.resume		= n10_pm_gps_resume,
	.driver		= {
		.name		= "n10-pm-gps",
	},
};

static int __devinit n10_pm_gps_init(void)
{
	return platform_driver_register(&n10_pm_gps_driver);
}

static void n10_pm_gps_exit(void)
{
	platform_driver_unregister(&n10_pm_gps_driver);
}

module_init(n10_pm_gps_init);
module_exit(n10_pm_gps_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eduardo José Tagle <ejtagle@tutopia.com>");
MODULE_DESCRIPTION("n10 3G / GPS power management");
