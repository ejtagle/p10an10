/*
 * arch/arm/mach-tegra/board-n10.h
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

#ifndef _MACH_TEGRA_BOARD_N10_H
#define _MACH_TEGRA_BOARD_N10_H

/* Config */
#define N10_RAM_SIZE			TEGRA_GPIO_PG3	/* IN: RAM size: 1=512M, 0=1G */
#define N10_RAM_VENDOR_1		TEGRA_GPIO_PG4	/* IN: RAM vendor */
#define N10_RAM_VENDOR_2		TEGRA_GPIO_PG5	/* IN: RAM vendor */
#define N10_NAND_VENDOR_1		TEGRA_GPIO_PG6	/* IN: NAND vendor */
#define N10_NAND_VENDOR_2		TEGRA_GPIO_PG7	/* IN: NAND vendor */

#define N10_FORCE_RECOVERY		TEGRA_GPIO_PI1	/* IN: (FORCE_RECOVERY#) */
#define N10_DOCKING_GPIO		TEGRA_GPIO_PP1	/* IN: DOCKING_GPIO: 0=docked */
#define N10_SW_PMU_MCU_RST		TEGRA_GPIO_PP2	/* OUT: System reset */

#define N10_UNKFN_IN1			TEGRA_GPIO_PH2	/* IN: tied to ground */

/* Bluetooth */
#define N10_BT_RST_SHUTDOWN_N 	TEGRA_GPIO_PU0 	/* OUT: BCM4329 0= reset asserted (BT_RST_SHUTDOWN#) */
#define N10_BT_IRQ_N			TEGRA_GPIO_PU6  /* IN:  BT_IRQ# */
#define N10_BT_WAKEUP			TEGRA_GPIO_PU1	/* OUT: BT_WAKEUP */

/* Touchscreen */
#define N10_TS_OFF_N			TEGRA_GPIO_PA2	/* OUT: 1=Powered UP (TP_OFF#) */
#define N10_TS_RESET			TEGRA_GPIO_PD4  /* OUT: 0=Reset (CTP_RST) */
#define N10_TS_RESET_STATE		TEGRA_GPIO_PD5	/* IN: Touchscreen reset state */
#define N10_TS_IRQ				TEGRA_GPIO_PV6	/* IN: TS IRQ (active low) (TOUCH_INT)*/

/* 3G */
#define N10_3G_ON				TEGRA_GPIO_PA3 	/* OUT: 0= 3G enabled, 1=3G disabled) (3G_ON)*/
#define N10_3G_BULK_TEGRA		TEGRA_GPIO_PA4 	/* OUT: 1= 3G reset, 0=3G ok (3G_BULK_TEGRA)*/
#define N10_3G_BOOST_TEGRA_N	TEGRA_GPIO_PT3 	/* OUT: (3G_BOOST#_TEGRA)*/
#define N10_3G_WAKE_DET			TEGRA_GPIO_PA5	/* IN: 3G Wake det */

/* Back camera */
#define N10_BACK_CAM_RST_N		TEGRA_GPIO_PD2  /* OUT: 0=reset : Def:1 (CAM_RST_N) */
#define N10_BACK_CAM_PDWN_N		TEGRA_GPIO_PBB5	/* OUT: 1=Powered # (CAM_PDWN) */
#define N10_BACK_CAM_PWR_EN_N		TEGRA_GPIO_PV4	/* OUT: 0=Powered # (CAM_PWR_EN_N) */
#define N10_BACK_CAM_USB_DET	TEGRA_GPIO_PL2  /* IN: 0=Backcam is USB (BACK_USB_DET) */
#define N10_BACK_CAM_5M_MIPI_DET TEGRA_GPIO_PL3	/* IN: backcam_5MMIPI 0=5Mpix (5M_MIPI_DET) */
#define N10_BACK_CAM_2M_MIPI_DET TEGRA_GPIO_PL4  /* IN: backcam_2MMIPI 0=2Mpix (2M_MIPI_DET) */

/* Front camera */
#define N10_FRONT_CAM_WEBCAM_ON_N TEGRA_GPIO_PO1 /* OUT: 1=USB cam powered (WEBCAM_ON#)*/
#define N10_FRONT_CAM_MIPI_TYPE	TEGRA_GPIO_PG0   /* IN: default 0 */

