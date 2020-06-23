/*
 * Copyright (C) 2010 NVIDIA, Inc.
 *               2012 Eduardo José Tagle <ejtagle@tutopia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/i2c.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/console.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/virtual_adj.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/tps6586x.h>
#include <linux/power_supply.h>
#include <linux/power/shmcu_power.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/err.h>
#include <linux/mfd/shmcu.h>

#include <asm/io.h>

#include <mach/io.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/gpio.h>
#include <mach/system.h>

#include "board-n10.h"
#include "gpio-names.h"
#include "devices.h"
#include "pm.h"
#include "wakeups-t2.h"
#include "fuse.h"


#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)

/* Core voltage rail : VDD_CORE -> SM0
*/
static struct regulator_consumer_supply tps658621_sm0_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL)
};

/* CPU voltage rail : VDD_CPU -> SM1
*/
static struct regulator_consumer_supply tps658621_sm1_supply[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL)
};

/* unused */
static struct regulator_consumer_supply tps658621_sm2_supply[] = {
	REGULATOR_SUPPLY("vdd_sm2", NULL)
};

static struct regulator_consumer_supply tps658621_ldo0_supply[] = {
	REGULATOR_SUPPLY("vdd_ldo0", NULL),
	REGULATOR_SUPPLY("p_cam_avdd", NULL),
	REGULATOR_SUPPLY("vcsi", "tegra_camera")
	
};

static struct regulator_consumer_supply tps658621_ldo1_supply[] = { /* 1V2 */
	REGULATOR_SUPPLY("vdd_ldo1", NULL),
	REGULATOR_SUPPLY("avdd_pll", NULL) /*ok-*/
};

static struct regulator_consumer_supply tps658621_ldo2_supply[] = { 
	REGULATOR_SUPPLY("vdd_ldo2", NULL),
	REGULATOR_SUPPLY("vdd_rtc", NULL) /*ok-*/
};

static struct regulator_consumer_supply tps658621_ldo3_supply[] = { /* 3V3 */
	REGULATOR_SUPPLY("vdd_ldo3", NULL),
	REGULATOR_SUPPLY("avdd_usb", NULL),/*ok-*/
	REGULATOR_SUPPLY("avdd_usb_pll", NULL), /*ok-*/
	REGULATOR_SUPPLY("vdd_nand", NULL), /*ok-*/
	REGULATOR_SUPPLY("vdd_sdio", NULL), /*ok-*/
};

static struct regulator_consumer_supply tps658621_ldo4_supply[] = { /* VDD IO VI */
	REGULATOR_SUPPLY("vdd_ldo4", NULL),
	REGULATOR_SUPPLY("avdd_osc", NULL), /*ok-*/
	REGULATOR_SUPPLY("vddio_sys", NULL), /*ok-*/
	REGULATOR_SUPPLY("vddio_lcd", NULL), /*ok-*/
	REGULATOR_SUPPLY("vddio_audio", NULL), /*ok-*/
	REGULATOR_SUPPLY("vdd_ddr", NULL), /*ok-*/
	REGULATOR_SUPPLY("vddio_uart", NULL), /*ok-*/
	REGULATOR_SUPPLY("vddio_bb", NULL), /*ok-*/
};

static struct regulator_consumer_supply tps658621_ldo5_supply[] = {
	REGULATOR_SUPPLY("vdd_ldo5", NULL),
	REGULATOR_SUPPLY("vmmc", "sdhci-tegra.3"), // camera
};

static struct regulator_consumer_supply tps658621_ldo6_supply[] = { //(camera)
	REGULATOR_SUPPLY("vdd_ldo6", NULL),
	REGULATOR_SUPPLY("vcsi", "tegra_camera"),
	REGULATOR_SUPPLY("vdd_dac", NULL), /*ok-*/
	REGULATOR_SUPPLY("vdd_vi", NULL), /*ok-*/
};

static struct regulator_consumer_supply tps658621_ldo7_supply[] = {
	REGULATOR_SUPPLY("vdd_ldo7", NULL),
	REGULATOR_SUPPLY("avdd_hdmi", NULL) /*ok-*/
};

