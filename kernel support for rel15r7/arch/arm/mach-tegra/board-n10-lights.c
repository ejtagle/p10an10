/*
 * arch/arm/mach-tegra/board-n10-lights.c
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

#include <linux/console.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/memblock.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>

#include <mach/io.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/nand.h>
#include <mach/iomap.h>

#include "board.h"
#include "board-n10.h"
#include "clock.h"
#include "gpio-names.h"
#include "devices.h"

static struct gpio_led leds_leds[] = {
	{
		.name = "iconkeys-back",
		.gpio = SHMCU_GPIO0,
		.active_low = 0,
	},
	{
		.name = "camera-torch",
		.gpio = N10_LED_LIGHT,
		.active_low = 0,
	}
};

static struct gpio_led_platform_data leds_led_data = {
	.num_leds =     ARRAY_SIZE(leds_leds),
	.leds =         leds_leds,
};

static struct platform_device leds_gpio_leds = {
	.name =         "leds-gpio",
	.id =           -1,
	.dev = {
		.platform_data = &leds_led_data,
	}
};

static struct platform_device *n10_lights_devices[] __initdata = {
	&leds_gpio_leds,
};

int __init n10_lights_register_devices(void)
{

	gpio_request(N10_NEW_MIPI_LED_ON,"New MIPI LED On");
	gpio_direction_input(N10_NEW_MIPI_LED_ON);

	return platform_add_devices(n10_lights_devices, ARRAY_SIZE(n10_lights_devices));
}