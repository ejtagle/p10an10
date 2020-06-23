/*
 * Copyright (C) 2012 Eduardo José Tagle <ejtagle@tutopia.com> 
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/mpu.h>
#include <linux/mpu3050.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>

#include "board-n10.h"
#include "gpio-names.h"

/* Invensense MPU Definitions */
#define MPU_GYRO_NAME			"mpu3050"
#define MPU_GYRO_IRQ_GPIO		N10_GYRO_IRQ

#define MPU_GYRO_ADDR			0x68
#define MPU_GYRO_BUS_NUM		0
#define MPU_GYRO_ORIENTATION	{0, 1, 0,  1, 0, 0,  0, 0, -1}

#define MPU_ACCEL_NAME			"kxtf9"
#define MPU_ACCEL_IRQ_GPIO		N10_ACCEL_IRQ
#define MPU_ACCEL_ADDR			0x0F
#define MPU_ACCEL_BUS_NUM		0
#define MPU_ACCEL_ORIENTATION	{0, 1, 0,  1, 0, 0,  0, 0, -1}

#define MPU_COMPASS_NAME		"ami30x"
#define MPU_COMPASS_IRQ_GPIO 	N10_COMPASS_IRQ
#define MPU_COMPASS_ADDR		0x0E
#define MPU_COMPASS_BUS_NUM		0
#define MPU_COMPASS_ORIENTATION	{1, 0, 0,  0,-1, 0,  0, 0, -1}

static struct mpu_platform_data mpu3050_data = { /*ok*/
	.int_config  = 0x10,
	.orientation = MPU_GYRO_ORIENTATION,  /* Orientation matrix for MPU on n10 */
	.level_shifter = 0,
};

static struct ext_slave_platform_data kxtf9_accel_data = {
	.address     = MPU_ACCEL_ADDR,
	.irq         = MPU_ACCEL_IRQ_GPIO,
	.adapt_num   = MPU_ACCEL_BUS_NUM,
	.bus         = EXT_SLAVE_BUS_SECONDARY,
	.orientation = MPU_ACCEL_ORIENTATION,  /* Orientation matrix for Kionix on n10 */
};

static struct ext_slave_platform_data ami30x_compass_data = {
	.address     = MPU_COMPASS_ADDR,
	.irq         = MPU_COMPASS_IRQ_GPIO,
	.adapt_num   = MPU_COMPASS_BUS_NUM,
	.bus         = EXT_SLAVE_BUS_PRIMARY,
	.orientation = MPU_COMPASS_ORIENTATION,  /* Orientation matrix for AKM on n10 */
};

static struct i2c_board_info __initdata mpu3050_i2c0_boardinfo[] = { /*ok*/
	{
		I2C_BOARD_INFO(MPU_GYRO_NAME, MPU_GYRO_ADDR),
		.irq = TEGRA_GPIO_TO_IRQ(N10_GYRO_IRQ),
		.platform_data = &mpu3050_data,
	},
	{
		I2C_BOARD_INFO(MPU_ACCEL_NAME, MPU_ACCEL_ADDR),
		.irq = TEGRA_GPIO_TO_IRQ(N10_ACCEL_IRQ),
		.platform_data = &kxtf9_accel_data,
	},
	{
		I2C_BOARD_INFO(MPU_COMPASS_NAME, MPU_COMPASS_ADDR),
		.irq = TEGRA_GPIO_TO_IRQ(N10_COMPASS_IRQ),
		.platform_data = &ami30x_compass_data,
	},
};

int __init n10_sensors_register_devices(void)
{
	return i2c_register_board_info(0, mpu3050_i2c0_boardinfo,
		ARRAY_SIZE(mpu3050_i2c0_boardinfo));
}
