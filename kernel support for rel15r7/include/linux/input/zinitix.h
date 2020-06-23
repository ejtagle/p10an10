/* drivers/input/touchscreen/zinitix.c
 *
 * Copyright (C) 2011-12 Eduardo José Tagle
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

struct zinitix_platform_data {
	int power_gpio;					/* power control gpio 	(1=powered) */
	int reset_gpio;					/* reset control gpio 	(0=reset) */
	int irq_gpio;					/* irq gpio */
	int icon_keycode[3];			/* icon keycodes */
};