/* Keyboard */
#define N10_KEY_FIND			TEGRA_GPIO_PQ3 	/* IN: 0=pressed */
#define N10_KEY_HOME			TEGRA_GPIO_PQ1 	/* IN: 0=pressed */
#define N10_KEY_BACK			TEGRA_GPIO_PQ2	/* IN: 0=pressed */
#define N10_KEY_VOLUMEUP 		TEGRA_GPIO_PQ5 	/* IN: 0=pressed */
#define N10_KEY_VOLUMEDOWN 		TEGRA_GPIO_PQ4 	/* IN: 0=pressed */
#define N10_KEY_POWER 			TEGRA_GPIO_PV2 	/* IN: 0=pressed (AP_ONKEY#)*/
#define N10_KEY_MENU 			TEGRA_GPIO_PC7	/* IN: 0=pressed */
#define N10_KEY_UNK				TEGRA_GPIO_PQ7	/* IN: Connected to ground */

/* USB */
#define N10_TG_MINI_USB_ON_N	TEGRA_GPIO_PG1 	/* OUT: Host USB Power (TG_MINI_USB_ON#) 0=On */

/* GPS */
#define N10_GPS_DET				TEGRA_GPIO_PH3	/* IN: Atheros GPS (GPS_DET) */		
#define N10_GPS_RESET_N			TEGRA_GPIO_PS4	/* OUT: Atheros GPS: 0=reset (GPS_RESET#) */		

/* WLAN */
#define N10_WLAN_WF_RST_PWDN_N	TEGRA_GPIO_PK6 	/* OUT: WLAN reset: 0=reset [*] (WF_RST_PWDN#) */
#define N10_WLAN_WL_HOST_WAKE	TEGRA_GPIO_PS0  /* IN: WL_HOST_WAKE */

/* SDCARD */
#define N10_SDIO2_CD			TEGRA_GPIO_PI5	/* IN: SDIO2 change detect 0=Card detected */
#define N10_SDIO2_WP			TEGRA_GPIO_PH1	/* IN: SDIO2 write protect */
#define N10_SDIO2_POWER			TEGRA_GPIO_PI6  /* OUT: SDIO2 power (EN_VDDIO_SD) */

/* Audio jack */
#define N10_HP_DETECT			TEGRA_GPIO_PW2 	/* IN: HeadPhone detect for audio codec: 1=Hedphone plugged (HEAD_DET#)*/
#define N10_CODEC_IRQ			TEGRA_GPIO_PK5 	/* IN: Codec IRQ */

/* Video */
#define N10_LCD_DETECT			TEGRA_GPIO_PO0  /* IN: default 0 (LCD_DETECT) */
#define N10_LCD_SELECT1			TEGRA_GPIO_PO6  /* IN: default 0 (LCD_SELECT1) */
#define N10_LCD_SELECT2			TEGRA_GPIO_PO7  /* IN: default 0 (LCD_SELECT2) */
#define N10_LCD_BL_EN			TEGRA_GPIO_PV5  /* OUT: (LCD_BL_EN)*/ 
#define N10_EN_VDD_BL			TEGRA_GPIO_PX1  /* OUT: (EN_VDD_BL)*/
#define N10_EN_VDD_PNL			TEGRA_GPIO_PP0  /* OUT: (EN_VDD_PNL) */
#define N10_LVDS_SHDN_N			TEGRA_GPIO_PD0  /* OUT: Def:1 (LVDS_SHDN#)*/
#define N10_LVDS_SHDN2			TEGRA_GPIO_PX0	/* IN: (LVDS_SHDN2) */
#define N10_HDMI_HPD			TEGRA_GPIO_PN7  /* IN: 1=HDMI plug detected */
#define N10_BL_PWM_ID			2				/* PWM2 controls backlight */
#define N10_LCD_BL_PWM_1V8		TEGRA_GPIO_PU5	/* PWM pin */

/* Flash */
#define N10_LED_LIGHT			TEGRA_GPIO_PS3	/* OUT: 1=Power led (LED_LIGHT) */
#define N10_NEW_MIPI_LED_ON		TEGRA_GPIO_PT4	/* IN: Led is turned on */

