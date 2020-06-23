/*
 * arch/arm/mach-tegra/n10-vibrator.c
 *
 * Copyright (C) 2012 Eduardo José Tagle <ejtagle@tutopia.com>
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <asm/mach-types.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <linux/if.h>
#include <linux/skbuff.h>
#include <linux/random.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/rfkill.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>

#include "board-n10.h"
#include "gpio-names.h"
#include <../drivers/staging/android/timed_output.h>

static int s_Timeout = 0;

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	s_Timeout = value;
	if (value) {
		gpio_set_value(N10_SHAKE_ON,1);
		msleep(value);
		gpio_set_value(N10_SHAKE_ON,0);
	} else {
		gpio_set_value(N10_SHAKE_ON,0);
	}
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	return s_Timeout;
}

static struct timed_output_dev tegra_vibrator = {
	.name		= "vibrator",
	.get_time	= vibrator_get_time,
	.enable		= vibrator_enable,
};

/* ----- Initialization/removal -------------------------------------------- */
static int __init n10_vibrator_probe(struct platform_device *pdev)
{
	int status;

	s_Timeout = 0;
	
	gpio_request(N10_SHAKE_ON, "vibrator");
	gpio_direction_output(N10_SHAKE_ON, 0);

	gpio_request(N10_VIBRATE_DET, "vibrate detector");
	gpio_direction_input(N10_VIBRATE_DET);
	
	status = timed_output_dev_register(&tegra_vibrator);
	return status;
}

static int n10_vibrator_remove(struct platform_device *pdev)
{
	timed_output_dev_unregister(&tegra_vibrator);
	
	gpio_free(N10_SHAKE_ON);
	
	return 0;
}

static struct platform_driver n10_vibrator_driver = {
	.probe		= n10_vibrator_probe,
	.remove		= n10_vibrator_remove,
	.driver		= {
		.name		= "n10-vibrator",
	},
};

static int __devinit n10_vibrator_init(void)
{
	gpio_request(N10_VIBRATE_DET,"Vibrate det");
	gpio_direction_input(N10_VIBRATE_DET);


	return platform_driver_register(&n10_vibrator_driver);
}

static void n10_vibrator_exit(void)
{
	platform_driver_unregister(&n10_vibrator_driver);
}

module_init(n10_vibrator_init);
module_exit(n10_vibrator_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eduardo José Tagle <ejtagle@tutopia.com>");
MODULE_DESCRIPTION("n10 Vibrator");
