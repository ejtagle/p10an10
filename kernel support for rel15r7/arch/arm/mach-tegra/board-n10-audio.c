/*
 * arch/arm/mach-tegra/board-n10-audio.c
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

/* All configurations related to audio */
 
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
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>
#include <asm/io.h>

#include <mach/io.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/gpio.h>
#include <mach/i2s.h>
#include <mach/spdif.h>
#include <mach/audio.h>  

#include <mach/tegra_rt5631_pdata.h>  

#include <mach/system.h>

#include "board.h"
#include "board-n10.h"
#include "gpio-names.h"
#include "devices.h"

/* Default music path: I2S1(DAC1)<->Dap1<->HifiCodec
   Bluetooth to codec: I2S2(DAC2)<->Dap4<->Bluetooth
*/
/* For n10, 
	Codec is RT5631
	Codec I2C Address = 0x34 (includes R/W bit), i2c #0
	Codec MCLK = APxx DAP_MCLK1
	
	Bluetooth is always master
*/

static struct i2c_board_info __initdata n10_i2c_bus0_board_info[] = {
	{
		I2C_BOARD_INFO("rt5631", 0x1a),
	},
};

static struct tegra_rt5631_platform_data n10_audio_pdata = {
	.gpio_hp_det 			   = N10_HP_DETECT,
	.hifi_codec_datafmt_stereo = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF,	/* HiFi codec data format (stereo)*/
	.hifi_codec_datafmt_mono   = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF,	/* HiFi codec data format (mono) */
	.hifi_codec_master         = false,					/* If Hifi codec is master */
	.bt_codec_datafmt	       = (SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_NB_NF),	/* Bluetooth codec data format */
	.bt_codec_master           = true,					/* If bt codec is master */
};

static struct platform_device n10_audio_device = {
	.name = "tegra-snd-rt5631",
	.id   = 0,
	.dev = {
		.platform_data = &n10_audio_pdata,
	}
};

static struct platform_device *n10_i2s_devices[] __initdata = {
	&tegra_i2s_device1,
	&tegra_i2s_device2,
	&tegra_spdif_device,
	&tegra_das_device,
	&spdif_dit_device,	
	&bluetooth_dit_device,
	&tegra_pcm_device,
	&n10_audio_device, /* this must come last, as we need the DAS to be initialized to access the codec registers ! */
};

int __init n10_audio_register_devices(void)
{
	int ret;
	
	pr_info("Audio: n10_audio_init\n");

	gpio_request(N10_CODEC_IRQ,"CODEC irq");
	gpio_direction_input(N10_CODEC_IRQ);

	
	/* We need the platform device to be registered firat, to make sure MCLK is 
	   running before accessing the I2C audio codec regs */
	ret = platform_add_devices(n10_i2s_devices, ARRAY_SIZE(n10_i2s_devices));	
	if (ret)
		return ret;
	
	return i2c_register_board_info(0, n10_i2c_bus0_board_info, 
		ARRAY_SIZE(n10_i2c_bus0_board_info)); 
	
}
	