/* Vibrator */
#define N10_SHAKE_ON			TEGRA_GPIO_PU2	/* OUT: 1=Vibrating (SHAKE_ON) */
#define N10_VIBRATE_DET			TEGRA_GPIO_PH0	/* IN: Vibrate detector */

/* Soft buttons led */
#define N10_LED_LOCK_OFF_N		TEGRA_GPIO_PV0	/* OUT: 1=Power led  (LED_LOCK_OFF#)*/

/* Sensor ISR */
#define N10_GYRO_IRQ			TEGRA_GPIO_PZ4	/* IN: Gyro interrupt (GYRO_INT) */
#define N10_ACCEL_IRQ			TEGRA_GPIO_PN4	/* IN: Accelerometer interrupt (ACCEL_INT) */
#define N10_COMPASS_IRQ			TEGRA_GPIO_PW0  /* IN: Compass interrupt (COMPASS_INT) */
#define N10_COMPASS_DRDY		TEGRA_GPIO_PN5	/* IN: Compass DRDY */
#define N10_ALS_IRQ				TEGRA_GPIO_PZ2	/* IN: */

/* Charger */
#define N10_DCIN_DET			TEGRA_GPIO_PY2	/* IN: 1=Charging (DCIN_DET) */
#define N10_LOW_BATT			TEGRA_GPIO_PW3	/* IN: Low battery */
#define N10_AC_OK				TEGRA_GPIO_PV3	/* IN: AC OK (0=ok) (AP_ACOK#) */
	
/* Model detection */
#define N10_MODEL0 				TEGRA_GPIO_PO2  /* IN: AUDIO-DET */
#define N10_MODEL1 				TEGRA_GPIO_PO3  /* IN: SENSOR-DET */
#define N10_MODEL2 				TEGRA_GPIO_PO4  /* IN: */

/* Temperature sensor */
#define N10_T25_TEMP			TEGRA_GPIO_PN6	/* IN: CPU Overtemperature */

/* #define N10_EMC_SAMSUNG		*/
/* #define N10_EMC_ELPIDA50NM	*/
/* #define N10_EMC_ELPIDA40NM	*/

/* Graphics */
#define N10_FB_PAGES		2				/* At least, 2 video pages */
#define N10_FB_BPP			32				/* bits per pixel */
#define N10_FB_HDMI_PAGES	2				/* At least, 2 video pages for HDMI */
#define N10_FB_HDMI_BPP		32				/* bits per pixel */

#define N10_MEM_SIZE 		SZ_512M			/* Total memory */

#define N10_GPU_MEM_SIZE 	SZ_256M		/* Memory reserved for GPU */
/*#define N10_GPU_MEM_SIZE 	SZ_128M*/	/* Memory reserved for GPU */
/*#define N10_GPU_MEM_SIZE 	SZ_64M*/		/* Memory reserved for GPU */
/*#define N10_GPU_MEM_SIZE 	(45*SZ_2M)*/	/* Memory reserved for GPU */

/*#define N10_FB1_MEM_SIZE   SZ_4M*/      	/* Memory reserved for Framebuffer 1: LCD */
/*#define N10_FB2_MEM_SIZE   SZ_8M*/      	/* Memory reserved for Framebuffer 2: HDMI out */

#define N10_FB1_MEM_SIZE   SZ_8M      		/* Memory reserved for Framebuffer 1: LCD */
#define N10_FB2_MEM_SIZE   SZ_16M      		/* Memory reserved for Framebuffer 2: HDMI out */


#define DYNAMIC_GPU_MEM 1						/* use dynamic memory for GPU */

/* LCD panel to use */
/* #define N10_1280x800PANEL_1 */
/* #define N10_1280x800PANEL_2 */
/* #define N10_1366x768PANEL */
#define N10_1024x600PANEL1 /* The P10AN01 default panel */
/* #define N10_1024x600PANEL2 */

/* maximum allowed HDMI resolution */
/*#define N10_1920x1080HDMI*/
#define N10_1280x720HDMI


#define N10_48KHZ_AUDIO /* <- define this if you want 48khz audio sampling rate instead of 44100Hz */

