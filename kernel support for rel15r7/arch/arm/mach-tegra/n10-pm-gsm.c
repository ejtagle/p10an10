/*
 * arch/arm/mach-tegra/n10-pm-gsm.c
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

/* GSM/UMTS power control via GPIO */
  
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <asm/mach-types.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/random.h>
#include <linux/jiffies.h>
#include <linux/rfkill.h>
#include <linux/workqueue.h>

#include "board-n10.h"
#include "gpio-names.h"

#include <mach/hardware.h>
#include <asm/mach-types.h>

struct n10_pm_gsm_data {
	struct rfkill *rfkill;
	struct work_struct work;			/* status change */
	struct workqueue_struct *work_queue; 
	 
#ifdef CONFIG_PM
	int pre_resume_state;
	int keep_on_in_suspend;
#endif
	volatile int powered_up;
};


static void gsm_work_func(struct work_struct *work)
{
	struct n10_pm_gsm_data *gsm_data = container_of(work, struct n10_pm_gsm_data, work);

	if (gsm_data->powered_up) {

		pr_debug("powering ON WWLAN ...\n");
	
		/* 3G power on sequence */
		gpio_direction_output(N10_3G_ON,0);
		msleep(5000);
		
		gpio_direction_output(N10_3G_BULK_TEGRA,1);
		msleep(600); 
		gpio_direction_output(N10_3G_BULK_TEGRA,0); 
		msleep(5000); 
		
		gpio_direction_output(N10_3G_ON,1);
		msleep(5000); 
		gpio_direction_output(N10_3G_ON,0);
		msleep(5000);
		
		gpio_direction_output(N10_3G_BULK_TEGRA,1);
		msleep(600); 
		gpio_direction_output(N10_3G_BULK_TEGRA,0); 
		msleep(5000); 
		
		pr_debug("WWLAN powered ON\n");
	} else {

		gpio_direction_output(N10_3G_ON,1);
		gpio_direction_output(N10_3G_BULK_TEGRA,1); 

		pr_debug("WWLAN powered OFF\n");
	}
}   

/* Power control */
static void __n10_pm_gsm_toggle_radio(struct device *dev, unsigned int on) /*2.2ok*/
{
	struct n10_pm_gsm_data *gsm_data = dev_get_drvdata(dev);

	/* Avoid turning it on or off if already in that state */
	if (gsm_data->powered_up == on)
		return;
		
	/* store new state */
	gsm_data->powered_up = on;
	
	/* Queue a power up/down operation */
	queue_work(gsm_data->work_queue, &gsm_data->work);
}

/* rfkill */
static int gsm_rfkill_set_block(void *data, bool blocked)
{
	struct device *dev = data;
	dev_dbg(dev, "blocked %d\n", blocked);

	__n10_pm_gsm_toggle_radio(dev, !blocked);

	return 0;
}

static const struct rfkill_ops n10_gsm_rfkill_ops = {
    .set_block = gsm_rfkill_set_block,
};

static ssize_t gsm_read(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	int ret = 0;
	struct n10_pm_gsm_data *gsm_data = dev_get_drvdata(dev);
	
	if (!strcmp(attr->attr.name, "power_on")) {
		ret = gsm_data->powered_up;
	} else if (!strcmp(attr->attr.name, "reset")) {
		ret = !gsm_data->powered_up;
	}
#ifdef CONFIG_PM
	else if (!strcmp(attr->attr.name, "keep_on_in_suspend")) {
		ret = gsm_data->keep_on_in_suspend;
	}
#endif 	

	return strlcpy(buf, (!ret) ? "0\n" : "1\n", 3);
}

static ssize_t gsm_write(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	unsigned long on = simple_strtoul(buf, NULL, 10);
	struct n10_pm_gsm_data *gsm_data = dev_get_drvdata(dev);

	if (!strcmp(attr->attr.name, "power_on")) {
	
		rfkill_set_sw_state(gsm_data->rfkill, !on); /* here it receives the blocked state */
		__n10_pm_gsm_toggle_radio(dev, !!on);
		
	} else if (!strcmp(attr->attr.name, "reset")) {
	
		/* reset is low-active, so we need to invert */
		rfkill_set_sw_state(gsm_data->rfkill, !!on); /* here it receives the blocked state */
		__n10_pm_gsm_toggle_radio(dev, !on);
		
	}
#ifdef CONFIG_PM
	else if (!strcmp(attr->attr.name, "keep_on_in_suspend")) {
		gsm_data->keep_on_in_suspend = on;
	}
#endif 

	return count;
}

static DEVICE_ATTR(power_on, 0666, gsm_read, gsm_write);
static DEVICE_ATTR(reset, 0666, gsm_read, gsm_write);
#ifdef CONFIG_PM
static DEVICE_ATTR(keep_on_in_suspend, 0666, gsm_read, gsm_write);
#endif


#ifdef CONFIG_PM
static int n10_gsm_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct n10_pm_gsm_data *gsm_data = dev_get_drvdata(&pdev->dev);

	dev_dbg(&pdev->dev, "suspending\n");

	gsm_data->pre_resume_state = gsm_data->powered_up;
	if (!gsm_data->keep_on_in_suspend)
		__n10_pm_gsm_toggle_radio(&pdev->dev, 0);
	else
		dev_warn(&pdev->dev, "keeping GSM ON during suspend\n");
		
	return 0;
}

