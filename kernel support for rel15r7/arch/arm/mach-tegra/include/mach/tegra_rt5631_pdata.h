/*
 * arch/arm/mach-tegra/include/mach/tegra_rt5631_pdata.h
 *
 * Copyright 2011 NVIDIA, Inc.
 * Copyright 2012 Eduardo José Tagle <ejtagle@tutopia.com>
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

struct tegra_rt5631_platform_data {
	int 	gpio_hp_det;
	int		hifi_codec_datafmt_stereo;	/* HiFi codec data format (stereo sound) */
	int		hifi_codec_datafmt_mono;	/* HiFi codec data format (mono sound) */
	bool	hifi_codec_master;/* If Hifi codec is master */
	int		bt_codec_datafmt;	/* Bluetooth codec data format */
	bool	bt_codec_master;	/* If bt codec is master */
};