// TPS6586x GPIOs as registered 
#define TPS6586X_GPIO_BASE		(TEGRA_NR_GPIOS) 
#define N10_EN_VDD_5V 			(TPS6586X_GPIO_BASE)
#define N10_EN_VDD_1V2 			(TPS6586X_GPIO_BASE + 1) 
#define TPS6586X_GPIO2 			(TPS6586X_GPIO_BASE + 2)
#define TPS6586X_GPIO3 			(TPS6586X_GPIO_BASE + 3)

#define TPS6586X_IRQ_BASE		(TEGRA_NR_IRQS)
#define TPS6586X_IRQ_END		(TPS6586X_IRQ_BASE + 32) 
#define TPS6586X_IRQ_RTC_ALM1 	(TPS6586X_IRQ_BASE + TPS6586X_INT_RTC_ALM1)
#define TPS6586X_IRQ_USB_DET	(TPS6586X_IRQ_BASE + TPS6586X_INT_USB_DET)

#define SHMCU_GPIO_BASE			(TPS6586X_GPIO_BASE + 4) 
#define SHMCU_GPIO0 			(SHMCU_GPIO_BASE)

extern int n10_wifi_set_carddetect(int val);

extern void n10_init_emc(void);
extern void n10_pinmux_init(void);
extern void n10_clks_init(void);

extern int n10_lights_register_devices(void);
extern int n10_usb_register_devices(void);
extern int n10_audio_register_devices(void);
extern int n10_eeprom_register_devices(void);
extern int n10_gpu_register_devices(void);
extern int n10_uart_register_devices(void);
extern int n10_spi_register_devices(void);
extern int n10_aes_register_devices(void);
extern int n10_wdt_register_devices(void);
extern int n10_i2c_register_devices(void);
extern int n10_power_register_devices(void);
extern int n10_keyboard_register_devices(void);
extern int n10_touch_register_devices(void);
extern int n10_sdhci_register_devices(void);
extern int n10_sensors_register_devices(void);
extern int n10_wlan_pm_register_devices(void);
extern int n10_gps_pm_register_devices(void);
extern int n10_gsm_pm_register_devices(void);
extern int n10_bt_pm_register_devices(void);
extern int n10_nand_register_devices(void);
extern int n10_camera_pm_register_devices(void);
extern int n10_vibrator_register_devices(void);

/* Autocalculate framebuffer sizes */

#define TEGRA_ROUND_ALLOC(x) (((x) + 4095) & ((unsigned)(-4096)))

#if defined(N10_1280x800PANEL_1)
/* Panel same as Motorola Xoom (tm) */
/* Frame buffer size */
#	define N10_FB_SIZE TEGRA_ROUND_ALLOC(1280*800*(N10_FB_BPP/8)*N10_FB_PAGES)
#elif defined(N10_1280x800PANEL_2)
/* If using 1280x800 panel (panel upgrade) */
/* Frame buffer size */
#	define N10_FB_SIZE TEGRA_ROUND_ALLOC(1280*800*(N10_FB_BPP/8)*N10_FB_PAGES)
#elif defined(N10_1366x768PANEL)
/* Frame buffer size */
#	define N10_FB_SIZE TEGRA_ROUND_ALLOC(1368*768*(N10_FB_BPP/8)*N10_FB_PAGES)
#elif defined(N10_1024x600PANEL1)
/* If using 1024x600 panel (n10 default panel) */
/* Frame buffer size */
#	define N10_FB_SIZE TEGRA_ROUND_ALLOC(1024*600*(N10_FB_BPP/8)*N10_FB_PAGES)
#else
/* Frame buffer size */
#	define N10_FB_SIZE TEGRA_ROUND_ALLOC(1024*600*(N10_FB_BPP/8)*N10_FB_PAGES)
#endif

#if defined(N10_1920x1080HDMI)
/* Frame buffer size */
#	define N10_FB_HDMI_SIZE TEGRA_ROUND_ALLOC(1920*1080*(N10_FB_HDMI_BPP/8)*N10_FB_PAGES)
#else
#	define N10_FB_HDMI_SIZE TEGRA_ROUND_ALLOC(1280*720*(N10_FB_HDMI_BPP/8)*N10_FB_PAGES)
#endif


#define usleep(x) usleep_range(x,x);


/*
 UART2 GPS
 UART3 BT (W/RTS/CTS)
 UART4: Debug
 
 U5 = LCD_BL_PWM_1V
 
 
 DAP4=BT
 */

#endif

