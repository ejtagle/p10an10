/*
 * drivers/rtc/rtc-shmcu.c
 *
 * RTC driver for shmcu
 *
 * Copyright (c) 2012, Eduardo José Tagle <ejtagle@tutopia.com>
 * Copyright (c) 2010, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

//#define DEBUG

#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/shmcu.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/slab.h>

struct shmcu_rtc {
	struct rtc_device	*rtc;
};

static inline struct device *to_shmcu_dev(struct device *dev)
{
	return dev->parent;
}

static int shmcu_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct device *shmcu_dev = to_shmcu_dev(dev);
	u8 buff[7];
	
	memset(buff,0,sizeof(buff));
	shmcu_read(shmcu_dev, 2, buff);

	tm->tm_year = (buff[0] * 100) + buff[1];
	tm->tm_mon  =  buff[2] - 1;
	tm->tm_mday =  buff[3];
	tm->tm_hour =  buff[4];
	tm->tm_min  =  buff[5];
	tm->tm_sec  =  buff[6];

	return rtc_valid_tm(tm);
}

static int shmcu_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct device *shmcu_dev = to_shmcu_dev(dev);
	u8 buff[7];
	
	buff[0] = tm->tm_year / 100;
	buff[1] = tm->tm_year % 100;
	buff[2] = tm->tm_mon + 1;
	buff[3] = tm->tm_mday;
	buff[4] = tm->tm_hour;
	buff[5] = tm->tm_min;
	buff[6] = tm->tm_sec;
	
	shmcu_write(shmcu_dev, 3, buff);

	return 0;
}

static const struct rtc_class_ops shmcu_rtc_ops = {
	.read_time	= shmcu_rtc_read_time,
	.set_time	= shmcu_rtc_set_time,
};

static int __devinit shmcu_rtc_probe(struct platform_device *pdev)
{
	struct shmcu_rtc *rtc;
	int err;

	rtc = kzalloc(sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, rtc);

	rtc->rtc = rtc_device_register("shmcu-rtc", &pdev->dev,
				       &shmcu_rtc_ops, THIS_MODULE);

	if (IS_ERR(rtc->rtc)) {
		err = PTR_ERR(rtc->rtc);
		goto fail;
	}

	return 0;

fail:
	if (!IS_ERR_OR_NULL(rtc->rtc))
		rtc_device_unregister(rtc->rtc);
	kfree(rtc);
	return err;
}

static int __devexit shmcu_rtc_remove(struct platform_device *pdev)
{
	struct shmcu_rtc *rtc = dev_get_drvdata(&pdev->dev);

	rtc_device_unregister(rtc->rtc);
	kfree(rtc);
	return 0;
}

static struct platform_driver shmcu_rtc_driver = {
	.driver	= {
		.name	= "shmcu-rtc",
		.owner	= THIS_MODULE,
	},
	.probe	= shmcu_rtc_probe,
	.remove	= __devexit_p(shmcu_rtc_remove),
};

static int __init shmcu_rtc_init(void)
{
	return platform_driver_register(&shmcu_rtc_driver);
}
module_init(shmcu_rtc_init);

static void __exit shmcu_rtc_exit(void)
{
	platform_driver_unregister(&shmcu_rtc_driver);
}
module_exit(shmcu_rtc_exit);

MODULE_DESCRIPTION("TI shmcu RTC driver");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtc-shmcu");
