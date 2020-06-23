/*
 * arch/arm/mach-tegra/board-n10-wlan.c
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
 
#define DEBUG

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
#include <linux/wlan_plat.h>
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

static struct clk *wifi_32k_clk = NULL;
static int n10_wifi_power(int on) /*ok*/
{
	pr_debug("%s: %d\n", __func__, on);

	gpio_set_value(N10_WLAN_WF_RST_PWDN_N, on);
	msleep(200);

	if (on)
		clk_enable(wifi_32k_clk);
	else
		clk_disable(wifi_32k_clk); 	

	return 0;
}

static int n10_wifi_reset(int on)
{
	pr_debug("%s: %d\n", __func__, on);
	return 0;
} 

#ifdef CONFIG_TEGRA_PREPOWER_WIFI
static int __init n10_wifi_prepower(void)
{
	n10_wifi_power(1);
	return 0;
}

subsys_initcall_sync(n10_wifi_prepower);
#endif
 
static struct wifi_platform_data n10_wifi_control = { /*ok*/
	.set_power      = n10_wifi_power,
	.set_reset      = n10_wifi_reset,
	.set_carddetect = n10_wifi_set_carddetect,
};

static struct resource wifi_resource[] = {
	[0] = {
		.name  = "bcm4329_wlan_irq",
		.start = TEGRA_GPIO_TO_IRQ(N10_WLAN_WL_HOST_WAKE),
		.end   = TEGRA_GPIO_TO_IRQ(N10_WLAN_WL_HOST_WAKE),
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE,
	},
}; 

static struct platform_device n10_wifi_device = { /*ok*/
	.name           = "bcmdhd_wlan",
	.id             = 1,
	.num_resources	= 1,
	.resource		= wifi_resource,
	.dev            = {
		.platform_data = &n10_wifi_control,
	},
}; 

int __init n10_wlan_pm_register_devices(void)
{

	wifi_32k_clk = clk_get_sys(NULL, "blink");
	if (IS_ERR(wifi_32k_clk)) {
		pr_err("%s: unable to get blink clock\n", __func__);
		return PTR_ERR(wifi_32k_clk);
	}
		
	gpio_request(N10_WLAN_WF_RST_PWDN_N, "wlan_reset_pwdn_n");
	gpio_request(N10_WLAN_WL_HOST_WAKE, "wl_host_wake");
	
	gpio_direction_output(N10_WLAN_WF_RST_PWDN_N, 0);
	gpio_direction_input(N10_WLAN_WL_HOST_WAKE);	

	platform_device_register(&n10_wifi_device);

	device_init_wakeup(&n10_wifi_device.dev, 1);
	device_set_wakeup_enable(&n10_wifi_device.dev, 0);

	return 0;
}