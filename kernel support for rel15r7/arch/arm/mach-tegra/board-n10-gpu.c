/*
 * arch/arm/mach-tegra/board-n10-gpu.c
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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/version.h>
#include <linux/regulator/consumer.h>
#include <linux/resource.h>
#include <linux/platform_device.h>
#include <linux/pwm_backlight.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <asm/mach-types.h>
#include <linux/nvhost.h>
#include <linux/nvmap.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>

#include "board.h"
#include "devices.h"
#include "gpio-names.h"
#include "board-n10.h"

#ifdef CONFIG_TEGRA_DC
static struct regulator *n10_hdmi_reg = NULL;
static struct regulator *n10_hdmi_pll = NULL;
#endif

static int n10_backlight_init(struct device *dev)
{
	return 0;
};

static void n10_backlight_exit(struct device *dev)
{
	gpio_set_value(N10_LCD_BL_EN , 0);
}

static int n10_backlight_notify(struct device *unused, int brightness)
{
	gpio_set_value(N10_LCD_BL_EN, !!brightness);
	return brightness;
}

static int n10_disp1_check_fb(struct device *dev, struct fb_info *info);

static struct platform_pwm_backlight_data n10_backlight_data = {
	.pwm_id		= N10_BL_PWM_ID,
	.max_brightness	= 255,
	.dft_brightness	= 200,
	.pwm_period_ns	= 1000000,
	.init		= n10_backlight_init,
	.exit		= n10_backlight_exit,
	.notify		= n10_backlight_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= n10_disp1_check_fb,
};

static struct platform_device n10_panel_bl_driver = {
	.name = "pwm-backlight",
	.id = -1,
	.dev = {
		.platform_data = &n10_backlight_data,
	},
};

#ifdef CONFIG_TEGRA_DC
static int n10_panel_enable(void)
{
	struct regulator *reg = regulator_get(NULL, "vdd_ldo4");

	regulator_enable(reg);
	regulator_put(reg);

	gpio_set_value(N10_EN_VDD_BL, 1);
	gpio_set_value(N10_EN_VDD_PNL, 1);
	msleep(300);
	
	gpio_set_value(N10_LVDS_SHDN_N, 1);
	gpio_set_value(N10_LCD_BL_EN, 1);
	return 0; 
}

static int n10_panel_disable(void)
{
	gpio_set_value(N10_LCD_BL_EN, 0);
	gpio_set_value(N10_LVDS_SHDN_N, 0);
	msleep(300);
	
	gpio_set_value(N10_EN_VDD_PNL, 0);
	gpio_set_value(N10_EN_VDD_BL, 0);
	return 0; 
}

static int n10_hdmi_enable(void)
{
	if (!n10_hdmi_reg) {
		n10_hdmi_reg = regulator_get(NULL, "avdd_hdmi"); /* LD07 */
		if (IS_ERR_OR_NULL(n10_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			n10_hdmi_reg = NULL;
			return PTR_ERR(n10_hdmi_reg);
		}
	}
	regulator_enable(n10_hdmi_reg);

	if (!n10_hdmi_pll) {
		n10_hdmi_pll = regulator_get(NULL, "avdd_hdmi_pll"); /* LD08 */
		if (IS_ERR_OR_NULL(n10_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			n10_hdmi_pll = NULL;
			regulator_disable(n10_hdmi_reg);
			n10_hdmi_reg = NULL;
			return PTR_ERR(n10_hdmi_pll);
		}
	}
	regulator_enable(n10_hdmi_pll);
	return 0; 
}

static int n10_hdmi_disable(void)
{
	regulator_disable(n10_hdmi_reg);
	regulator_disable(n10_hdmi_pll);
	return 0; 
}
#endif

static struct tegra_dc_mode n10_panel_modes[] = {
	{
		.pclk = 68930000,
		.h_ref_to_sync = 11,
		.v_ref_to_sync = 1,
		.h_sync_width = 58,
		.v_sync_width = 4,
		.h_back_porch = 58,
		.v_back_porch = 4,
		.h_active = 1280,
		.v_active = 800,
		.h_front_porch = 58,
		.v_front_porch = 4, 
	},
};

static struct tegra_fb_data n10_fb_data = {
	.win		= 0,
	.xres		= 1280,
	.yres		= 800,
	.bits_per_pixel	= N10_FB_BPP, 
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};



#if defined(N10_1920x1080HDMI)

/* Frame buffer size assuming 32bpp color and 2 pages for page flipping */
static struct tegra_fb_data n10_hdmi_fb_data = {
	.win		= 0,
	.xres		= 1920,
	.yres		= 1080,
	.bits_per_pixel	= N10_FB_HDMI_BPP,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

#else

static struct tegra_fb_data n10_hdmi_fb_data = {
	.win		= 0,
	.xres		= 1366,
	.yres		= 768,
	.bits_per_pixel	= N10_FB_HDMI_BPP,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};
#endif

static struct tegra_dc_out n10_disp1_out = {
	.type		= TEGRA_DC_OUT_RGB,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.depth		= 18,
	
