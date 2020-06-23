/* OK */
/*
 * arch/arm/mach-tegra/board-n10-keyboard.c
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

#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>

#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <asm/io.h>

#include <mach/io.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/gpio.h>
#include <mach/system.h>

#include "board-n10.h"
#include "gpio-names.h"
#include "devices.h" 
#include "pm.h"
#include "wakeups-t2.h"

#ifdef CONFIG_KEYBOARD_GPIO
static struct gpio_keys_button n10_keys[] = {
	[0] = {
		.gpio = N10_KEY_VOLUMEUP,
		.active_low = true,
		.debounce_interval = 50,
		.wakeup = false,		
		.code = KEY_VOLUMEUP,
		.type = EV_KEY,		
		.desc = "volume up",
	},
	[1] = {
		.gpio = N10_KEY_VOLUMEDOWN,
		.active_low = true,
		.debounce_interval = 50,
		.wakeup = false,		
		.code = KEY_VOLUMEDOWN,
		.type = EV_KEY,		
		.desc = "volume down",
	},
	[2] = {
		.gpio = N10_KEY_POWER,
		.active_low = true,
		.debounce_interval = 150,
		.wakeup = true,		
		.code = KEY_POWER,
		.type = EV_KEY,		
		.desc = "power",
	},
	[3] = {
		.gpio = N10_KEY_FIND,
		.active_low = true,
		.debounce_interval = 50,
		.wakeup = false,		
		.code = KEY_FIND,
		.type = EV_KEY,		
		.desc = "find",
	},
	[4] = {
		.gpio = N10_KEY_HOME,
		.active_low = true,
		.debounce_interval = 50,
		.wakeup = false,		
		.code = KEY_HOME,
		.type = EV_KEY,		
		.desc = "home",
	},
	[5] = {
		.gpio = N10_KEY_BACK,
		.active_low = true,
		.debounce_interval = 50,
		.wakeup = false,		
		.code = KEY_BACK,
		.type = EV_KEY,		
		.desc = "back",
	},
	[6] = {
		.gpio = N10_KEY_MENU,
		.active_low = true,
		.debounce_interval = 50,
		.wakeup = false,		
		.code = KEY_MENU,
		.type = EV_KEY,		
		.desc = "menu",
	},
};

#define PMC_WAKE_STATUS 0x14

static int n10_wakeup_key(void)
{
	unsigned long status =
		readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_WAKE_STATUS);

	return status & TEGRA_WAKE_GPIO_PV2 ? KEY_POWER : KEY_RESERVED;
}  

static struct gpio_keys_platform_data n10_keys_platform_data = {
	.buttons 	= n10_keys,
	.nbuttons 	= ARRAY_SIZE(n10_keys),
	.wakeup_key	= n10_wakeup_key,
};

static struct platform_device n10_keys_device = {
	.name 		= "gpio-keys",
	.id 		= 0,
	.dev		= {
		.platform_data = &n10_keys_platform_data,
	},
};

static struct platform_device *n10_key_devices[] __initdata = {
	&n10_keys_device,
};

/* Register all keyboard devices */
int __init n10_keyboard_register_devices(void)
{
	return platform_add_devices(n10_key_devices, ARRAY_SIZE(n10_key_devices));
}

#else
int __init n10_keyboard_register_devices(void)
{
	return 0;
}
#endif


