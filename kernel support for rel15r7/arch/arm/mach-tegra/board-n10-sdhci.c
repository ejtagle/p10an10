/*
 * arch/arm/mach-tegra/board-n10-sdhci.c
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
#include <linux/version.h>
#include <linux/err.h>

#include <asm/mach-types.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/sdhci.h>
#include <mach/pinmux.h>

#include "gpio-names.h"
#include "devices.h"
#include "board-n10.h"

/*
  For N10, 
    SDIO0: WLan
	SDIO1: Unused
	SDIO2: SD MMC
	SDIO3: eMMC
 */

static void (*wifi_status_cb)(int card_present, void *dev_id) = NULL;
static void *wifi_status_cb_devid = NULL;

/* 2.6.36+ version has a hook to check card status. Use it */
static int n10_wifi_status_register(
		void (*callback)(int card_present_ignored, void *dev_id),
		void *dev_id)
{
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	return 0;
} 

/* Used to set the virtual CD of wifi adapter */
int n10_wifi_set_carddetect(int val)
{
	pr_debug("%s: %d\n", __func__, val);

	/* Let the SDIO infrastructure know about the change */
	if (wifi_status_cb) {
		wifi_status_cb(val, wifi_status_cb_devid);
	} else
		pr_info("%s: Nobody to notify\n", __func__);

	return 0;
}
EXPORT_SYMBOL_GPL(n10_wifi_set_carddetect);

static struct embedded_sdio_data embedded_sdio_data0 = {
	.cccr   = {
		.sdio_vsn       = 2,
		.multi_block    = 1,
		.low_speed      = 0,
		.wide_bus       = 0,
		.high_power     = 1,
		.high_speed     = 1,
	},
	.cis  = {
		.vendor         = 0x02d0,
		.device         = 0x4329,
	},
};

struct tegra_sdhci_platform_data n10_wifi_data = {
	.mmc_data = {
		.register_status_notify	= n10_wifi_status_register, 
		.embedded_sdio = &embedded_sdio_data0,
		.built_in = 1,
		.ocr_mask = MMC_OCR_1V8_MASK,
	},
	.pm_flags = MMC_PM_KEEP_POWER,
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data3 = {
	.cd_gpio = N10_SDIO2_CD,
	.wp_gpio = N10_SDIO2_WP,
	.power_gpio = N10_SDIO2_POWER, 	
};

/* eMMC */
static struct tegra_sdhci_platform_data tegra_sdhci_platform_data4 = {
	.is_8bit = 1,
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.mmc_data = {
		.built_in = 1,
	} 
};

static struct platform_device *n10_sdhci_devices[] __initdata = {
	&tegra_sdhci_device1,
	/* eMMC should be registered first */
	&tegra_sdhci_device4,	
	&tegra_sdhci_device3,
};

/* Register sdhci devices */
int __init n10_sdhci_register_devices(void)
{
	/* Plug in platform data */
	tegra_sdhci_device1.dev.platform_data = &n10_wifi_data;
	tegra_sdhci_device3.dev.platform_data = &tegra_sdhci_platform_data3;
	tegra_sdhci_device4.dev.platform_data = &tegra_sdhci_platform_data4;

	return platform_add_devices(n10_sdhci_devices, ARRAY_SIZE(n10_sdhci_devices));
}