	 /* Enable dithering. Tegra supports error diffusion
	    when the active region is less than 1280 pixels
        wide - Otherwise, ordered dither is all we will 
		have. */
	/*.dither		= TEGRA_DC_ORDERED_DITHER,*/
	.dither		= TEGRA_DC_ERRDIFF_DITHER,

	.height 	= 136, /* mm */
	.width 		= 217, /* mm */
	
	.modes	 	= n10_panel_modes,
	.n_modes 	= ARRAY_SIZE(n10_panel_modes),

	.enable		= n10_panel_enable,
	.disable	= n10_panel_disable,
};

static struct tegra_dc_out n10_hdmi_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,

	.dcc_bus	= 1,
	.hotplug_gpio	= N10_HDMI_HPD,
	
	.max_pixclock	= KHZ2PICOS(148500),

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= n10_hdmi_enable,
	.disable	= n10_hdmi_disable,
};

static struct tegra_dc_platform_data n10_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.emc_clk_rate	= 300000000,
	.default_out	= &n10_disp1_out,
	.fb		= &n10_fb_data,
};

static struct tegra_dc_platform_data n10_hdmi_pdata = {
	.flags		= 0,
	.default_out	= &n10_hdmi_out,
	.fb		= &n10_hdmi_fb_data,
};

#if !defined(DYNAMIC_GPU_MEM)
/* Estimate memory layout for GPU - static layout */
#define N10_GPU_MEM_START	(N10_MEM_SIZE - N10_GPU_MEM_SIZE)
#define N10_FB_BASE		 	(N10_GPU_MEM_START)
#define N10_FB_HDMI_BASE 	(N10_GPU_MEM_START + N10_FB_SIZE)
#define N10_CARVEOUT_BASE 	(N10_GPU_MEM_START + N10_FB_SIZE + N10_FB_HDMI_SIZE)
#define N10_CARVEOUT_SIZE	(N10_MEM_SIZE - N10_CARVEOUT_BASE)
#endif

#ifdef CONFIG_TEGRA_DC
/* Display Controller */
static struct resource n10_disp1_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
#if !defined(DYNAMIC_GPU_MEM)
		.start	= N10_FB_BASE,
		.end	= N10_FB_BASE + N10_FB_SIZE - 1, 
#endif
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource n10_disp2_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_B_GENERAL,
		.end	= INT_DISPLAY_B_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY2_BASE,
		.end	= TEGRA_DISPLAY2_BASE + TEGRA_DISPLAY2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
#if !defined(DYNAMIC_GPU_MEM)
		.start	= N10_FB_HDMI_BASE,
		.end	= N10_FB_HDMI_BASE + N10_FB_HDMI_SIZE - 1,
#endif
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};
#endif

static struct nvhost_device n10_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= n10_disp1_resources,
	.num_resources	= ARRAY_SIZE(n10_disp1_resources),
	.dev = {
		.platform_data = &n10_disp1_pdata,
	},
};


static int n10_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &n10_disp1_device.dev;
}

static struct nvhost_device n10_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= n10_disp2_resources,
	.num_resources	= ARRAY_SIZE(n10_disp2_resources),
	.dev = {
		.platform_data = &n10_hdmi_pdata,
	},
};

static struct nvmap_platform_carveout n10_carveouts[] = {
	[0] = NVMAP_HEAP_CARVEOUT_IRAM_INIT,
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
#if !defined(DYNAMIC_GPU_MEM)
		.base		= N10_CARVEOUT_BASE,
		.size		= N10_CARVEOUT_SIZE,
#endif
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data n10_nvmap_data = {
	.carveouts	= n10_carveouts,
	.nr_carveouts	= ARRAY_SIZE(n10_carveouts),
};

static struct platform_device n10_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &n10_nvmap_data,
	},
};

