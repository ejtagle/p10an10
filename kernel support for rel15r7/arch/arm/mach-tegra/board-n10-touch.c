/*
 * arch/arm/mach-tegra/board-n10-touch.c
 *
 * Copyright (C) 2012 Eduardo José Tagle <ejtagle@tutopia.com>
 * Copyright (C) 2010 Google, Inc.
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
 
#include <linux/resource.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <asm/mach-types.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/pinmux.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/input/zinitix.h>
#include <linux/regulator/consumer.h>

#include "board-n10.h"
#include "gpio-names.h"

struct zinitix_platform_data zinitix_pdata = {
	.power_gpio = N10_TS_OFF_N,		/* power control gpio 	(1=powered) */
	.reset_gpio = N10_TS_RESET,		/* reset control gpio 	(0=reset) */
	.irq_gpio = N10_TS_IRQ,
	.icon_keycode = { 
		KEY_HOME,
		KEY_BACK,
		KEY_MENU
	},
};

static struct i2c_board_info __initdata n10_i2c_bus3_touch_info[] = {
	{
		I2C_BOARD_INFO("zinitix", 0x20),
		.irq = TEGRA_GPIO_TO_IRQ(N10_TS_IRQ),
		.platform_data = &zinitix_pdata,
	},
};

int __init n10_touch_register_devices(void)
{
	int ret;
	struct regulator *ts_ldo6 = NULL;
	
	gpio_request(N10_TS_RESET_STATE, "ts reset state");
	gpio_direction_input(N10_TS_RESET_STATE);
	
	ts_ldo6 = regulator_get(NULL, "vdd_ldo6");
	if (IS_ERR_OR_NULL(ts_ldo6)) {
		pr_err("%s: Couldn't get regulator ldo6\n", __func__);
		goto fail_put_regulator;
	}

	ret = regulator_set_voltage(ts_ldo6, 1800*1000, 1800*1000);
	if (ret){
		pr_err("%s: Failed to set ldo6 to 1.8v\n", __func__);
		goto fail_put_regulator;
	}

	ret = regulator_enable(ts_ldo6);
	if (ret){
		pr_err("%s: Failed to enable ldo6\n", __func__);
		goto fail_put_regulator;
	}

fail_put_regulator:	

	i2c_register_board_info(3, n10_i2c_bus3_touch_info, 1);
	return 0;
}