static int n10_gsm_resume(struct platform_device *pdev)
{
	struct n10_pm_gsm_data *gsm_data = dev_get_drvdata(&pdev->dev);
	dev_dbg(&pdev->dev, "resuming\n");

	__n10_pm_gsm_toggle_radio(&pdev->dev, gsm_data->pre_resume_state);
	return 0;
}
#else
#define n10_gsm_suspend	NULL
#define n10_gsm_resume		NULL
#endif

static struct attribute *n10_gsm_sysfs_entries[] = {
	&dev_attr_power_on.attr,
	&dev_attr_reset.attr,
#ifdef CONFIG_PM	
	&dev_attr_keep_on_in_suspend.attr,
#endif
	NULL
};

static struct attribute_group n10_gsm_attr_group = {
	.name	= NULL,
	.attrs	= n10_gsm_sysfs_entries,
};

static int __init n10_gsm_probe(struct platform_device *pdev)
{
	/* default-on */
	const int default_blocked_state = 0;

	struct rfkill *rfkill;
	struct n10_pm_gsm_data *gsm_data;
	int ret;

	gsm_data = kzalloc(sizeof(*gsm_data), GFP_KERNEL);
	if (!gsm_data) {
		dev_err(&pdev->dev, "no memory for context\n");
		return -ENOMEM;
	}
	dev_set_drvdata(&pdev->dev, gsm_data);
	
	/* Keep GSM on in suspend by default */
	gsm_data->keep_on_in_suspend = 1; 

	gpio_request(N10_3G_ON,"3g_on");
	gpio_request(N10_3G_BULK_TEGRA,"3g_bulk_tegra");
	gpio_request(N10_3G_BOOST_TEGRA_N,"3g_boost_tegra_n");
	gpio_request(N10_3G_WAKE_DET,"3g_wake_det");
	
	gpio_direction_input(N10_3G_BOOST_TEGRA_N);
	gpio_direction_input(N10_3G_WAKE_DET);
	
	/* Turn it on ... */
	gpio_direction_output(N10_3G_ON,1);
	msleep(600);
	gpio_set_value(N10_3G_ON,0);
	
	gsm_data->work_queue = create_singlethread_workqueue("gsm_power_wq");
	if (!gsm_data->work_queue) {
		kfree(gsm_data);
		dev_err(&pdev->dev, "could not create workqueue\n");
		return -ENOMEM;
	}
	INIT_WORK(&gsm_data->work, gsm_work_func); 	
	
	/* register rfkill interface */
	rfkill = rfkill_alloc(pdev->name, &pdev->dev, RFKILL_TYPE_WWAN,
                            &n10_gsm_rfkill_ops, &pdev->dev);

	if (!rfkill) {
		dev_err(&pdev->dev, "Failed to allocate rfkill\n");
		destroy_workqueue(gsm_data->work_queue);
		gpio_free(N10_3G_ON);
		gpio_free(N10_3G_BULK_TEGRA);
		kfree(gsm_data);
		dev_set_drvdata(&pdev->dev, NULL);
		return -ENOMEM;
	}
	gsm_data->rfkill = rfkill;

	/* Set the default state */
	rfkill_init_sw_state(rfkill, default_blocked_state);
	__n10_pm_gsm_toggle_radio(&pdev->dev, !default_blocked_state);

	ret = rfkill_register(rfkill);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register rfkill\n");
		rfkill_destroy(rfkill);		
		gpio_free(N10_3G_ON);
		gpio_free(N10_3G_BULK_TEGRA);
		kfree(gsm_data);
		dev_set_drvdata(&pdev->dev, NULL);
		return ret;
	}

	dev_info(&pdev->dev, "GSM/UMTS RFKill driver loaded\n");
	
	return sysfs_create_group(&pdev->dev.kobj, &n10_gsm_attr_group);
}

static int n10_gsm_remove(struct platform_device *pdev)
{
	struct n10_pm_gsm_data *gsm_data = dev_get_drvdata(&pdev->dev);
	if (!gsm_data)
		return 0;
	
	sysfs_remove_group(&pdev->dev.kobj, &n10_gsm_attr_group);

	rfkill_unregister(gsm_data->rfkill);
	rfkill_destroy(gsm_data->rfkill);

	__n10_pm_gsm_toggle_radio(&pdev->dev, 0);
	
	cancel_work_sync(&gsm_data->work);
	destroy_workqueue(gsm_data->work_queue);
	gpio_free(N10_3G_ON);
	gpio_free(N10_3G_BULK_TEGRA);

	kfree(gsm_data);
	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}
static struct platform_driver n10_gsm_driver = {
	.probe		= n10_gsm_probe,
	.remove		= n10_gsm_remove,
	.suspend	= n10_gsm_suspend,
	.resume		= n10_gsm_resume,
	.driver		= {
		.name		= "n10-pm-gsm",
	},
};

static int __devinit n10_gsm_init(void)
{
	return platform_driver_register(&n10_gsm_driver);
}

static void n10_gsm_exit(void)
{
	platform_driver_unregister(&n10_gsm_driver);
}

module_init(n10_gsm_init);
module_exit(n10_gsm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eduardo José Tagle <ejtagle@tutopia.com>");
MODULE_DESCRIPTION("n10 GSM power management");
