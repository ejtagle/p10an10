/*
 * arch/arm/mach-tegra/board-n10-camera.c
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
 
/* camera is in i2c bus 3, address 0x3c */

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
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <media/mt9d115.h>

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

#ifdef CONFIG_TEGRA_CAMERA

/*
 mt9d115:
 saddr=0 : 0x78
 saddr=1 : 0x7A
 
 
 VI_GP5 = RESET (active low) CAM_RST_N
 VI_GP4 = PWDN CAM_PDWN
 
 RESET_BAR#
 STANDBY
 
 Poweron:
 -Deassert STANDBY
 -Assert RESET_BAR#
 -Apply VDDIO
 -Wait 1ms
 -Apply Analog VDD
 -Wait 1ms
 -Enable EXTCLK and wati
 -Deassert RESET_BAR# for at least 10 EXTCLK cycles
 -Assert RESET_BAR for at least 6000 cycles
 RESET_BAR
 
 
 NOTE: EXTCLK must be running for sensor to be accessible!
 
 */

static int mt9d115_sensor_power_on(void) 
{
	// Deassert standby
	gpio_direction_output(N10_BACK_CAM_RST_N,  1);
	gpio_direction_output(N10_BACK_CAM_PWR_EN_N, 0);
	msleep(20);

	// do MT9D115 hardware reset and enter hardware standby mode
	gpio_direction_output(N10_BACK_CAM_RST_N,  0);
	msleep(1);
	gpio_direction_output(N10_BACK_CAM_RST_N,  1);
	msleep(1);

	return 0;
}

static int mt9d115_sensor_power_off(void)
{
	// Just go into hard standby
	gpio_direction_output(N10_BACK_CAM_PWR_EN_N, 1);
	
	// standby time need 2 frames
	msleep(150); 	
	
	return 0;
}

static int mt9d115_sensor_flash_on(void) 
{
	gpio_direction_output(N10_LED_LIGHT,  1);
	return 0;
}

static int mt9d115_sensor_flash_off(void)
{
	gpio_direction_output(N10_LED_LIGHT, 0);
	return 0;
}

static struct mt9d115_sensor_platform_data mt9d115_sensor_data = {
	.power_on = mt9d115_sensor_power_on,
	.power_off = mt9d115_sensor_power_off,
	.flash_on = mt9d115_sensor_flash_on,
	.flash_off = mt9d115_sensor_flash_off,
};

static const struct i2c_board_info n10_i2c3_board_info[] = { /*ok*/
	{
		I2C_BOARD_INFO("mt9d115", 0x3C),
		.platform_data = &mt9d115_sensor_data,
	},
};
 
static struct platform_device n10_camera_pm_device = {
	.name		= "n10-pm-camera",
	.id			= -1,
};

static struct platform_device tegra_camera = {
	.name = "tegra_camera",
	.id = -1,
}; 
 
static struct platform_device *n10_camera_pm_devices[] __initdata = {
	&tegra_camera,
	&n10_camera_pm_device,
};

int __init n10_camera_pm_register_devices(void)
{
	return platform_add_devices(n10_camera_pm_devices, ARRAY_SIZE(n10_camera_pm_devices));
}

struct camera_gpios {
	const char *name;
	int gpio;
};

#define CAMERA_GPIO(_name, _gpio)  \
	{                          \
		.name = _name,     \
		.gpio = _gpio,     \
	}

static struct camera_gpios mt9d115_sensor_gpio[] = {
	[0] = CAMERA_GPIO("cam_pdwn_n", N10_BACK_CAM_PDWN_N),
	[1] = CAMERA_GPIO("cam_rst_n", N10_BACK_CAM_RST_N),
	[2] = CAMERA_GPIO("cam_pwr_n", N10_BACK_CAM_PWR_EN_N),
};

int __init n10_camera_late_init(void)
{
	int ret;
	int i;
	
	for (i = 0; i < ARRAY_SIZE(mt9d115_sensor_gpio); i++) {
		ret = gpio_request(mt9d115_sensor_gpio[i].gpio,
				mt9d115_sensor_gpio[i].name);
		if (ret < 0) {
			pr_err("%s: gpio_request failed for gpio #%d\n",
				__func__, i);
			goto fail_free_gpio;
		}
	}

	/* DEBUGGING - Export cam GPIOs */
	gpio_export(N10_BACK_CAM_RST_N, true);
	gpio_export(N10_BACK_CAM_PDWN_N, true);
	gpio_export(N10_BACK_CAM_PWR_EN_N, true);
	
	// turn on camera power
	gpio_direction_input(N10_BACK_CAM_PDWN_N);
	mt9d115_sensor_power_on();

	/* Register sensor */
	i2c_new_device(i2c_get_adapter(3), n10_i2c3_board_info);
	
	return 0;

fail_free_gpio:
    while (i--)
        gpio_free(mt9d115_sensor_gpio[i].gpio);

	return ret;
}

late_initcall(n10_camera_late_init);

#else

int __init n10_camera_pm_register_devices(void)
{
	return 0;
}

#endif /* CONFIG_TEGRA_CAMERA */ 