static struct regulator_consumer_supply tps658621_ldo8_supply[] = { /* AVDD_HDMI_PLL */
	REGULATOR_SUPPLY("vdd_ldo8", NULL),
	REGULATOR_SUPPLY("avdd_hdmi_pll", NULL), /*ok-*/
	REGULATOR_SUPPLY("avdd_hdmi_hotplug", NULL) /*ok-*/
};

static struct regulator_consumer_supply tps658621_ldo9_supply[] = {
	REGULATOR_SUPPLY("vdd_ldo9", NULL),
	REGULATOR_SUPPLY("vdd_ddr_rx", NULL) /*ok-*/
};

static struct regulator_consumer_supply tps658621_rtc_supply[] = {
	REGULATOR_SUPPLY("vdd_rtc_2", NULL)
};

/* unused */
/*static struct regulator_consumer_supply tps658621_buck_supply[] = {
	REGULATOR_SUPPLY("pll_e", NULL),
};*/

/* Super power voltage rail for the SOC : VDD SOC
*/
static struct regulator_consumer_supply tps658621_soc_supply[] = {
	REGULATOR_SUPPLY("vdd_soc", NULL) /*ok*/
};


/* MIPI voltage rail (DSI_CSI): AVDD_DSI_CSI -> VDD_1V2
   Wlan : VCORE_WIFI (VDD_1V2)
*/
static struct regulator_consumer_supply fixed_switch_1v2_supply[] = {
	REGULATOR_SUPPLY("avdd_dsi_csi", NULL), /*ok-*/
	REGULATOR_SUPPLY("vcore_wifi", NULL)
};

/* PEX_CLK voltage rail : PMU_GPIO-1 -> VDD_1V5
*/
static struct regulator_consumer_supply fixed_switch_5v_supply[] = {
	REGULATOR_SUPPLY("vdd_pex_clk_2", NULL),
};

/* VAON */
static struct regulator_consumer_supply fixed_vdd_aon_supply[] = { 
	REGULATOR_SUPPLY("vdd_aon", NULL)
};

/*
 * Current TPS6586x is known for having a voltage glitch if current load changes
 * from low to high in auto PWM/PFM mode for CPU's Vdd line.
 */
static struct tps6586x_settings sm1_config = {
	.sm_pwm_mode = PWM_ONLY,
	.slew_rate   = SLEW_RATE_3520UV_PER_SEC,
};

static struct tps6586x_settings sm0_config = {
	.sm_pwm_mode = PWM_DEFAULT_VALUE,
	.slew_rate   = SLEW_RATE_3520UV_PER_SEC,
};

#define ADJ_REGULATOR_INIT(_id, _minmv, _maxmv, _aon, _bon, config)	\
	{													\
		.constraints = {								\
			.name = "tps658621_" #_id,					\
			.min_uV = (_minmv)*1000,					\
			.max_uV = (_maxmv)*1000,					\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |\
					     REGULATOR_MODE_STANDBY),		\
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |	\
					   REGULATOR_CHANGE_STATUS |		\
					   REGULATOR_CHANGE_VOLTAGE),		\
			.always_on	= _aon, 						\
			.boot_on	= _bon, 						\
		},												\
		.num_consumer_supplies = ARRAY_SIZE(tps658621_##_id##_supply),\
		.consumer_supplies = tps658621_##_id##_supply,	\
		.driver_data = config,							\
	}
	
