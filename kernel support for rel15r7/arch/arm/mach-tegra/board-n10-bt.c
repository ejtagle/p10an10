/*
 * arch/arm/mach-tegra/board-n10-bt.c
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
#include <linux/err.h>

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

#ifdef CONFIG_BCM4329_RFKILL
static struct resource n10_bcm4329_rfkill_resources[] = { /*ok*/
	{
		.name   = "bcm4329_nreset_gpio",
		.start  = N10_BT_RST_SHUTDOWN_N,
		.end    = N10_BT_RST_SHUTDOWN_N,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device n10_bcm4329_rfkill_device = { /*ok*/
	.name = "bcm4329_rfkill",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(n10_bcm4329_rfkill_resources),
	.resource       = n10_bcm4329_rfkill_resources,
};
#endif

#ifdef CONFIG_BT_BLUESLEEP
static struct resource n10_bluesleep_resources[] = {
	[0] = {
		.name = "gpio_host_wake",
			.start  = N10_BT_IRQ_N,
			.end    = N10_BT_IRQ_N,
			.flags  = IORESOURCE_IO,
	},
	[1] = {
		.name = "gpio_ext_wake",
			.start  = N10_BT_WAKEUP,
			.end    = N10_BT_WAKEUP,
			.flags  = IORESOURCE_IO,
	},
	[2] = {
		.name = "host_wake",
			.start  = TEGRA_GPIO_TO_IRQ(N10_BT_IRQ_N),
			.end    = TEGRA_GPIO_TO_IRQ(N10_BT_IRQ_N),
			.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE,
	},
};

static struct platform_device n10_bluesleep_device = {
	.name           = "bluesleep",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(n10_bluesleep_resources),
	.resource       = n10_bluesleep_resources,
}; 

#endif 

static struct platform_device *n10_bt_pm_devices[] __initdata = {
#ifdef CONFIG_BCM4329_RFKILL
	&n10_bcm4329_rfkill_device,
#endif
#ifdef CONFIG_BT_BLUESLEEP
	&n10_bluesleep_device,
#endif
};

#ifdef CONFIG_BT_BLUESLEEP
extern void bluesleep_setup_uart_port(struct platform_device *uart_dev);
#endif

int __init n10_bt_pm_register_devices(void)
{
	int ret;
#ifdef CONFIG_BCM4329_RFKILL
	/* Add Clock Resource */
	clk_add_alias("bcm4329_32k_clk", n10_bcm4329_rfkill_device.name, \
				"blink", NULL);
#endif

#if defined(CONFIG_BT_BLUESLEEP) || defined(CONFIG_BCM4329_RFKILL)
	ret = platform_add_devices(n10_bt_pm_devices, ARRAY_SIZE(n10_bt_pm_devices));
	
#ifdef CONFIG_BT_BLUESLEEP	
	bluesleep_setup_uart_port(&tegra_uartc_device);
#endif
	return ret;
	
#else
	return 0;
#endif
}