static struct platform_device *n10_gfx_devices[] __initdata = {
	&n10_nvmap_device,
	&tegra_pwfm2_device,
	&n10_panel_bl_driver,
	&tegra_gart_device,
	&tegra_avp_device,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
/* put early_suspend/late_resume handlers here for the display in order
 * to keep the code out of the display driver, keeping it closer to upstream
 */
struct early_suspend n10_panel_early_suspender;

static void n10_panel_early_suspend(struct early_suspend *h)
{
	/* power down LCD, add use a black screen for HDMI */
	if (num_registered_fb > 0)
		fb_blank(registered_fb[0], FB_BLANK_POWERDOWN);
	if (num_registered_fb > 1)
		fb_blank(registered_fb[1], FB_BLANK_NORMAL);
#ifdef CONFIG_TEGRA_CONVSERVATIVE_GOV_ON_EARLYSUPSEND
	cpufreq_store_default_gov();
	if (cpufreq_change_gov(cpufreq_conservative_gov))
		pr_err("Early_suspend: Error changing governor to %s\n",
				cpufreq_conservative_gov);
#endif
}

static void n10_panel_late_resume(struct early_suspend *h)
{
	unsigned i;
#ifdef CONFIG_TEGRA_CONVSERVATIVE_GOV_ON_EARLYSUPSEND
	if (cpufreq_restore_default_gov())
		pr_err("Early_suspend: Unable to restore governor\n");
#endif
	for (i = 0; i < num_registered_fb; i++)
		fb_blank(registered_fb[i], FB_BLANK_UNBLANK);
}
#endif 

int __init n10_gpu_register_devices(void)
{
	struct resource *res;
	int err;
	
#if defined(CONFIG_TEGRA_NVMAP)
	/* Plug in carveout memory area and size */
	if (tegra_carveout_start > 0 && tegra_carveout_size > 0) {
		n10_carveouts[1].base = tegra_carveout_start;
		n10_carveouts[1].size = tegra_carveout_size;
	}
#endif

	gpio_request(N10_EN_VDD_BL, "en_vdd_bl");
	gpio_direction_output(N10_EN_VDD_BL, 1);
	
	gpio_request(N10_EN_VDD_PNL , "en_vdd_pnl");
	gpio_direction_output(N10_EN_VDD_PNL , 1);
	
	usleep(300);
	
	gpio_request(N10_LVDS_SHDN_N, "lvds_shut_n");
	gpio_direction_output(N10_LVDS_SHDN_N, 1);
	
	gpio_request(N10_LVDS_SHDN2,"lvds_shut2");
	gpio_direction_input(N10_LVDS_SHDN2);
	
	gpio_request(N10_LCD_BL_EN, "bl_enb");
	gpio_direction_output(N10_LCD_BL_EN, 1);
	
	gpio_request(N10_HDMI_HPD, "hdmi_hpd");
	gpio_direction_input(N10_HDMI_HPD);


#ifdef CONFIG_HAS_EARLYSUSPEND
	n10_panel_early_suspender.suspend = n10_panel_early_suspend;
	n10_panel_early_suspender.resume = n10_panel_late_resume;
	n10_panel_early_suspender.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&n10_panel_early_suspender);
#endif 

#ifdef CONFIG_TEGRA_GRHOST
	err = nvhost_device_register(&tegra_grhost_device);
	if (err)
		return err;
#endif

	err = platform_add_devices(n10_gfx_devices,
				   ARRAY_SIZE(n10_gfx_devices));
			
#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)			
	/* Plug in framebuffer 1 memory area and size */
	if (tegra_fb_start > 0 && tegra_fb_size > 0) {
		res = nvhost_get_resource_byname(&n10_disp1_device,
			IORESOURCE_MEM, "fbmem");
		res->start = tegra_fb_start;
		res->end = tegra_fb_start + tegra_fb_size - 1;
	}

	/* Plug in framebuffer 2 memory area and size */
	if (tegra_fb2_start > 0 && tegra_fb2_size > 0) {
		res = nvhost_get_resource_byname(&n10_disp2_device,
			IORESOURCE_MEM, "fbmem");
			res->start = tegra_fb2_start;
			res->end = tegra_fb2_start + tegra_fb2_size - 1;
	}
#endif
				   
	/* Move the bootloader framebuffer to our framebuffer */
	if (tegra_bootloader_fb_start > 0 && tegra_fb_start > 0 &&
		tegra_fb_size > 0 && tegra_bootloader_fb_size > 0) {
		
		/* Copy the bootloader fb to the fb. */		
		tegra_move_framebuffer(tegra_fb_start, tegra_bootloader_fb_start,
			min(tegra_fb_size, tegra_bootloader_fb_size)); 		
			
		/* Copy the bootloader fb to the fb2. */
		tegra_move_framebuffer(tegra_fb2_start, tegra_bootloader_fb_start,
			min(tegra_fb2_size, tegra_bootloader_fb_size)); 			
	}		

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	/* Register the framebuffers */
	if (!err)
		err = nvhost_device_register(&n10_disp1_device);

	if (!err)
		err = nvhost_device_register(&n10_disp2_device);
#endif

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_NVAVP)
	/* Register the nvavp device */
	if (!err)
		err = nvhost_device_register(&nvavp_device);
#endif

	return err;
}

int __init n10_protected_aperture_init(void)
{
	if (tegra_grhost_aperture > 0) {
		tegra_protected_aperture_init(tegra_grhost_aperture);
	}
	return 0;
}
late_initcall(n10_protected_aperture_init);

