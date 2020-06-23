/*
 * arch/arm/mach-tegra/board-n10-bootinfo.c
 *  Boot information via /proc/bootinfo.
 *   The information currently includes:
 *	  - the powerup reason
 *	  - the hardware revision
 *
 *  All new user-space consumers of the powerup reason should use
 * the /proc/bootinfo interface; all kernel-space consumers of the
 * powerup reason should use the stingray_powerup_reason interface.
 *
 * Copyright (C) 2011 Eduardo José Tagle <ejtagle@tutopia.com> 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>

#include <asm/setup.h>
#include <asm/io.h>

#include <mach/io.h>
#include <mach/iomap.h>
#include <mach/system.h>

#include "board.h"
#include "board-n10.h"
#include "gpio-names.h"
#include "devices.h"
#include "wakeups-t2.h"

#define PMC_WAKE_STATUS 0x14

static int n10_was_wakeup(void)
{
	unsigned long status = 
		readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_WAKE_STATUS);
	return status & TEGRA_WAKE_GPIO_PV2 ? 1 : 0;
} 

static int bootinfo_show(struct seq_file *m, void *v)
{
	seq_printf(m, "POWERUPREASON : 0x%08x\n",
		n10_was_wakeup());

	seq_printf(m, "BOARDREVISION : 0x%08x\n",
		0x01);

	return 0;
}

static int bootinfo_open(struct inode *inode, struct file *file)
{
	return single_open(file, bootinfo_show, NULL);
}

static const struct file_operations bootinfo_operations = {
	.open		= bootinfo_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int __init bootinfo_init(void)
{
	struct proc_dir_entry *pe;

	pe = proc_create("bootinfo", S_IRUGO, NULL, &bootinfo_operations);
	if (!pe)
		return -ENOMEM;

	gpio_request(N10_BACK_CAM_USB_DET,"back_cam_usb_type");
	gpio_direction_input(N10_BACK_CAM_USB_DET);
	gpio_request(N10_BACK_CAM_5M_MIPI_DET,"back_cam_5m_mipi");
	gpio_direction_input(N10_BACK_CAM_5M_MIPI_DET);
	gpio_request(N10_BACK_CAM_2M_MIPI_DET,"back_cam_2m_mipi");
	gpio_direction_input(N10_BACK_CAM_2M_MIPI_DET);

	gpio_request(N10_RAM_SIZE,"RAM Size");
	gpio_direction_input(N10_RAM_SIZE);
	
	gpio_request(N10_RAM_VENDOR_1,"RAM vendor #1");
	gpio_direction_input(N10_RAM_VENDOR_1);
	gpio_request(N10_RAM_VENDOR_2,"RAM vendor #2");
	gpio_direction_input(N10_RAM_VENDOR_2);

	gpio_request(N10_NAND_VENDOR_1,"NAND vendor #1");
	gpio_direction_input(N10_NAND_VENDOR_1);
	gpio_request(N10_NAND_VENDOR_2,"NAND vendor #2");
	gpio_direction_input(N10_NAND_VENDOR_2);

	
	gpio_request(N10_FRONT_CAM_MIPI_TYPE,"front_cam_mipi_type");
	gpio_direction_input(N10_FRONT_CAM_MIPI_TYPE);
	
	gpio_request(N10_LCD_DETECT,"lcd_detect");
	gpio_direction_input(N10_LCD_DETECT);
	gpio_request(N10_LCD_SELECT1,"lcd_select1");
	gpio_direction_input(N10_LCD_SELECT1);
	gpio_request(N10_LCD_SELECT2,"lcd_select2");
	gpio_direction_input(N10_LCD_SELECT2);

	gpio_request(N10_MODEL0,"n10_model0");
	gpio_direction_input(N10_MODEL0);
	gpio_request(N10_MODEL1,"n10_model1");
	gpio_direction_input(N10_MODEL1);
	gpio_request(N10_MODEL2,"n10_model2");
	gpio_direction_input(N10_MODEL2);
	
	return 0;
}

device_initcall(bootinfo_init);