#define FIXED_REGULATOR_INIT(_id, _mv, _aon, _bon)		\
	{													\
		.constraints = {								\
			.name = #_id,								\
			.min_uV = (_mv)*1000,						\
			.max_uV = (_mv)*1000,						\
			.valid_modes_mask = REGULATOR_MODE_NORMAL,	\
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,	\
			.always_on	= _aon, 						\
			.boot_on	= _bon, 						\
		},												\
		.num_consumer_supplies = ARRAY_SIZE( fixed_##_id##_supply),\
		.consumer_supplies = fixed_##_id##_supply,	\
	}

#define ON	1
#define OFF	0

static struct regulator_init_data sm0_data  		 
	= ADJ_REGULATOR_INIT(sm0,  725, 1500,  ON,  ON, &sm0_config); // 1200
static struct regulator_init_data sm1_data  		 
	= ADJ_REGULATOR_INIT(sm1,  725, 1500,  ON,  ON, &sm1_config); // 1000 (min was 1100)
static struct regulator_init_data sm2_data  		 
	= ADJ_REGULATOR_INIT(sm2, 3000, 4550,  ON,  ON, NULL); // 3700
static struct regulator_init_data ldo0_data 		 
	= ADJ_REGULATOR_INIT(ldo0,1200, 1250, OFF, OFF, NULL); // 3300 (1.2)
static struct regulator_init_data ldo1_data 		 
	= ADJ_REGULATOR_INIT(ldo1,1100, 1150,  ON,  ON, NULL); // 1100  V-1V1
static struct regulator_init_data ldo2_data 		 
	= ADJ_REGULATOR_INIT(ldo2,1200, 1250,  ON,  ON, NULL); // 1200  V-RTC
static struct regulator_init_data ldo3_data 		 
	= ADJ_REGULATOR_INIT(ldo3,3300, 3350,  ON, OFF, NULL); // 3300 
static struct regulator_init_data ldo4_data 		 
	= ADJ_REGULATOR_INIT(ldo4,1800, 1850,  ON, OFF, NULL); // 1800
static struct regulator_init_data ldo5_data 		 
	= ADJ_REGULATOR_INIT(ldo5,2800, 2850,  ON,  ON, NULL); // 2850
static struct regulator_init_data ldo6_data 		 
	= ADJ_REGULATOR_INIT(ldo6,1800, 1850,  ON,  ON, NULL); // 1850
static struct regulator_init_data ldo7_data 		 
	= ADJ_REGULATOR_INIT(ldo7,3300, 3350, OFF, OFF, NULL); // 3300
static struct regulator_init_data ldo8_data 		 
	= ADJ_REGULATOR_INIT(ldo8,1800, 1850, OFF, OFF, NULL); // 1800
static struct regulator_init_data ldo9_data 		 
	= ADJ_REGULATOR_INIT(ldo9,2800, 2850,  ON,  ON, NULL); // 2800

static struct regulator_init_data rtc_data  		 
	= ADJ_REGULATOR_INIT(rtc, 1250, 3350, ON, ON, NULL); // 3300
	
/*static struct regulator_init_data buck_data 
	= ADJ_REGULATOR_INIT(buck,1250, 3350, OFF, OFF, NULL); // 3300*/
	
static struct regulator_init_data soc_data  		 
	= ADJ_REGULATOR_INIT(soc, 1250, 3300,  ON,  ON, NULL);
	
static struct regulator_init_data switch_5v_data  
	= FIXED_REGULATOR_INIT(switch_5v , 5000, ON, OFF ); // 5000 (VDD5.0)
static struct regulator_init_data switch_1v2_data  
	= FIXED_REGULATOR_INIT(switch_1v2, 1200, ON, OFF ); // 1200 (VDD1.2) 


// vdd_aon is a virtual regulator used to allow frequency and voltage scaling of the CPU/EMC														
static struct regulator_init_data vdd_aon_data =
{
	.constraints = {
		.name = "ldo_vdd_aon",
		.min_uV =  625000,
		.max_uV = 2700000,
		.valid_modes_mask = (REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY),
		.valid_ops_mask = (REGULATOR_CHANGE_MODE |REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE),
		.always_on	= 1,
		.boot_on	= 1,
	},
	.num_consumer_supplies = ARRAY_SIZE(fixed_vdd_aon_supply),
	.consumer_supplies = fixed_vdd_aon_supply,
};


#define FIXED_REGULATOR_CONFIG(_id, _mv, _gpio, _activehigh, _odrain, _delay, _atboot, _data)	\
	{												\
		.supply_name 	= #_id,						\
		.microvolts  	= (_mv)*1000,				\
		.gpio        	= _gpio,					\
		.enable_high	= _activehigh,				\
		.gpio_is_open_drain = _odrain,				\
		.startup_delay	= _delay,					\
		.enabled_at_boot= _atboot,					\
		.init_data		= &_data,					\
	}

/* The next 3 are fixed regulators controlled by PMU GPIOs */
static struct fixed_voltage_config switch_5v_cfg  
	= FIXED_REGULATOR_CONFIG(switch_5v  , 5000, N10_EN_VDD_5V  , 1,0, 200000, 1, switch_5v_data);
static struct fixed_voltage_config switch_1v2_cfg
	= FIXED_REGULATOR_CONFIG(switch_1v2 , 1200, N10_EN_VDD_1V2 , 1,0, 200000, 1, switch_1v2_data); /* Used for MIPI interface! */


/* the always on vdd_aon: required for freq. scaling to work */
static struct virtual_adj_voltage_config vdd_aon_cfg = {
	.supply_name = "REG-AON",
	.id			 = -1,
	.min_mV 	 =  625,
	.max_mV 	 = 2700,
	.step_mV 	 =   25,
	.mV			 = 1800,
	.init_data	 = &vdd_aon_data,
};

static struct platform_device n10_vdd_aon_reg_device = 
{
	.name = "reg-virtual-adj-voltage",
	.id = -1,
	.dev = {
		.platform_data = &vdd_aon_cfg,
	},
};

#define TPS_ADJ_REG(_id, _data)			\
	{									\
		.id = TPS6586X_ID_##_id,		\
		.name = "tps6586x-regulator",	\
		.platform_data = _data,			\
	}
	
#define TPS_GPIO_FIX_REG(_id,_data)		\
	{									\
		.id = _id,						\
		.name = "reg-fixed-voltage",	\
		.platform_data = _data,			\
	} 	
	

static struct tps6586x_rtc_platform_data n10_rtc_data = {
	.irq = TEGRA_NR_IRQS + TPS6586X_INT_RTC_ALM1,
	.start = {
		.year  = 2012,
		.month = 1,
		.day   = 1,
		.hour  = 1,
		.min   = 1,
		.sec   = 1,
	},
	.cl_sel = TPS6586X_RTC_CL_SEL_1_5PF /* use lowest (external 20pF cap) */
};

static struct tps6586x_subdev_info tps_devs[] = {
	TPS_ADJ_REG(SM_0, &sm0_data),
	TPS_ADJ_REG(SM_1, &sm1_data),
	TPS_ADJ_REG(SM_2, &sm2_data),
	TPS_ADJ_REG(LDO_0, &ldo0_data), 
	TPS_ADJ_REG(LDO_1, &ldo1_data),
	TPS_ADJ_REG(LDO_2, &ldo2_data),
	TPS_ADJ_REG(LDO_3, &ldo3_data),
	TPS_ADJ_REG(LDO_4, &ldo4_data),
	TPS_ADJ_REG(LDO_5, &ldo5_data),
	TPS_ADJ_REG(LDO_6, &ldo6_data),
	TPS_ADJ_REG(LDO_7, &ldo7_data),
	TPS_ADJ_REG(LDO_8, &ldo8_data),
	TPS_ADJ_REG(LDO_9, &ldo9_data),
	TPS_ADJ_REG(LDO_RTC, &rtc_data),
	TPS_ADJ_REG(LDO_SOC, &soc_data),
	TPS_GPIO_FIX_REG(0, &switch_5v_cfg),
	TPS_GPIO_FIX_REG(1, &switch_1v2_cfg),
	{
		.id		= -1,
		.name		= "tps6586x-rtc",
		.platform_data	= &n10_rtc_data,
	},
};

static struct tps6586x_platform_data tps_platform = {
	.gpio_base = TPS6586X_GPIO_BASE,
	.irq_base  = TPS6586X_IRQ_BASE,
	.subdevs   = tps_devs,
	.num_subdevs = ARRAY_SIZE(tps_devs),
	.use_power_off  = true,	
};

static struct i2c_board_info __initdata n10_regulators[] = {
	{
		I2C_BOARD_INFO("tps6586x", 0x34),
		.irq		= INT_EXTERNAL_PMU,
		.platform_data = &tps_platform,
	},
};


/* Power controller of N10 platform data */
static struct shmcu_power_platform_data shmcu_power_pdata = {
	.dc_charger_irq = TEGRA_GPIO_TO_IRQ(N10_DCIN_DET),
	.dc_charger_gpio = N10_DCIN_DET,
};

/* Power controller of N10 */
static struct shmcu_subdev_info shmcu_subdevs[] = {
	{
		.name = "shmcu-power",
		.id   = 0,
		.platform_data = &shmcu_power_pdata,
	},
	{
		.name = "shmcu-rtc",
		.id   = 1,
	},
};

/* The N10 Embedded controller */
static struct shmcu_platform_data shmcu_mfd_platform_data = {
	.led_gpio	= N10_LED_LOCK_OFF_N,
	.gpio_base	= SHMCU_GPIO_BASE,
	.subdevs	= shmcu_subdevs,
	.num_subdevs = ARRAY_SIZE(shmcu_subdevs),
};

static struct i2c_board_info __initdata n10_shmcu_mfd[] = {
	{
		I2C_BOARD_INFO("shmcu", 0x66),
		.platform_data = &shmcu_mfd_platform_data,
	},
};

/* missing from defines ... remove ASAP when defined in devices.c */
static struct resource tegra_rtc_resources[] = {
	[0] = {
		.start	= TEGRA_RTC_BASE,
		.end	= TEGRA_RTC_BASE + TEGRA_RTC_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_RTC,
		.end	= INT_RTC,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device tegra_rtc_device = {
	.name		= "tegra_rtc",
	.id		= -1,
	.resource	= tegra_rtc_resources,
	.num_resources	= ARRAY_SIZE(tegra_rtc_resources),
};

static struct platform_device *n10_power_devices[] __initdata = {
	&n10_vdd_aon_reg_device,
	&tegra_pmu_device,
	&tegra_rtc_device,	
};

static void n10_board_suspend(int lp_state, enum suspend_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_SUSPEND_BEFORE_CPU))
		tegra_console_uart_suspend();
}

static void n10_board_resume(int lp_state, enum resume_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_RESUME_AFTER_CPU))
		tegra_console_uart_resume();
} 

