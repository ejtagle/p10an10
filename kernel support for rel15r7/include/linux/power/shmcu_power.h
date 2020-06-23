/*
 * Battery charger driver for SHMCU embedded controller
 *
 * Copyright (C) 2012 Eduardo José Tagle <ejtagle@tutopia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


struct shmcu_power_platform_data {
	int dc_charger_gpio;					/* Gpio to detect Charger plugged in */
	int dc_charger_irq;		
};

