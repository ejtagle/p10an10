/*
 * arch/arm/mach-tegra/board-n10-eeprom.c
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
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/i2c.h>
#include <linux/version.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>
#include <asm/io.h>

#include <mach/io.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/system.h>

#include "board.h"
#include "board-n10.h"
#include "gpio-names.h"
#include "devices.h"

static struct i2c_board_info __initdata n10_i2c_bus4_board_info[] = {
	{
		I2C_BOARD_INFO("eeprom", 0x50),
	},
};

int __init n10_eeprom_register_devices(void)
{
	int ret;
	ret = i2c_register_board_info(4, n10_i2c_bus4_board_info, 
		ARRAY_SIZE(n10_i2c_bus4_board_info)); 
		return ret;
}
	