static struct tegra_suspend_platform_data n10_suspend_data = { /*ok*/
	/*
	 * Check power on time and crystal oscillator start time
	 * for appropriate settings.
	 */
	.cpu_timer	= 2000,
	.cpu_off_timer	= 100,
	.suspend_mode	= TEGRA_SUSPEND_LP0,
	.core_timer	= 0x7e7e,
	.core_off_timer = 0xf,
	.corereq_high	= false,
	.sysclkreq_high	= true,	
	.board_suspend = n10_board_suspend,
	.board_resume = n10_board_resume, 
};

/* Init power management unit of Tegra2 */
int __init n10_power_register_devices(void)
{
	int err;
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	void __iomem *chip_id = IO_ADDRESS(TEGRA_APB_MISC_BASE) + 0x804;
	u32 pmc_ctrl;
	u32 minor;

	minor = (readl(chip_id) >> 16) & 0xf;
	/* A03 (but not A03p) chips do not support LP0 */
	if (minor == 3 && !(tegra_spare_fuse(18) || tegra_spare_fuse(19)))
		n10_suspend_data.suspend_mode = TEGRA_SUSPEND_LP1;
	
	/* configure the power management controller to trigger PMU
	 * interrupts when low
	 */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	/* Reserve and init the unknown function gpios */
	

	gpio_request(N10_FORCE_RECOVERY,"Force recovery");
	gpio_direction_input(N10_FORCE_RECOVERY);

	gpio_request(N10_DOCKING_GPIO,"Docking GPIO");
	gpio_direction_input(N10_DOCKING_GPIO);

	gpio_request(N10_SW_PMU_MCU_RST,"Sw PMU MCU Reset");
	gpio_direction_output(N10_SW_PMU_MCU_RST,0);

	gpio_request(N10_AC_OK,"AC ok");
	gpio_direction_input(N10_AC_OK);

	gpio_request(N10_LOW_BATT,"Low battery");
	gpio_direction_input(N10_LOW_BATT);

	gpio_request(N10_T25_TEMP,"T25 temperature");
	gpio_direction_input(N10_T25_TEMP);
	
	gpio_request(N10_UNKFN_IN1,"unknown_in_1");
	gpio_direction_input(N10_UNKFN_IN1);
	
	err = i2c_register_board_info(4, n10_regulators, 1);
	if (err < 0) 
		pr_warning("Unable to initialize regulator\n");

		
	err = i2c_register_board_info(4, n10_shmcu_mfd, 1);
	if (err < 0) 
		pr_warning("Unable to shmcu mfd\n");

	/* signal that power regulators have fully specified constraints */
	//regulator_has_full_constraints();
	
	tegra_init_suspend(&n10_suspend_data);
	
	/* register all pm devices - This must come AFTER the registration of the TPS i2c interfase,
	   as we need the GPIO definitions exported by that driver */
	return platform_add_devices(n10_power_devices, ARRAY_SIZE(n10_power_devices));
}
