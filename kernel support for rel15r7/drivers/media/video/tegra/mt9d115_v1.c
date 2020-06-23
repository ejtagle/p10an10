/*
 * kernel/drivers/media/video/tegra
 *
 * Aptina MT9D115 sensor driver
 *
 * Copyright (C) 2010 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define DEBUG 1

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <mach/gpio.h>
#include <media/mt9d115.h>
#include <media/tegra_camera.h>

#define CAMERA_EFFECT_OFF			0
#define CAMERA_EFFECT_MONO			1
#define CAMERA_EFFECT_SEPIA			2
#define CAMERA_EFFECT_NEGATIVE		3
#define CAMERA_EFFECT_SOLARIZE		4
#define CAMERA_EFFECT_POSTERIZE		5

#define CAMERA_WB_MODE_AWB			0
#define CAMERA_WB_MODE_INCANDESCENT	1
#define CAMERA_WB_MODE_SUNLIGHT		2
#define CAMERA_WB_MODE_FLUORESCENT	3
#define CAMERA_WB_MODE_CLOUDY		4

#define SENSOR_PREVIEW_MODE			0
#define SENSOR_SNAPSHOT_MODE		1

#define SENSOR_IOCTLEX_SET_ISO				_IOW('o', 21, int) 

#define CAMERA_ISO_SET_AUTO			0
#define CAMERA_ISO_SET_100			1
#define CAMERA_ISO_SET_200			2
#define CAMERA_ISO_SET_400			3

#define SENSOR_IOCTLEX_SET_ANTIBANDING		_IOW('o', 23, int) 

#define CAMERA_ANTIBANDING_SET_OFF	0
#define CAMERA_ANTIBANDING_SET_60HZ	1
#define CAMERA_ANTIBANDING_SET_50HZ	2
#define CAMERA_ANTIBANDING_SET_AUTO	3

#define SENSOR_IOCTLEX_SET_BRIGHTNESS		_IOW('o', 25, int) 

#define CAMERA_BRIGHTNESS_0			0
#define CAMERA_BRIGHTNESS_1			1
#define CAMERA_BRIGHTNESS_2			2
#define CAMERA_BRIGHTNESS_3			3
#define CAMERA_BRIGHTNESS_4			4
#define CAMERA_BRIGHTNESS_5			5
#define CAMERA_BRIGHTNESS_6			6
#define CAMERA_BRIGHTNESS_7			7

#define SENSOR_IOCTLEX_SET_SATURATION		_IOW('o', 27, int) 

#define CAMERA_SATURATION_0			0
#define CAMERA_SATURATION_1			1
#define CAMERA_SATURATION_2			2
#define CAMERA_SATURATION_3			3
#define CAMERA_SATURATION_4			4

#define SENSOR_IOCTLEX_SET_CONTRAST			_IOW('o', 29, int) 

#define CAMERA_CONTRAST_0			0
#define CAMERA_CONTRAST_1			1
#define CAMERA_CONTRAST_2			2
#define CAMERA_CONTRAST_3			3
#define CAMERA_CONTRAST_4			4

#define SENSOR_IOCTLEX_SET_SHARPNESS  		_IOW('o', 31, int) 

#define CAMERA_SHARPNESS_0			0
#define CAMERA_SHARPNESS_1			1
#define CAMERA_SHARPNESS_2			2
#define CAMERA_SHARPNESS_3			3
#define CAMERA_SHARPNESS_4			4

#define SENSOR_IOCTLEX_SET_EXPOSURE_COMP  	_IOW('o', 33, int) 

#define CAMERA_EXPOSURE_0			0
#define CAMERA_EXPOSURE_1			1
#define CAMERA_EXPOSURE_2			2
#define CAMERA_EXPOSURE_3			3
#define CAMERA_EXPOSURE_4			4


/*
 * CAMIO Input MCLK (MHz)
 *
 * MCLK: 6-54 MHz
 *
 * maximum frame rate: 
 * 15 fps at full resolution (JPEG),
 * 30 fps in preview mode
 *
 * when MCLK=40MHZ, PCLK=48MHZ (PLL is enabled by sensor)
 * when MCLK=48MHZ, PCLK=48MHZ (PLL is disabled by sensor)
 *
 * 54MHz is the maximum value accepted by sensor
 */
#define MT9D115_CAMIO_MCLK  24000000

/* Sensor I2C Device ID */
#define REG_MT9D115_MODEL_ID    	0x0000
#define MT9D115_MODEL_ID        	0x2580

/* Sensor I2C Device Sub ID */
#define REG_MT9D115_MODEL_ID_SUB    0x31FE
#define MT9D115_MODEL_ID_SUB        0x0011

/* SOC Registers */
#define REG_MT9D115_STANDBY_CONTROL	0x0018


#define REG_MT9D115_WIDTH      		0x2703

#define SENSOR_640_WIDTH_VAL  0x280
#define SENSOR_800_WIDTH_VAL  0x320
#define SENSOR_720_WIDTH_VAL  0x500
#define SENSOR_1600_WIDTH_VAL 0x640

/*-------------------------------------------Important---------------------------------------
 * for changing the SENSOR_NAME, you must need to change the owner of the device. For example
 * Please add /dev/mt9d115 0600 media camera in below file
 * ./device/nvidia/ventana/ueventd.ventana.rc
 * Otherwise, ioctl will get permission deny
 * -------------------------------------------Important--------------------------------------
*/

#define SENSOR_NAME     "mt9d115"
#define DEV(x)          "/dev/"x
#define SENSOR_PATH     DEV(SENSOR_NAME)
#define LOG_NAME(x)     "ImagerODM-"x
#define LOG_TAG         LOG_NAME(SENSOR_NAME)

#define SENSOR_WAIT_MS       0 /* special number to indicate this is wait time require */
#define SENSOR_TABLE_END     1 /* special number to indicate this is end of table */
#define SENSOR_MAX_RETRIES   3 /* max counter for retry I2C access */ 

struct sensor_reg {
	u16 addr;
	u16 val;
};

/* PLL Setup */
static struct sensor_reg const pll_tbl[] = {
#if 0
    {0x0014, 0x2545},		//PLL Control: BYPASS PLL = 9541
    {0x0010, 0x0110},		//PLL Dividers = 272
    {0x0012, 0x1FF7},		//PLL P Dividers = 8183
    {0x0014, 0x2547},		//PLL Control: PLL_ENABLE on = 9543
    {0x0014, 0x2447},		//PLL Control: SEL_LOCK_DET on = 9287
	{SENSOR_WAIT_MS, 1},	// delay=1
    {0x0014, 0x2047},		//PLL Control: PLL_BYPASS off = 8263
    {0x0014, 0x2046},		//PLL Control: = 8262
    {0x321C, 0x0003}, 		// OFIFO_CONTROL_STATUS
    {0x0018, 0x402D}, 		// STANDBY_CONTROL
    {0x0018, 0x402C}, 		// STANDBY_CONTROL
	{SENSOR_WAIT_MS, 20},	// delay=20
    //  POLL  STANDBY_CONTROL::STANDBY_DONE =>  0x01, 0x00
#else
	// reset the sensor
	{0x001A, 0x0001},
	{SENSOR_WAIT_MS, 10},
	{0x001A, 0x0000},
	{SENSOR_WAIT_MS, 50},

	// select output interface
	{0x001A, 0x0050},	// BITFIELD=0x001A, 0x0200, 0 // disable Parallel
	{0x001A, 0x0058},	// BITFIELD=0x001A, 0x0008, 1 // MIPI

	// program the on-chip PLL
	{0x0014, 0x21F9},	// PLL Control: BYPASS PLL = 8697

	// mipi timing for YUV422 (clk_txfifo_wr = 85/42.5Mhz; clk_txfifo_rd = 63.75Mhz)
	{0x0010, 0x0115},	// PLL Dividers = 277
	{0x0012, 0x00F5},	// wcd = 8

	{0x0014, 0x2545},	// PLL Control: TEST_BYPASS on = 9541
	{0x0014, 0x2547},	// PLL Control: PLL_ENABLE on = 9543
	{0x0014, 0x2447},	// PLL Control: SEL_LOCK_DET on
	{SENSOR_WAIT_MS, 1},
	{0x0014, 0x2047},	// PLL Control: PLL_BYPASS off
	{0x0014, 0x2046},	// PLL Control: TEST_BYPASS off

	// enable powerup stop
	{0x0018, 0x402D},	// LOAD=MCU Powerup Stop Enable
	{SENSOR_WAIT_MS, 10},

	{0x0018, 0x402C},	// LOAD=GO // release MCU from standby
	{SENSOR_WAIT_MS, 10},
#endif
	{SENSOR_TABLE_END, 0x0000}
};

/* Preview and Snapshot Setup: Preview set to 800x600 on Context A, Capture set to 1600x1200 on context B */
static struct sensor_reg const prev_snap_tbl[] = {

    // Baic Init
    {0x098C, 0xA117}, 	// MCU_ADDRESS [SEQ_PREVIEW_0_AE]
    {0x0990, 0x0002}, 	// MCU_DATA_0
    {0x098C, 0xA11D}, 	// MCU_ADDRESS [SEQ_PREVIEW_1_AE]
    {0x0990, 0x0002}, 	// MCU_DATA_0
    {0x098C, 0xA129}, 	// MCU_ADDRESS [SEQ_PREVIEW_3_AE]
    {0x0990, 0x0002}, 	// MCU_DATA_0
    {0x098C, 0xA24F}, 	// MCU_ADDRESS [AE_BASETARGET]
    {0x0990, 0x0046}, 	// MCU_DATA_0
    {0x098C, 0xA20C}, 	// MCU_ADDRESS [AE_MAX_INDEX]
    {0x0990, 0x000B}, 	// MCU_DATA_0
    {0x098C, 0xA216}, 	// MCU_ADDRESS
    {0x0990, 0x0091}, 	// MCU_DATA_0
    {0x098C, 0xA20E}, 	// MCU_ADDRESS [AE_MAX_VIRTGAIN]
    {0x0990, 0x0091}, 	// MCU_DATA_0
    {0x098C, 0x2212}, 	// MCU_ADDRESS [AE_MAX_DGAIN_AE1]
    {0x0990, 0x00A4}, 	// MCU_DATA_0
    {0x3210, 0x01B8}, 	// COLOR_PIPELINE_CONTROL
    {0x098C, 0xAB36}, 	// MCU_ADDRESS [HG_CLUSTERDC_TH]
    {0x0990, 0x0014}, 	// MCU_DATA_0
    {0x098C, 0x2B66}, 	// MCU_ADDRESS [HG_CLUSTER_DC_BM]
    {0x0990, 0x2AF8}, 	// MCU_DATA_0
    {0x098C, 0xAB20}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0080}, 	// MCU_DATA_0
    {0x098C, 0xAB24}, 	// MCU_ADDRESS [HG_LL_SAT2]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xAB21}, 	// MCU_ADDRESS [HG_LL_INTERPTHRESH1]
    {0x0990, 0x000A}, 	// MCU_DATA_0
    {0x098C, 0xAB25}, 	// MCU_ADDRESS [HG_LL_INTERPTHRESH2]
    {0x0990, 0x002A}, 	// MCU_DATA_0
    {0x098C, 0xAB22}, 	// MCU_ADDRESS [HG_LL_APCORR1]
    {0x0990, 0x0007}, 	// MCU_DATA_0
    {0x098C, 0xAB26}, 	// MCU_ADDRESS [HG_LL_APCORR2]
    {0x0990, 0x0001}, 	// MCU_DATA_0
    {0x098C, 0xAB23}, 	// MCU_ADDRESS [HG_LL_APTHRESH1]
    {0x0990, 0x0004}, 	// MCU_DATA_0
    {0x098C, 0xAB27}, 	// MCU_ADDRESS [HG_LL_APTHRESH2]
    {0x0990, 0x0009}, 	// MCU_DATA_0
    {0x098C, 0x2B28}, 	// MCU_ADDRESS [HG_LL_BRIGHTNESSSTART]
    {0x0990, 0x0BB8}, 	// MCU_DATA_0
    {0x098C, 0x2B2A}, 	// MCU_ADDRESS [HG_LL_BRIGHTNESSSTOP]
    {0x0990, 0x2968}, 	// MCU_DATA_0
    {0x098C, 0xAB2C}, 	// MCU_ADDRESS [HG_NR_START_R]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
    {0x098C, 0xAB30}, 	// MCU_ADDRESS [HG_NR_STOP_R]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
    {0x098C, 0xAB2D}, 	// MCU_ADDRESS [HG_NR_START_G]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
    {0x098C, 0xAB31}, 	// MCU_ADDRESS [HG_NR_STOP_G]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
    {0x098C, 0xAB2E}, 	// MCU_ADDRESS [HG_NR_START_B]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
    {0x098C, 0xAB32}, 	// MCU_ADDRESS [HG_NR_STOP_B]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
    {0x098C, 0xAB2F}, 	// MCU_ADDRESS [HG_NR_START_OL]
    {0x0990, 0x000A}, 	// MCU_DATA_0
    {0x098C, 0xAB33}, 	// MCU_ADDRESS [HG_NR_STOP_OL]
    {0x0990, 0x0006}, 	// MCU_DATA_0
    {0x098C, 0xAB34}, 	// MCU_ADDRESS [HG_NR_GAINSTART]
    {0x0990, 0x0020}, 	// MCU_DATA_0
    {0x098C, 0xAB35}, 	// MCU_ADDRESS [HG_NR_GAINSTOP]
    {0x0990, 0x0091}, 	// MCU_DATA_0
    {0x098C, 0xA765}, 	// MCU_ADDRESS [MODE_COMMONMODESETTINGS_FILTER_MODE]
    {0x0990, 0x0006}, 	// MCU_DATA_0
	
	// Gamma
    {0x098C, 0xAB37}, 	// MCU_ADDRESS [HG_GAMMA_MORPH_CTRL]
    {0x0990, 0x0003}, 	// MCU_DATA_0
    {0x098C, 0x2B38}, 	// MCU_ADDRESS [HG_GAMMASTARTMORPH]
    {0x0990, 0x2968}, 	// MCU_DATA_0
    {0x098C, 0x2B3A}, 	// MCU_ADDRESS [HG_GAMMASTOPMORPH]
    {0x0990, 0x2D50}, 	// MCU_DATA_0
    {0x098C, 0x2B62}, 	// MCU_ADDRESS [HG_FTB_START_BM]
    {0x0990, 0xFFFE}, 	// MCU_DATA_0
    {0x098C, 0x2B64}, 	// MCU_ADDRESS [HG_FTB_STOP_BM]
    {0x0990, 0xFFFF}, 	// MCU_DATA_0
    {0x098C, 0xAB4F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_0]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xAB50}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_1]
    {0x0990, 0x0013}, 	// MCU_DATA_0
    {0x098C, 0xAB51}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_2]
    {0x0990, 0x0027}, 	// MCU_DATA_0
    {0x098C, 0xAB52}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_3]
    {0x0990, 0x0043}, 	// MCU_DATA_0
    {0x098C, 0xAB53}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_4]
    {0x0990, 0x0068}, 	// MCU_DATA_0
    {0x098C, 0xAB54}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_5]
    {0x0990, 0x0081}, 	// MCU_DATA_0
    {0x098C, 0xAB55}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_6]
    {0x0990, 0x0093}, 	// MCU_DATA_0
    {0x098C, 0xAB56}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_7]
    {0x0990, 0x00A3}, 	// MCU_DATA_0
    {0x098C, 0xAB57}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_8]
    {0x0990, 0x00B0}, 	// MCU_DATA_0
    {0x098C, 0xAB58}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_9]
    {0x0990, 0x00BC}, 	// MCU_DATA_0
    {0x098C, 0xAB59}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_10]
    {0x0990, 0x00C7}, 	// MCU_DATA_0
    {0x098C, 0xAB5A}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_11]
    {0x0990, 0x00D1}, 	// MCU_DATA_0
    {0x098C, 0xAB5B}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_12]
    {0x0990, 0x00DA}, 	// MCU_DATA_0
    {0x098C, 0xAB5C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_13]
    {0x0990, 0x00E2}, 	// MCU_DATA_0
    {0x098C, 0xAB5D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_14]
    {0x0990, 0x00E9}, 	// MCU_DATA_0
    {0x098C, 0xAB5E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_15]
    {0x0990, 0x00EF}, 	// MCU_DATA_0
    {0x098C, 0xAB5F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_16]
    {0x0990, 0x00F4}, 	// MCU_DATA_0
    {0x098C, 0xAB60}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_17]
    {0x0990, 0x00FA}, 	// MCU_DATA_0
    {0x098C, 0xAB61}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_18]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
	
	// CCM
    {0x098C, 0x2306}, 	// MCU_ADDRESS [AWB_CCM_L_0]
    {0x0990, 0x01D6}, 	// MCU_DATA_0
    {0x098C, 0x2308}, 	// MCU_ADDRESS [AWB_CCM_L_1]
    {0x0990, 0xFF89}, 	// MCU_DATA_0
    {0x098C, 0x230A}, 	// MCU_ADDRESS [AWB_CCM_L_2]
    {0x0990, 0xFFA1}, 	// MCU_DATA_0
    {0x098C, 0x230C}, 	// MCU_ADDRESS [AWB_CCM_L_3]
    {0x0990, 0xFF73}, 	// MCU_DATA_0
    {0x098C, 0x230E}, 	// MCU_ADDRESS [AWB_CCM_L_4]
    {0x0990, 0x019C}, 	// MCU_DATA_0
    {0x098C, 0x2310}, 	// MCU_ADDRESS [AWB_CCM_L_5]
    {0x0990, 0xFFF1}, 	// MCU_DATA_0
    {0x098C, 0x2312}, 	// MCU_ADDRESS [AWB_CCM_L_6]
    {0x0990, 0xFFB0}, 	// MCU_DATA_0
    {0x098C, 0x2314}, 	// MCU_ADDRESS [AWB_CCM_L_7]
    {0x0990, 0xFF2D}, 	// MCU_DATA_0
    {0x098C, 0x2316}, 	// MCU_ADDRESS [AWB_CCM_L_8]
    {0x0990, 0x0223}, 	// MCU_DATA_0
    {0x098C, 0x2318}, 	// MCU_ADDRESS [AWB_CCM_L_9]
    {0x0990, 0x001C}, 	// MCU_DATA_0
    {0x098C, 0x231A}, 	// MCU_ADDRESS [AWB_CCM_L_10]
    {0x0990, 0x0048}, 	// MCU_DATA_0
    {0x098C, 0x2318}, 	// MCU_ADDRESS [AWB_CCM_L_9]
    {0x0990, 0x001C}, 	// MCU_DATA_0
    {0x098C, 0x231A}, 	// MCU_ADDRESS [AWB_CCM_L_10]
    {0x0990, 0x0038}, 	// MCU_DATA_0
    {0x098C, 0x2318}, 	// MCU_ADDRESS [AWB_CCM_L_9]
    {0x0990, 0x001E}, 	// MCU_DATA_0
    {0x098C, 0x231A}, 	// MCU_ADDRESS [AWB_CCM_L_10]
    {0x0990, 0x0038}, 	// MCU_DATA_0
    {0x098C, 0x2318}, 	// MCU_ADDRESS [AWB_CCM_L_9]
    {0x0990, 0x0022}, 	// MCU_DATA_0
    {0x098C, 0x231A}, 	// MCU_ADDRESS [AWB_CCM_L_10]
    {0x0990, 0x0038}, 	// MCU_DATA_0
    {0x098C, 0x2318}, 	// MCU_ADDRESS [AWB_CCM_L_9]
    {0x0990, 0x002C}, 	// MCU_DATA_0
    {0x098C, 0x231A}, 	// MCU_ADDRESS [AWB_CCM_L_10]
    {0x0990, 0x0038}, 	// MCU_DATA_0
    {0x098C, 0x2318}, 	// MCU_ADDRESS [AWB_CCM_L_9]
    {0x0990, 0x0024}, 	// MCU_DATA_0
    {0x098C, 0x231A}, 	// MCU_ADDRESS [AWB_CCM_L_10]
    {0x0990, 0x0038}, 	// MCU_DATA_0
    {0x098C, 0x231C}, 	// MCU_ADDRESS [AWB_CCM_RL_0]
    {0x0990, 0xFFCD}, 	// MCU_DATA_0
    {0x098C, 0x231E}, 	// MCU_ADDRESS [AWB_CCM_RL_1]
    {0x0990, 0x0023}, 	// MCU_DATA_0
    {0x098C, 0x2320}, 	// MCU_ADDRESS [AWB_CCM_RL_2]
    {0x0990, 0x0010}, 	// MCU_DATA_0
    {0x098C, 0x2322}, 	// MCU_ADDRESS [AWB_CCM_RL_3]
    {0x0990, 0x0026}, 	// MCU_DATA_0
    {0x098C, 0x2324}, 	// MCU_ADDRESS [AWB_CCM_RL_4]
    {0x0990, 0xFFE9}, 	// MCU_DATA_0
    {0x098C, 0x2326}, 	// MCU_ADDRESS [AWB_CCM_RL_5]
    {0x0990, 0xFFF1}, 	// MCU_DATA_0
    {0x098C, 0x2328}, 	// MCU_ADDRESS [AWB_CCM_RL_6]
    {0x0990, 0x003A}, 	// MCU_DATA_0
    {0x098C, 0x232A}, 	// MCU_ADDRESS [AWB_CCM_RL_7]
    {0x0990, 0x005D}, 	// MCU_DATA_0
    {0x098C, 0x232C}, 	// MCU_ADDRESS [AWB_CCM_RL_8]
    {0x0990, 0xFF69}, 	// MCU_DATA_0
    {0x098C, 0x232E}, 	// MCU_ADDRESS [AWB_CCM_RL_9]
    {0x0990, 0x000C}, 	// MCU_DATA_0
    {0x098C, 0x2330}, 	// MCU_ADDRESS [AWB_CCM_RL_10]
    {0x0990, 0xFFE4}, 	// MCU_DATA_0
    {0x098C, 0x232E}, 	// MCU_ADDRESS [AWB_CCM_RL_9]
    {0x0990, 0x000C}, 	// MCU_DATA_0
    {0x098C, 0x2330}, 	// MCU_ADDRESS [AWB_CCM_RL_10]
    {0x0990, 0xFFF4}, 	// MCU_DATA_0
    {0x098C, 0x232E}, 	// MCU_ADDRESS [AWB_CCM_RL_9]
    {0x0990, 0x000A}, 	// MCU_DATA_0
    {0x098C, 0x2330}, 	// MCU_ADDRESS [AWB_CCM_RL_10]
    {0x0990, 0xFFF4}, 	// MCU_DATA_0
    {0x098C, 0x232E}, 	// MCU_ADDRESS [AWB_CCM_RL_9]
    {0x0990, 0x0006}, 	// MCU_DATA_0
    {0x098C, 0x2330}, 	// MCU_ADDRESS [AWB_CCM_RL_10]
    {0x0990, 0xFFF4}, 	// MCU_DATA_0
    {0x098C, 0x232E}, 	// MCU_ADDRESS [AWB_CCM_RL_9]
    {0x0990, 0xFFFC}, 	// MCU_DATA_0
    {0x098C, 0x2330}, 	// MCU_ADDRESS [AWB_CCM_RL_10]
    {0x0990, 0xFFF4}, 	// MCU_DATA_0
    {0x098C, 0x232E}, 	// MCU_ADDRESS [AWB_CCM_RL_9]
    {0x0990, 0x0004}, 	// MCU_DATA_0
    {0x098C, 0x2330}, 	// MCU_ADDRESS [AWB_CCM_RL_10]
    {0x0990, 0xFFF4}, 	// MCU_DATA_0

	// SOC2031 patch
    {0x098C, 0x0415}, 	// MCU_ADDRESS
    {0x0990, 0xF601},
    {0x0992, 0x42C1},
    {0x0994, 0x0326},
    {0x0996, 0x11F6},
    {0x0998, 0x0143},
    {0x099A, 0xC104},
    {0x099C, 0x260A},
    {0x099E, 0xCC04},
    {0x098C, 0x0425}, 	// MCU_ADDRESS
    {0x0990, 0x33BD},
    {0x0992, 0xA362},
    {0x0994, 0xBD04},
    {0x0996, 0x3339},
    {0x0998, 0xC6FF},
    {0x099A, 0xF701},
    {0x099C, 0x6439},
    {0x099E, 0xFE01},
    {0x098C, 0x0435}, 	// MCU_ADDRESS
    {0x0990, 0x6918},
    {0x0992, 0xCE03},
    {0x0994, 0x25CC},
    {0x0996, 0x0013},
    {0x0998, 0xBDC2},
    {0x099A, 0xB8CC},
    {0x099C, 0x0489},
    {0x099E, 0xFD03},
    {0x098C, 0x0445}, 	// MCU_ADDRESS
    {0x0990, 0x27CC},
    {0x0992, 0x0325},
    {0x0994, 0xFD01},
    {0x0996, 0x69FE},
    {0x0998, 0x02BD},
    {0x099A, 0x18CE},
    {0x099C, 0x0339},
    {0x099E, 0xCC00},
    {0x098C, 0x0455}, 	// MCU_ADDRESS
    {0x0990, 0x11BD},
    {0x0992, 0xC2B8},
    {0x0994, 0xCC04},
    {0x0996, 0xC8FD},
    {0x0998, 0x0347},
    {0x099A, 0xCC03},
    {0x099C, 0x39FD},
    {0x099E, 0x02BD},
    {0x098C, 0x0465}, 	// MCU_ADDRESS
    {0x0990, 0xDE00},
    {0x0992, 0x18CE},
    {0x0994, 0x00C2},
    {0x0996, 0xCC00},
    {0x0998, 0x37BD},
    {0x099A, 0xC2B8},
    {0x099C, 0xCC04},
    {0x099E, 0xEFDD},
    {0x098C, 0x0475}, 	// MCU_ADDRESS
    {0x0990, 0xE6CC},
    {0x0992, 0x00C2},
    {0x0994, 0xDD00},
    {0x0996, 0xC601},
    {0x0998, 0xF701},
    {0x099A, 0x64C6},
    {0x099C, 0x03F7},
    {0x099E, 0x0165},
    {0x098C, 0x0485}, 	// MCU_ADDRESS
    {0x0990, 0x7F01},
    {0x0992, 0x6639},
    {0x0994, 0x3C3C},
    {0x0996, 0x3C34},
    {0x0998, 0xCC32},
    {0x099A, 0x3EBD},
    {0x099C, 0xA558},
    {0x099E, 0x30ED},
    {0x098C, 0x0495}, 	// MCU_ADDRESS
    {0x0990, 0x04BD},
    {0x0992, 0xB2D7},
    {0x0994, 0x30E7},
    {0x0996, 0x06CC},
    {0x0998, 0x323E},
    {0x099A, 0xED00},
    {0x099C, 0xEC04},
    {0x099E, 0xBDA5},
    {0x098C, 0x04A5}, 	// MCU_ADDRESS
    {0x0990, 0x44CC},
    {0x0992, 0x3244},
    {0x0994, 0xBDA5},
    {0x0996, 0x585F},
    {0x0998, 0x30ED},
    {0x099A, 0x02CC},
    {0x099C, 0x3244},
    {0x099E, 0xED00},
    {0x098C, 0x04B5}, 	// MCU_ADDRESS
    {0x0990, 0xF601},
    {0x0992, 0xD54F},
    {0x0994, 0xEA03},
    {0x0996, 0xAA02},
    {0x0998, 0xBDA5},
    {0x099A, 0x4430},
    {0x099C, 0xE606},
    {0x099E, 0x3838},
    {0x098C, 0x04C5}, 	// MCU_ADDRESS
    {0x0990, 0x3831},
    {0x0992, 0x39BD},
    {0x0994, 0xD661},
    {0x0996, 0xF602},
    {0x0998, 0xF4C1},
    {0x099A, 0x0126},
    {0x099C, 0x0BFE},
    {0x099E, 0x02BD},
    {0x098C, 0x04D5}, 	// MCU_ADDRESS
    {0x0990, 0xEE10},
    {0x0992, 0xFC02},
    {0x0994, 0xF5AD},
    {0x0996, 0x0039},
    {0x0998, 0xF602},
    {0x099A, 0xF4C1},
    {0x099C, 0x0226},
    {0x099E, 0x0AFE},
    {0x098C, 0x04E5}, 	// MCU_ADDRESS
    {0x0990, 0x02BD},
    {0x0992, 0xEE10},
    {0x0994, 0xFC02},
    {0x0996, 0xF7AD},
    {0x0998, 0x0039},
    {0x099A, 0x3CBD},
    {0x099C, 0xB059},
    {0x099E, 0xCC00},
    {0x098C, 0x04F5}, 	// MCU_ADDRESS
    {0x0990, 0x28BD},
    {0x0992, 0xA558},
    {0x0994, 0x8300},
    {0x0996, 0x0027},
    {0x0998, 0x0BCC},
    {0x099A, 0x0026},
    {0x099C, 0x30ED},
    {0x099E, 0x00C6},
    {0x098C, 0x0505}, 	// MCU_ADDRESS
    {0x0990, 0x03BD},
    {0x0992, 0xA544},
    {0x0994, 0x3839},
    {0x098C, 0x2006}, 	// MCU_ADDRESS [MON_ARG1]
    {0x0990, 0x0415}, 	// MCU_DATA_0
    {0x098C, 0xA005}, 	// MCU_ADDRESS [MON_CMD]
    {0x0990, 0x0001}, 	// MCU_DATA_0
	{SENSOR_WAIT_MS, 200},	// delay=200

	// Lens register settings for A-2031SOC (MT9D115) REV1
    {0x364E, 0x0770}, 	// P_GR_P0Q0
    {0x3650, 0xD48A}, 	// P_GR_P0Q1
    {0x3652, 0x4E71}, 	// P_GR_P0Q2
    {0x3654, 0xBB10}, 	// P_GR_P0Q3
    {0x3656, 0x8C33}, 	// P_GR_P0Q4
    {0x3658, 0x00F0}, 	// P_RD_P0Q0
    {0x365A, 0xDE8C}, 	// P_RD_P0Q1
    {0x365C, 0x6791}, 	// P_RD_P0Q2
    {0x365E, 0xBD0F}, 	// P_RD_P0Q3
    {0x3660, 0x89D3}, 	// P_RD_P0Q4
    {0x3662, 0x00D0}, 	// P_BL_P0Q0
    {0x3664, 0x934B}, 	// P_BL_P0Q1
    {0x3666, 0x4E91}, 	// P_BL_P0Q2
    {0x3668, 0x96F0}, 	// P_BL_P0Q3
    {0x366A, 0xA5B3}, 	// P_BL_P0Q4
    {0x366C, 0x00D0}, 	// P_GB_P0Q0
    {0x366E, 0xE72C}, 	// P_GB_P0Q1
    {0x3670, 0x4F31}, 	// P_GB_P0Q2
    {0x3672, 0x8290}, 	// P_GB_P0Q3
    {0x3674, 0x8653}, 	// P_GB_P0Q4
    {0x3676, 0xF8CA}, 	// P_GR_P1Q0
    {0x3678, 0x93D0}, 	// P_GR_P1Q1
    {0x367A, 0x6BB0}, 	// P_GR_P1Q2
    {0x367C, 0x2112}, 	// P_GR_P1Q3
    {0x367E, 0xBEAF}, 	// P_GR_P1Q4
    {0x3680, 0x17CD}, 	// P_RD_P1Q0
    {0x3682, 0x5FCF}, 	// P_RD_P1Q1
    {0x3684, 0x0270}, 	// P_RD_P1Q2
    {0x3686, 0x9D92}, 	// P_RD_P1Q3
    {0x3688, 0xB210}, 	// P_RD_P1Q4
    {0x368A, 0xD48C}, 	// P_BL_P1Q0
    {0x368C, 0xBA8F}, 	// P_BL_P1Q1
    {0x368E, 0x4250}, 	// P_BL_P1Q2
    {0x3690, 0x0272}, 	// P_BL_P1Q3
    {0x3692, 0x9271}, 	// P_BL_P1Q4
    {0x3694, 0x90CB}, 	// P_GB_P1Q0
    {0x3696, 0x1290}, 	// P_GB_P1Q1
    {0x3698, 0x274D}, 	// P_GB_P1Q2
    {0x369A, 0xA1B2}, 	// P_GB_P1Q3
    {0x369C, 0x0992}, 	// P_GB_P1Q4
    {0x369E, 0x4A92}, 	// P_GR_P2Q0
    {0x36A0, 0xA7F1}, 	// P_GR_P2Q1
    {0x36A2, 0xD074}, 	// P_GR_P2Q2
    {0x36A4, 0x22B2}, 	// P_GR_P2Q3
    {0x36A6, 0x3351}, 	// P_GR_P2Q4
    {0x36A8, 0x4232}, 	// P_RD_P2Q0
    {0x36AA, 0x8E10}, 	// P_RD_P2Q1
    {0x36AC, 0x8374}, 	// P_RD_P2Q2
    {0x36AE, 0xC791}, 	// P_RD_P2Q3
    {0x36B0, 0xF055}, 	// P_RD_P2Q4
    {0x36B2, 0x31D2}, 	// P_BL_P2Q0
    {0x36B4, 0x9011}, 	// P_BL_P2Q1
    {0x36B6, 0xB6B4}, 	// P_BL_P2Q2
    {0x36B8, 0x0473}, 	// P_BL_P2Q3
    {0x36BA, 0x8892}, 	// P_BL_P2Q4
    {0x36BC, 0x49D2}, 	// P_GB_P2Q0
    {0x36BE, 0xC910}, 	// P_GB_P2Q1
    {0x36C0, 0xAFB4}, 	// P_GB_P2Q2
    {0x36C2, 0xDE8D}, 	// P_GB_P2Q3
    {0x36C4, 0x88B5}, 	// P_GB_P2Q4
    {0x36C6, 0x0611}, 	// P_GR_P3Q0
    {0x36C8, 0x4AD1}, 	// P_GR_P3Q1
    {0x36CA, 0x3B13}, 	// P_GR_P3Q2
    {0x36CC, 0xA410}, 	// P_GR_P3Q3
    {0x36CE, 0xBA76}, 	// P_GR_P3Q4
    {0x36D0, 0x2451}, 	// P_RD_P3Q0
    {0x36D2, 0x8392}, 	// P_RD_P3Q1
    {0x36D4, 0x718F}, 	// P_RD_P3Q2
    {0x36D6, 0x4C51}, 	// P_RD_P3Q3
    {0x36D8, 0xC295}, 	// P_RD_P3Q4
    {0x36DA, 0x1551}, 	// P_BL_P3Q0
    {0x36DC, 0x652F}, 	// P_BL_P3Q1
    {0x36DE, 0x3392}, 	// P_BL_P3Q2
    {0x36E0, 0xE8D1}, 	// P_BL_P3Q3
    {0x36E2, 0x9876}, 	// P_BL_P3Q4
    {0x36E4, 0x0B11}, 	// P_GB_P3Q0
    {0x36E6, 0xAE12}, 	// P_GB_P3Q1
    {0x36E8, 0x0394}, 	// P_GB_P3Q2
    {0x36EA, 0x7171}, 	// P_GB_P3Q3
    {0x36EC, 0xFDF6}, 	// P_GB_P3Q4
    {0x36EE, 0xAD54}, 	// P_GR_P4Q0
    {0x36F0, 0x6C8C}, 	// P_GR_P4Q1
    {0x36F2, 0x8917}, 	// P_GR_P4Q2
    {0x36F4, 0x2BF6}, 	// P_GR_P4Q3
    {0x36F6, 0x2BBA}, 	// P_GR_P4Q4
    {0x36F8, 0x91D4}, 	// P_RD_P4Q0
    {0x36FA, 0x8DD3}, 	// P_RD_P4Q1
    {0x36FC, 0xDD57}, 	// P_RD_P4Q2
    {0x36FE, 0x21B7}, 	// P_RD_P4Q3
    {0x3700, 0x5CFA}, 	// P_RD_P4Q4
    {0x3702, 0xA474}, 	// P_BL_P4Q0
    {0x3704, 0x02F2}, 	// P_BL_P4Q1
    {0x3706, 0xA0B7}, 	// P_BL_P4Q2
    {0x3708, 0x7075}, 	// P_BL_P4Q3
    {0x370A, 0x3DFA}, 	// P_BL_P4Q4
    {0x370C, 0xA8B4}, 	// P_GB_P4Q0
    {0x370E, 0xD512}, 	// P_GB_P4Q1
    {0x3710, 0xBEB7}, 	// P_GB_P4Q2
    {0x3712, 0x1EB7}, 	// P_GB_P4Q3
    {0x3714, 0x5C3A}, 	// P_GB_P4Q4
    {0x3644, 0x0338}, 	// POLY_ORIGIN_C
    {0x3642, 0x0240}, 	// POLY_ORIGIN_R
    {0x3210, 0x01B8}, 	// COLOR_PIPELINE_CONTROL

    {0x321C, 0x0003},	//By Pass TxFIFO = 3
    {0x098C, 0x2703},	//Output Width (A)
    {0x0990, 0x0320},	//      = 800
    {0x098C, 0x2705},	//Output Height (A)
    {0x0990, 0x0258},	//      = 600
    {0x098C, 0x2707},	//Output Width (B)
    {0x0990, 0x0640},	//      = 1600
    {0x098C, 0x2709},	//Output Height (B)
    {0x0990, 0x04B0},	//      = 1200
    {0x098C, 0x270D},	//Row Start (A)
    {0x0990, 0x0000},	//      = 0
    {0x098C, 0x270F},	//Column Start (A)
    {0x0990, 0x0000},	//      = 0
    {0x098C, 0x2711},	//Row End (A)
    {0x0990, 0x04BD},	//      = 1213
    {0x098C, 0x2713},	//Column End (A)
    {0x0990, 0x064D},	//      = 1613
    {0x098C, 0x2715},	//Row Speed (A)
    {0x0990, 0x0111},	//      = 273

                                                                    
#if defined(CONFIG_MACH_TURIES)                                            
    {0x098C, 0x2717}, // MCU_ADDRESS [MODE_SENSOR_READ_MODE_A]
    {0x0990, 0x046C}, // MCU_DATA_0 for vertical orientation  
#else                                                                      
    {0x098C, 0x2717}, // MCU_ADDRESS [MODE_SENSOR_READ_MODE_A]
    {0x0990, 0x046F}, // MCU_DATA_0                           
#endif
	
    {0x098C, 0x2719},	//sensor_fine_correction (A)
    {0x0990, 0x005A},	//      = 90
    {0x098C, 0x271B},	//sensor_fine_IT_min (A)
    {0x0990, 0x01BE},	//      = 446
    {0x098C, 0x271D},	//sensor_fine_IT_max_margin (A)
    {0x0990, 0x0131},	//      = 305
    {0x098C, 0x271F},	//Frame Lines (A)
    {0x0990, 0x02B3},	//      = 691
    {0x098C, 0x2721},	//Line Length (A)
    {0x0990, 0x074E},	//      = 1870
    {0x098C, 0x2723},	//Row Start (B)
    {0x0990, 0x0004},	//      = 4
    {0x098C, 0x2725},	//Column Start (B)
    {0x0990, 0x0004},	//      = 4
    {0x098C, 0x2727},	//Row End (B)
    {0x0990, 0x04BB},	//      = 1211
    {0x098C, 0x2729},	//Column End (B)
    {0x0990, 0x064B},	//      = 1611
    {0x098C, 0x272B},	//Row Speed (B)
    {0x0990, 0x0111},	//      = 273

  
#if defined(CONFIG_MACH_TURIES)
    {0x098C, 0x272D},  // MCU_ADDRESS [MODE_SENSOR_READ_MODE_B]
    {0x0990, 0x0024},  // MCU_DATA_0 for vertical orientation
#else
    {0x098C, 0x272D},  // MCU_ADDRESS [MODE_SENSOR_READ_MODE_B]
    {0x0990, 0x0027},  // MCU_DATA_0
#endif
	
    {0x098C, 0x272F},	//sensor_fine_correction (B)
    {0x0990, 0x003A},	//      = 58
    {0x098C, 0x2731},	//sensor_fine_IT_min (B)
    {0x0990, 0x00F6},	//      = 246
    {0x098C, 0x2733},	//sensor_fine_IT_max_margin (B)
    {0x0990, 0x008B},	//      = 139
    {0x098C, 0x2735},	//Frame Lines (B)
    {0x0990, 0x059A},	//      = 1434
    {0x098C, 0x2737},	//Line Length (B)
    {0x0990, 0x07BC},	//      = 1980
    {0x098C, 0x2739},	//Crop_X0 (A)
    {0x0990, 0x0000},	//      = 0
    {0x098C, 0x273B},	//Crop_X1 (A)
    {0x0990, 0x031F},	//      = 799
    {0x098C, 0x273D},	//Crop_Y0 (A)
    {0x0990, 0x0000},	//      = 0
    {0x098C, 0x273F},	//Crop_Y1 (A)
    {0x0990, 0x0257},	//      = 599
    {0x098C, 0x2747},	//Crop_X0 (B)
    {0x0990, 0x0000},	//      = 0
    {0x098C, 0x2749},	//Crop_X1 (B)
    {0x0990, 0x063F},	//      = 1599
    {0x098C, 0x274B},	//Crop_Y0 (B)
    {0x0990, 0x0000},	//      = 0
    {0x098C, 0x274D},	//Crop_Y1 (B)
    {0x0990, 0x04AF},	//      = 1199
    {0x098C, 0x222D},	//R9 Step
    {0x0990, 0x006B},	//      = 107
    {0x098C, 0xA408},	//search_f1_50
    {0x0990, 0x0019},	//      = 25
    {0x098C, 0xA409},	//search_f2_50
    {0x0990, 0x001B},	//      = 27
    {0x098C, 0xA40A},	//search_f1_60
    {0x0990, 0x001F},	//      = 31
    {0x098C, 0xA40B},	//search_f2_60
    {0x0990, 0x0021},	//      = 33
    {0x098C, 0x2411},	//R9_Step_60 (A)
    {0x0990, 0x006B},	//      = 107
    {0x098C, 0x2413},	//R9_Step_50 (A)
    {0x0990, 0x0080},	//      = 128
    {0x098C, 0x2415},	//R9_Step_60 (B)
    {0x0990, 0x0065},	//      = 101
    {0x098C, 0x2417},	//R9_Step_50 (B)
    {0x0990, 0x0079},	//      = 121
    {0x098C, 0xA404},	//FD Mode
    {0x0990, 0x0010},	//      = 16
    {0x098C, 0xA40D},	//Stat_min
    {0x0990, 0x0002},	//      = 2
    {0x098C, 0xA40E},	//Stat_max
    {0x0990, 0x0003},	//      = 3
    {0x098C, 0xA410},	//Min_amplitude
    {0x0990, 0x000A},	//      = 10

    //Sharpness
    {0x098C, 0xAB22},	// MCU_ADDRESS
    {0x0990, 0x0003},	// MCU_DATA_0


    // Set normal mode,  Frame rate >=11fps
    {0x098C, 0xA20C}, 	// MCU_ADDRESS [AE_MAX_INDEX]
    {0x0990, 0x000a}, 	// MCU_DATA_0
    {0x098C, 0xA215}, 	// MCU_ADDRESS [AE_INDEX_TH23]
    {0x0990, 0x0008}, 	// MCU_DATA_0

    // Saturation
    {0x098C, 0xAB20}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0068}, 	// MCU_DATA_0


    //AWB
    {0x098C, 0xA34A}, 	// MCU_ADDRESS [AWB_GAIN_MIN]
    {0x0990, 0x0059}, 	// MCU_DATA_0
    {0x098C, 0xA34B}, 	// MCU_ADDRESS [AWB_GAIN_MAX]
    {0x0990, 0x00B6}, 	// MCU_DATA_0
    {0x098C, 0xA34C}, 	// MCU_ADDRESS [AWB_GAINMIN_B]
    {0x0990, 0x0059}, 	// MCU_DATA_0
    {0x098C, 0xA34D}, 	// MCU_ADDRESS [AWB_GAINMAX_B]
    {0x0990, 0x00B5}, 	// MCU_DATA_0
    {0x098C, 0xA351}, 	// MCU_ADDRESS [AWB_CCM_POSITION_MIN]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xA352}, 	// MCU_ADDRESS [AWB_CCM_POSITION_MAX]
    {0x0990, 0x007F}, 	// MCU_DATA_0
    
    // Gamma 0.40  Black 5  C 1.35
    {0x098C, 0xAB4F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_0]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xAB50}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_1]
    {0x0990, 0x000C}, 	// MCU_DATA_0
    {0x098C, 0xAB51}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_2]
    {0x0990, 0x0022}, 	// MCU_DATA_0
    {0x098C, 0xAB52}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_3]
    {0x0990, 0x003F}, 	// MCU_DATA_0
    {0x098C, 0xAB53}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_4]
    {0x0990, 0x0062}, 	// MCU_DATA_0
    {0x098C, 0xAB54}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_5]
    {0x0990, 0x007D}, 	// MCU_DATA_0
    {0x098C, 0xAB55}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_6]
    {0x0990, 0x0093}, 	// MCU_DATA_0
    {0x098C, 0xAB56}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_7]
    {0x0990, 0x00A5}, 	// MCU_DATA_0
    {0x098C, 0xAB57}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_8]
    {0x0990, 0x00B3}, 	// MCU_DATA_0
    {0x098C, 0xAB58}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_9]
    {0x0990, 0x00BF}, 	// MCU_DATA_0
    {0x098C, 0xAB59}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_10]
    {0x0990, 0x00C9}, 	// MCU_DATA_0
    {0x098C, 0xAB5A}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_11]
    {0x0990, 0x00D3}, 	// MCU_DATA_0
    {0x098C, 0xAB5B}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_12]
    {0x0990, 0x00DB}, 	// MCU_DATA_0
    {0x098C, 0xAB5C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_13]
    {0x0990, 0x00E2}, 	// MCU_DATA_0
    {0x098C, 0xAB5D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_14]
    {0x0990, 0x00E9}, 	// MCU_DATA_0
    {0x098C, 0xAB5E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_15]
    {0x0990, 0x00EF}, 	// MCU_DATA_0
    {0x098C, 0xAB5F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_16]
    {0x0990, 0x00F5}, 	// MCU_DATA_0
    {0x098C, 0xAB60}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_17]
    {0x0990, 0x00FA}, 	// MCU_DATA_0
    {0x098C, 0xAB61}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_18]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
    {0x098C, 0xAB3C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_0]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xAB3D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_1]
    {0x0990, 0x000C}, 	// MCU_DATA_0
    {0x098C, 0xAB3E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_2]
    {0x0990, 0x0022}, 	// MCU_DATA_0
    {0x098C, 0xAB3F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_3]
    {0x0990, 0x003F}, 	// MCU_DATA_0
    {0x098C, 0xAB40}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_4]
    {0x0990, 0x0062}, 	// MCU_DATA_0
    {0x098C, 0xAB41}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_5]
    {0x0990, 0x007D}, 	// MCU_DATA_0
    {0x098C, 0xAB42}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_6]
    {0x0990, 0x0093}, 	// MCU_DATA_0
    {0x098C, 0xAB43}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_7]
    {0x0990, 0x00A5}, 	// MCU_DATA_0
    {0x098C, 0xAB44}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_8]
    {0x0990, 0x00B3}, 	// MCU_DATA_0
    {0x098C, 0xAB45}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_9]
    {0x0990, 0x00BF}, 	// MCU_DATA_0
    {0x098C, 0xAB46}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_10]
    {0x0990, 0x00C9}, 	// MCU_DATA_0
    {0x098C, 0xAB47}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_11]
    {0x0990, 0x00D3}, 	// MCU_DATA_0
    {0x098C, 0xAB48}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_12]
    {0x0990, 0x00DB}, 	// MCU_DATA_0
    {0x098C, 0xAB49}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_13]
    {0x0990, 0x00E2}, 	// MCU_DATA_0
    {0x098C, 0xAB4A}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_14]
    {0x0990, 0x00E9}, 	// MCU_DATA_0
    {0x098C, 0xAB4B}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_15]
    {0x0990, 0x00EF}, 	// MCU_DATA_0
    {0x098C, 0xAB4C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_16]
    {0x0990, 0x00F5}, 	// MCU_DATA_0
    {0x098C, 0xAB4D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_17]
    {0x0990, 0x00FA}, 	// MCU_DATA_0
    {0x098C, 0xAB4E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_18]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
  
#if defined(CONFIG_MACH_V9)
    {0x001E, 0x0777}, 
#endif
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg wb_cloudy_tbl[] = { /*ok*/
    {0x098C, 0xA34A}, 	// MCU_ADDRESS [AWB_GAIN_MIN]
    {0x0990, 0x00A2}, 	// MCU_DATA_0
    {0x098C, 0xA34B}, 	// MCU_ADDRESS [AWB_GAIN_MAX]
    {0x0990, 0x00A6}, 	// MCU_DATA_0
    {0x098C, 0xA34C}, 	// MCU_ADDRESS [AWB_GAINMIN_B]
    {0x0990, 0x0072}, 	// MCU_DATA_0
    {0x098C, 0xA34D}, 	// MCU_ADDRESS [AWB_GAINMAX_B]
    {0x0990, 0x0074}, 	// MCU_DATA_0
    {0x098C, 0xA351}, 	// MCU_ADDRESS [AWB_CCM_POSITION_MIN]
    {0x0990, 0x007f}, 	// MCU_DATA_0
    {0x098C, 0xA352}, 	// MCU_ADDRESS [AWB_CCM_POSITION_MAX]
    {0x0990, 0x007F}, 	// MCU_DATA_0
    {0x098C, 0xA103}, 	// MCU_ADDRESS [SEQ_CMD]
    {0x0990, 0x0005}, 	// MCU_DATA_
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg wb_daylight_tbl[] = { /*ok*/
    {0x098C, 0xA34A}, 	// MCU_ADDRESS [AWB_GAIN_MIN]
    {0x0990, 0x0099}, 	// MCU_DATA_0
    {0x098C, 0xA34B}, 	// MCU_ADDRESS [AWB_GAIN_MAX]
    {0x0990, 0x009B}, 	// MCU_DATA_0
    {0x098C, 0xA34C}, 	// MCU_ADDRESS [AWB_GAINMIN_B]
    {0x0990, 0x007E}, 	// MCU_DATA_0
    {0x098C, 0xA34D}, 	// MCU_ADDRESS [AWB_GAINMAX_B]
    {0x0990, 0x0080}, 	// MCU_DATA_0
    {0x098C, 0xA351}, 	// MCU_ADDRESS [AWB_CCM_POSITION_MIN]
    {0x0990, 0x007f}, 	// MCU_DATA_0
    {0x098C, 0xA352}, 	// MCU_ADDRESS [AWB_CCM_POSITION_MAX]
    {0x0990, 0x007F}, 	// MCU_DATA_0
    {0x098C, 0xA103}, 	// MCU_ADDRESS [SEQ_CMD]
    {0x0990, 0x0005}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg wb_flourescant_tbl[] = { /*ok*/
    {0x098C, 0xA34A}, 	// MCU_ADDRESS [AWB_GAIN_MIN]
    {0x0990, 0x0085}, 	// MCU_DATA_0
    {0x098C, 0xA34B}, 	// MCU_ADDRESS [AWB_GAIN_MAX]
    {0x0990, 0x0085}, 	// MCU_DATA_0
    {0x098C, 0xA34C}, 	// MCU_ADDRESS [AWB_GAINMIN_B]
    {0x0990, 0x0097}, 	// MCU_DATA_0
    {0x098C, 0xA34D}, 	// MCU_ADDRESS [AWB_GAINMAX_B]
    {0x0990, 0x0097}, 	// MCU_DATA_0
    {0x098C, 0xA351}, 	// MCU_ADDRESS [AWB_CCM_POSITION_MIN]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xA352}, 	// MCU_ADDRESS [AWB_CCM_POSITION_MAX]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xA103}, 	// MCU_ADDRESS [SEQ_CMD]
    {0x0990, 0x0005}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg wb_incandescent_tbl[] = { /*ok*/
    {0x098C, 0xA34A}, 	// MCU_ADDRESS [AWB_GAIN_MIN]
    {0x0990, 0x006e}, 	// MCU_DATA_0
    {0x098C, 0xA34B}, 	// MCU_ADDRESS [AWB_GAIN_MAX]
    {0x0990, 0x006e}, 	// MCU_DATA_0
    {0x098C, 0xA34C}, 	// MCU_ADDRESS [AWB_GAINMIN_B]
    {0x0990, 0x00b3}, 	// MCU_DATA_0
    {0x098C, 0xA34D}, 	// MCU_ADDRESS [AWB_GAINMAX_B]
    {0x0990, 0x00b5}, 	// MCU_DATA_0
    {0x098C, 0xA351}, 	// MCU_ADDRESS [AWB_CCM_POSITION_MIN]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xA352}, 	// MCU_ADDRESS [AWB_CCM_POSITION_MAX]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xA103}, 	// MCU_ADDRESS [SEQ_CMD]
    {0x0990, 0x0005}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};


static struct sensor_reg wb_auto_tbl[] = { /*ok*/
    {0x098C, 0xA34A}, 	// MCU_ADDRESS [AWB_GAIN_MIN]
    {0x0990, 0x0059}, 	// MCU_DATA_0
    {0x098C, 0xA34B}, 	// MCU_ADDRESS [AWB_GAIN_MAX]
    {0x0990, 0x00B6}, 	// MCU_DATA_0
    {0x098C, 0xA34C}, 	// MCU_ADDRESS [AWB_GAINMIN_B]
    {0x0990, 0x0059}, 	// MCU_DATA_0
    {0x098C, 0xA34D}, 	// MCU_ADDRESS [AWB_GAINMAX_B]
    {0x0990, 0x00B5}, 	// MCU_DATA_0
    {0x098C, 0xA351}, 	// MCU_ADDRESS [AWB_CCM_POSITION_MIN]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xA352}, 	// MCU_ADDRESS [AWB_CCM_POSITION_MAX]
    {0x0990, 0x007F}, 	// MCU_DATA_0
//the bigger of the value,the smaller of the green    
    {0x098C, 0xA367}, 	// MCU_ADDRESS [AWB_CCM_POSITION_MAX]
    {0x0990, 0x0070}, 	// MCU_DATA_0
	
    {0x098C, 0xA103}, 	// MCU_ADDRESS [SEQ_CMD]
    {0x0990, 0x0005}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const contrast_tbl_0[] = {
    // G 0.3  B 5  C 1
    {0x098C, 0xAB3C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_0]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xAB3D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_1]
    {0x0990, 0x0018}, 	// MCU_DATA_0
    {0x098C, 0xAB3E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_2]
    {0x0990, 0x0041}, 	// MCU_DATA_0
    {0x098C, 0xAB3F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_3]
    {0x0990, 0x0064}, 	// MCU_DATA_0
    {0x098C, 0xAB40}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_4]
    {0x0990, 0x0083}, 	// MCU_DATA_0
    {0x098C, 0xAB41}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_5]
    {0x0990, 0x0096}, 	// MCU_DATA_0
    {0x098C, 0xAB42}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_6]
    {0x0990, 0x00A5}, 	// MCU_DATA_0
    {0x098C, 0xAB43}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_7]
    {0x0990, 0x00B1}, 	// MCU_DATA_0
    {0x098C, 0xAB44}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_8]
    {0x0990, 0x00BC}, 	// MCU_DATA_0
    {0x098C, 0xAB45}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_9]
    {0x0990, 0x00C5}, 	// MCU_DATA_0
    {0x098C, 0xAB46}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_10]
    {0x0990, 0x00CE}, 	// MCU_DATA_0
    {0x098C, 0xAB47}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_11]
    {0x0990, 0x00D6}, 	// MCU_DATA_0
    {0x098C, 0xAB48}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_12]
    {0x0990, 0x00DD}, 	// MCU_DATA_0
    {0x098C, 0xAB49}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_13]
    {0x0990, 0x00E3}, 	// MCU_DATA_0
    {0x098C, 0xAB4A}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_14]
    {0x0990, 0x00E9}, 	// MCU_DATA_0
    {0x098C, 0xAB4B}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_15]
    {0x0990, 0x00EF}, 	// MCU_DATA_0
    {0x098C, 0xAB4C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_16]
    {0x0990, 0x00F5}, 	// MCU_DATA_0
    {0x098C, 0xAB4D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_17]
    {0x0990, 0x00FA}, 	// MCU_DATA_0
    {0x098C, 0xAB4E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_18]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
    {0x098C, 0xAB4F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_0]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xAB50}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_1]
    {0x0990, 0x0018}, 	// MCU_DATA_0
    {0x098C, 0xAB51}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_2]
    {0x0990, 0x0041}, 	// MCU_DATA_0
    {0x098C, 0xAB52}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_3]
    {0x0990, 0x0064}, 	// MCU_DATA_0
    {0x098C, 0xAB53}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_4]
    {0x0990, 0x0083}, 	// MCU_DATA_0
    {0x098C, 0xAB54}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_5]
    {0x0990, 0x0096}, 	// MCU_DATA_0
    {0x098C, 0xAB55}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_6]
    {0x0990, 0x00A5}, 	// MCU_DATA_0
    {0x098C, 0xAB56}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_7]
    {0x0990, 0x00B1}, 	// MCU_DATA_0
    {0x098C, 0xAB57}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_8]
    {0x0990, 0x00BC}, 	// MCU_DATA_0
    {0x098C, 0xAB58}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_9]
    {0x0990, 0x00C5}, 	// MCU_DATA_0
    {0x098C, 0xAB59}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_10]
    {0x0990, 0x00CE}, 	// MCU_DATA_0
    {0x098C, 0xAB5A}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_11]
    {0x0990, 0x00D6}, 	// MCU_DATA_0
    {0x098C, 0xAB5B}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_12]
    {0x0990, 0x00DD}, 	// MCU_DATA_0
    {0x098C, 0xAB5C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_13]
    {0x0990, 0x00E3}, 	// MCU_DATA_0
    {0x098C, 0xAB5D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_14]
    {0x0990, 0x00E9}, 	// MCU_DATA_0
    {0x098C, 0xAB5E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_15]
    {0x0990, 0x00EF}, 	// MCU_DATA_0
    {0x098C, 0xAB5F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_16]
    {0x0990, 0x00F5}, 	// MCU_DATA_0
    {0x098C, 0xAB60}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_17]
    {0x0990, 0x00FA}, 	// MCU_DATA_0
    {0x098C, 0xAB61}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_18]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const contrast_tbl_1[] = {
    // G 0.35  B 5  C 1.25
    {0x098C, 0xAB3C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_0]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xAB3D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_1]
    {0x0990, 0x0010}, 	// MCU_DATA_0
    {0x098C, 0xAB3E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_2]
    {0x0990, 0x002E}, 	// MCU_DATA_0
    {0x098C, 0xAB3F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_3]
    {0x0990, 0x004F}, 	// MCU_DATA_0
    {0x098C, 0xAB40}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_4]
    {0x0990, 0x0072}, 	// MCU_DATA_0
    {0x098C, 0xAB41}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_5]
    {0x0990, 0x008C}, 	// MCU_DATA_0
    {0x098C, 0xAB42}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_6]
    {0x0990, 0x009F}, 	// MCU_DATA_0
    {0x098C, 0xAB43}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_7]
    {0x0990, 0x00AD}, 	// MCU_DATA_0
    {0x098C, 0xAB44}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_8]
    {0x0990, 0x00BA}, 	// MCU_DATA_0
    {0x098C, 0xAB45}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_9]
    {0x0990, 0x00C4}, 	// MCU_DATA_0
    {0x098C, 0xAB46}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_10]
    {0x0990, 0x00CE}, 	// MCU_DATA_0
    {0x098C, 0xAB47}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_11]
    {0x0990, 0x00D6}, 	// MCU_DATA_0
    {0x098C, 0xAB48}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_12]
    {0x0990, 0x00DD}, 	// MCU_DATA_0
    {0x098C, 0xAB49}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_13]
    {0x0990, 0x00E4}, 	// MCU_DATA_0
    {0x098C, 0xAB4A}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_14]
    {0x0990, 0x00EA}, 	// MCU_DATA_0
    {0x098C, 0xAB4B}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_15]
    {0x0990, 0x00F0}, 	// MCU_DATA_0
    {0x098C, 0xAB4C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_16]
    {0x0990, 0x00F5}, 	// MCU_DATA_0
    {0x098C, 0xAB4D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_17]
    {0x0990, 0x00FA}, 	// MCU_DATA_0
    {0x098C, 0xAB4E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_18]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
    {0x098C, 0xAB4F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_0]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xAB50}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_1]
    {0x0990, 0x0010}, 	// MCU_DATA_0
    {0x098C, 0xAB51}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_2]
    {0x0990, 0x002E}, 	// MCU_DATA_0
    {0x098C, 0xAB52}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_3]
    {0x0990, 0x004F}, 	// MCU_DATA_0
    {0x098C, 0xAB53}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_4]
    {0x0990, 0x0072}, 	// MCU_DATA_0
    {0x098C, 0xAB54}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_5]
    {0x0990, 0x008C}, 	// MCU_DATA_0
    {0x098C, 0xAB55}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_6]
    {0x0990, 0x009F}, 	// MCU_DATA_0
    {0x098C, 0xAB56}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_7]
    {0x0990, 0x00AD}, 	// MCU_DATA_0
    {0x098C, 0xAB57}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_8]
    {0x0990, 0x00BA}, 	// MCU_DATA_0
    {0x098C, 0xAB58}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_9]
    {0x0990, 0x00C4}, 	// MCU_DATA_0
    {0x098C, 0xAB59}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_10]
    {0x0990, 0x00CE}, 	// MCU_DATA_0
    {0x098C, 0xAB5A}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_11]
    {0x0990, 0x00D6}, 	// MCU_DATA_0
    {0x098C, 0xAB5B}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_12]
    {0x0990, 0x00DD}, 	// MCU_DATA_0
    {0x098C, 0xAB5C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_13]
    {0x0990, 0x00E4}, 	// MCU_DATA_0
    {0x098C, 0xAB5D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_14]
    {0x0990, 0x00EA}, 	// MCU_DATA_0
    {0x098C, 0xAB5E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_15]
    {0x0990, 0x00F0}, 	// MCU_DATA_0
    {0x098C, 0xAB5F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_16]
    {0x0990, 0x00F5}, 	// MCU_DATA_0
    {0x098C, 0xAB60}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_17]
    {0x0990, 0x00FA}, 	// MCU_DATA_0
    {0x098C, 0xAB61}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_18]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};
static struct sensor_reg const contrast_tbl_2[] = {
    // Gamma 0.40  Black 5  C 1.35
    {0x098C, 0xAB4F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_0]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xAB50}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_1]
    {0x0990, 0x000C}, 	// MCU_DATA_0
    {0x098C, 0xAB51}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_2]
    {0x0990, 0x0022}, 	// MCU_DATA_0
    {0x098C, 0xAB52}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_3]
    {0x0990, 0x003F}, 	// MCU_DATA_0
    {0x098C, 0xAB53}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_4]
    {0x0990, 0x0062}, 	// MCU_DATA_0
    {0x098C, 0xAB54}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_5]
    {0x0990, 0x007D}, 	// MCU_DATA_0
    {0x098C, 0xAB55}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_6]
    {0x0990, 0x0093}, 	// MCU_DATA_0
    {0x098C, 0xAB56}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_7]
    {0x0990, 0x00A5}, 	// MCU_DATA_0
    {0x098C, 0xAB57}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_8]
    {0x0990, 0x00B3}, 	// MCU_DATA_0
    {0x098C, 0xAB58}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_9]
    {0x0990, 0x00BF}, 	// MCU_DATA_0
    {0x098C, 0xAB59}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_10]
    {0x0990, 0x00C9}, 	// MCU_DATA_0
    {0x098C, 0xAB5A}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_11]
    {0x0990, 0x00D3}, 	// MCU_DATA_0
    {0x098C, 0xAB5B}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_12]
    {0x0990, 0x00DB}, 	// MCU_DATA_0
    {0x098C, 0xAB5C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_13]
    {0x0990, 0x00E2}, 	// MCU_DATA_0
    {0x098C, 0xAB5D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_14]
    {0x0990, 0x00E9}, 	// MCU_DATA_0
    {0x098C, 0xAB5E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_15]
    {0x0990, 0x00EF}, 	// MCU_DATA_0
    {0x098C, 0xAB5F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_16]
    {0x0990, 0x00F5}, 	// MCU_DATA_0
    {0x098C, 0xAB60}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_17]
    {0x0990, 0x00FA}, 	// MCU_DATA_0
    {0x098C, 0xAB61}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_18]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
    {0x098C, 0xAB3C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_0]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xAB3D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_1]
    {0x0990, 0x000C}, 	// MCU_DATA_0
    {0x098C, 0xAB3E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_2]
    {0x0990, 0x0022}, 	// MCU_DATA_0
    {0x098C, 0xAB3F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_3]
    {0x0990, 0x003F}, 	// MCU_DATA_0
    {0x098C, 0xAB40}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_4]
    {0x0990, 0x0062}, 	// MCU_DATA_0
    {0x098C, 0xAB41}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_5]
    {0x0990, 0x007D}, 	// MCU_DATA_0
    {0x098C, 0xAB42}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_6]
    {0x0990, 0x0093}, 	// MCU_DATA_0
    {0x098C, 0xAB43}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_7]
    {0x0990, 0x00A5}, 	// MCU_DATA_0
    {0x098C, 0xAB44}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_8]
    {0x0990, 0x00B3}, 	// MCU_DATA_0
    {0x098C, 0xAB45}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_9]
    {0x0990, 0x00BF}, 	// MCU_DATA_0
    {0x098C, 0xAB46}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_10]
    {0x0990, 0x00C9}, 	// MCU_DATA_0
    {0x098C, 0xAB47}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_11]
    {0x0990, 0x00D3}, 	// MCU_DATA_0
    {0x098C, 0xAB48}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_12]
    {0x0990, 0x00DB}, 	// MCU_DATA_0
    {0x098C, 0xAB49}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_13]
    {0x0990, 0x00E2}, 	// MCU_DATA_0
    {0x098C, 0xAB4A}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_14]
    {0x0990, 0x00E9}, 	// MCU_DATA_0
    {0x098C, 0xAB4B}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_15]
    {0x0990, 0x00EF}, 	// MCU_DATA_0
    {0x098C, 0xAB4C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_16]
    {0x0990, 0x00F5}, 	// MCU_DATA_0
    {0x098C, 0xAB4D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_17]
    {0x0990, 0x00FA}, 	// MCU_DATA_0
    {0x098C, 0xAB4E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_18]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const contrast_tbl_3[] = {
    // G 0.5  B 7  C 1.3
    {0x098C, 0xAB3C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_0]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xAB3D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_1]
    {0x0990, 0x0005}, 	// MCU_DATA_0
    {0x098C, 0xAB3E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_2]
    {0x0990, 0x0010}, 	// MCU_DATA_0
    {0x098C, 0xAB3F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_3]
    {0x0990, 0x0029}, 	// MCU_DATA_0
    {0x098C, 0xAB40}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_4]
    {0x0990, 0x0049}, 	// MCU_DATA_0
    {0x098C, 0xAB41}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_5]
    {0x0990, 0x0062}, 	// MCU_DATA_0
    {0x098C, 0xAB42}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_6]
    {0x0990, 0x0078}, 	// MCU_DATA_0
    {0x098C, 0xAB43}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_7]
    {0x0990, 0x008D}, 	// MCU_DATA_0
    {0x098C, 0xAB44}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_8]
    {0x0990, 0x009E}, 	// MCU_DATA_0
    {0x098C, 0xAB45}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_9]
    {0x0990, 0x00AD}, 	// MCU_DATA_0
    {0x098C, 0xAB46}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_10]
    {0x0990, 0x00BA}, 	// MCU_DATA_0
    {0x098C, 0xAB47}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_11]
    {0x0990, 0x00C6}, 	// MCU_DATA_0
    {0x098C, 0xAB48}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_12]
    {0x0990, 0x00D0}, 	// MCU_DATA_0
    {0x098C, 0xAB49}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_13]
    {0x0990, 0x00DA}, 	// MCU_DATA_0
    {0x098C, 0xAB4A}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_14]
    {0x0990, 0x00E2}, 	// MCU_DATA_0
    {0x098C, 0xAB4B}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_15]
    {0x0990, 0x00EA}, 	// MCU_DATA_0
    {0x098C, 0xAB4C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_16]
    {0x0990, 0x00F2}, 	// MCU_DATA_0
    {0x098C, 0xAB4D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_17]
    {0x0990, 0x00F9}, 	// MCU_DATA_0
    {0x098C, 0xAB4E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_18]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
    {0x098C, 0xAB4F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_0]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xAB50}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_1]
    {0x0990, 0x0005}, 	// MCU_DATA_0
    {0x098C, 0xAB51}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_2]
    {0x0990, 0x0010}, 	// MCU_DATA_0
    {0x098C, 0xAB52}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_3]
    {0x0990, 0x0029}, 	// MCU_DATA_0
    {0x098C, 0xAB53}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_4]
    {0x0990, 0x0049}, 	// MCU_DATA_0
    {0x098C, 0xAB54}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_5]
    {0x0990, 0x0062}, 	// MCU_DATA_0
    {0x098C, 0xAB55}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_6]
    {0x0990, 0x0078}, 	// MCU_DATA_0
    {0x098C, 0xAB56}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_7]
    {0x0990, 0x008D}, 	// MCU_DATA_0
    {0x098C, 0xAB57}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_8]
    {0x0990, 0x009E}, 	// MCU_DATA_0
    {0x098C, 0xAB58}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_9]
    {0x0990, 0x00AD}, 	// MCU_DATA_0
    {0x098C, 0xAB59}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_10]
    {0x0990, 0x00BA}, 	// MCU_DATA_0
    {0x098C, 0xAB5A}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_11]
    {0x0990, 0x00C6}, 	// MCU_DATA_0
    {0x098C, 0xAB5B}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_12]
    {0x0990, 0x00D0}, 	// MCU_DATA_0
    {0x098C, 0xAB5C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_13]
    {0x0990, 0x00DA}, 	// MCU_DATA_0
    {0x098C, 0xAB5D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_14]
    {0x0990, 0x00E2}, 	// MCU_DATA_0
    {0x098C, 0xAB5E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_15]
    {0x0990, 0x00EA}, 	// MCU_DATA_0
    {0x098C, 0xAB5F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_16]
    {0x0990, 0x00F2}, 	// MCU_DATA_0
    {0x098C, 0xAB60}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_17]
    {0x0990, 0x00F9}, 	// MCU_DATA_0
    {0x098C, 0xAB61}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_18]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}	
};

static struct sensor_reg const contrast_tbl_4[] = {
    {0x098C, 0xAB3C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_0]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xAB3D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_1]
    {0x0990, 0x0003}, 	// MCU_DATA_0
    {0x098C, 0xAB3E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_2]
    {0x0990, 0x000A}, 	// MCU_DATA_0
    {0x098C, 0xAB3F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_3]
    {0x0990, 0x001C}, 	// MCU_DATA_0
    {0x098C, 0xAB40}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_4]
    {0x0990, 0x0036}, 	// MCU_DATA_0
    {0x098C, 0xAB41}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_5]
    {0x0990, 0x004D}, 	// MCU_DATA_0
    {0x098C, 0xAB42}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_6]
    {0x0990, 0x0063}, 	// MCU_DATA_0
    {0x098C, 0xAB43}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_7]
    {0x0990, 0x0078}, 	// MCU_DATA_0
    {0x098C, 0xAB44}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_8]
    {0x0990, 0x008D}, 	// MCU_DATA_0
    {0x098C, 0xAB45}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_9]
    {0x0990, 0x009E}, 	// MCU_DATA_0
    {0x098C, 0xAB46}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_10]
    {0x0990, 0x00AE}, 	// MCU_DATA_0
    {0x098C, 0xAB47}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_11]
    {0x0990, 0x00BC}, 	// MCU_DATA_0
    {0x098C, 0xAB48}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_12]
    {0x0990, 0x00C8}, 	// MCU_DATA_0
    {0x098C, 0xAB49}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_13]
    {0x0990, 0x00D3}, 	// MCU_DATA_0
    {0x098C, 0xAB4A}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_14]
    {0x0990, 0x00DD}, 	// MCU_DATA_0
    {0x098C, 0xAB4B}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_15]
    {0x0990, 0x00E7}, 	// MCU_DATA_0
    {0x098C, 0xAB4C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_16]
    {0x0990, 0x00EF}, 	// MCU_DATA_0
    {0x098C, 0xAB4D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_17]
    {0x0990, 0x00F7}, 	// MCU_DATA_0
    {0x098C, 0xAB4E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_A_18]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
    {0x098C, 0xAB4F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_0]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xAB50}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_1]
    {0x0990, 0x0003}, 	// MCU_DATA_0
    {0x098C, 0xAB51}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_2]
    {0x0990, 0x000A}, 	// MCU_DATA_0
    {0x098C, 0xAB52}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_3]
    {0x0990, 0x001C}, 	// MCU_DATA_0
    {0x098C, 0xAB53}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_4]
    {0x0990, 0x0036}, 	// MCU_DATA_0
    {0x098C, 0xAB54}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_5]
    {0x0990, 0x004D}, 	// MCU_DATA_0
    {0x098C, 0xAB55}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_6]
    {0x0990, 0x0063}, 	// MCU_DATA_0
    {0x098C, 0xAB56}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_7]
    {0x0990, 0x0078}, 	// MCU_DATA_0
    {0x098C, 0xAB57}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_8]
    {0x0990, 0x008D}, 	// MCU_DATA_0
    {0x098C, 0xAB58}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_9]
    {0x0990, 0x009E}, 	// MCU_DATA_0
    {0x098C, 0xAB59}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_10]
    {0x0990, 0x00AE}, 	// MCU_DATA_0
    {0x098C, 0xAB5A}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_11]
    {0x0990, 0x00BC}, 	// MCU_DATA_0
    {0x098C, 0xAB5B}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_12]
    {0x0990, 0x00C8}, 	// MCU_DATA_0
    {0x098C, 0xAB5C}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_13]
    {0x0990, 0x00D3}, 	// MCU_DATA_0
    {0x098C, 0xAB5D}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_14]
    {0x0990, 0x00DD}, 	// MCU_DATA_0
    {0x098C, 0xAB5E}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_15]
    {0x0990, 0x00E7}, 	// MCU_DATA_0
    {0x098C, 0xAB5F}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_16]
    {0x0990, 0x00EF}, 	// MCU_DATA_0
    {0x098C, 0xAB60}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_17]
    {0x0990, 0x00F7}, 	// MCU_DATA_0
    {0x098C, 0xAB61}, 	// MCU_ADDRESS [HG_GAMMA_TABLE_B_18]
    {0x0990, 0x00FF}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}	
};


/*
 * The settings of exposure and brightness could be influenced by each other.
 * So we should modify the configurations when the setting of exposure or 
 * brightness is changed.
 * We can find the right configuration in the following tables if we know the
 * values of exposure and brightness.
 */
//[exposure_tbl_0]
static struct sensor_reg const exposure0_brightness0_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0006},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure0_brightness1_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0010},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure0_brightness2_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x001A},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure0_brightness3_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0023},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure0_brightness4_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x002B},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure0_brightness5_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0038},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure0_brightness6_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0048},
	{SENSOR_TABLE_END, 0x0000}
};

//[exposure_tbl_1]
static struct sensor_reg const exposure1_brightness0_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x000A},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure1_brightness1_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0019},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure1_brightness2_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0028},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure1_brightness3_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0034},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure1_brightness4_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0040},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure1_brightness5_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0054},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure1_brightness6_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x006C},
	{SENSOR_TABLE_END, 0x0000}
};

//[exposure_tbl_2]
static struct sensor_reg const exposure2_brightness0_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0010},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure2_brightness1_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0022},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure2_brightness2_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0036},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure2_brightness3_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0046},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure2_brightness4_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0056},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure2_brightness5_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0070},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure2_brightness6_tbl[] = {
    {0x337E, 0x0000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0090},
	{SENSOR_TABLE_END, 0x0000}
};

//[exposure_tbl_3]
static struct sensor_reg const exposure3_brightness0_tbl[] = {
    {0x337E, 0x1800}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0010},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure3_brightness1_tbl[] = {
    {0x337E, 0x1800}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0022},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure3_brightness2_tbl[] = {
    {0x337E, 0x1800}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0036},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure3_brightness3_tbl[] = {
    {0x337E, 0x1800}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0046},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure3_brightness4_tbl[] = {
    {0x337E, 0x1800}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0056},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure3_brightness5_tbl[] = {
    {0x337E, 0x1800}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0070},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure3_brightness6_tbl[] = {
    {0x337E, 0x1800}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0090},
	{SENSOR_TABLE_END, 0x0000}
};

//[exposure_tbl_4]
static struct sensor_reg const exposure4_brightness0_tbl[] = {
    {0x337E, 0x3000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0010},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure4_brightness1_tbl[] = {
    {0x337E, 0x3000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0022},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure4_brightness2_tbl[] = {
    {0x337E, 0x3000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0036},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure4_brightness3_tbl[] = {
    {0x337E, 0x3000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0046},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure4_brightness4_tbl[] = {
    {0x337E, 0x3000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0056},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure4_brightness5_tbl[] = {
    {0x337E, 0x3000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0070},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const exposure4_brightness6_tbl[] = {
    {0x337E, 0x3000}, 
    {0x098C, 0xA24F},
    {0x0990, 0x0090},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const *brightness_exposure_tbl[][7] = {
    {
		exposure0_brightness0_tbl,
		exposure0_brightness1_tbl,
		exposure0_brightness2_tbl,
		exposure0_brightness3_tbl,
		exposure0_brightness4_tbl,
		exposure0_brightness5_tbl,
		exposure0_brightness6_tbl
	},
	{
		exposure1_brightness0_tbl,
		exposure1_brightness1_tbl,
		exposure1_brightness2_tbl,
		exposure1_brightness3_tbl,
		exposure1_brightness4_tbl,
		exposure1_brightness5_tbl,
		exposure1_brightness6_tbl
	},
	{
		exposure2_brightness0_tbl,
		exposure2_brightness1_tbl,
		exposure2_brightness2_tbl,
		exposure2_brightness3_tbl,
		exposure2_brightness4_tbl,
		exposure2_brightness5_tbl,
		exposure2_brightness6_tbl
	},
	{
		exposure3_brightness0_tbl,
		exposure3_brightness1_tbl,
		exposure3_brightness2_tbl,
		exposure3_brightness3_tbl,
		exposure3_brightness4_tbl,
		exposure3_brightness5_tbl,
		exposure3_brightness6_tbl
	},
	{
		exposure4_brightness0_tbl,
		exposure4_brightness1_tbl,
		exposure4_brightness2_tbl,
		exposure4_brightness3_tbl,
		exposure4_brightness4_tbl,
		exposure4_brightness5_tbl,
		exposure4_brightness6_tbl
	}
};

static struct sensor_reg const saturation_tbl_0[] = {
    {0x098C, 0xAB20}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0020}, 	// MCU_DATA_0
    {0x098C, 0xAB24}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xA103}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0005}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}	
};

static struct sensor_reg const saturation_tbl_1[] = {
    {0x098C, 0xAB20}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0048}, 	// MCU_DATA_0
    {0x098C, 0xAB24}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xA103}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0005}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}	
};

static struct sensor_reg const saturation_tbl_2[] = {
    {0x098C, 0xAB20}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0078}, 	// MCU_DATA_0   <- higher value, more saturation, comment 0x78 or 0x70 
    {0x098C, 0xAB24}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0000}, 	// MCU_DATA_0
    {0x098C, 0xA103}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0005}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}		
};

static struct sensor_reg const saturation_tbl_3[] = {
    {0x098C, 0xAB20}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0076}, 	// MCU_DATA_0
    {0x098C, 0xAB24}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0020}, 	// MCU_DATA_0
    {0x098C, 0xA103}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0005}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}		
};

static struct sensor_reg const saturation_tbl_4[] = {
    {0x098C, 0xAB20}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x007F}, 	// MCU_DATA_0
    {0x098C, 0xAB24}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0040}, 	// MCU_DATA_0
    {0x098C, 0xA103}, 	// MCU_ADDRESS [HG_LL_SAT1]
    {0x0990, 0x0005}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}		
};


static struct sensor_reg const sharpness_tbl_0[] = {
    {0x098C, 0xAB22}, 	// MCU_ADDRESS [HG_LL_APCORR1]
    {0x0990, 0x0000}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}		
};

static struct sensor_reg const sharpness_tbl_1[] = {
    {0x098C, 0xAB22}, 	// MCU_ADDRESS [HG_LL_APCORR1]
    {0x0990, 0x0002}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}		
};

static struct sensor_reg const sharpness_tbl_2[] = {    
    {0x098C, 0xAB22}, 	// MCU_ADDRESS [HG_LL_APCORR1]
    {0x0990, 0x0003}, 	// MCU_DATA_0 //0x07
	{SENSOR_TABLE_END, 0x0000}		
};

static struct sensor_reg const sharpness_tbl_3[] = {
    {0x098C, 0xAB22}, 	// MCU_ADDRESS [HG_LL_APCORR1]
    {0x0990, 0x0004}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}		
};

static struct sensor_reg const sharpness_tbl_4[] = {
    {0x098C, 0xAB22}, 	// MCU_ADDRESS [HG_LL_APCORR1]
    {0x0990, 0x0007}, 	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}		
};


static struct sensor_reg const iso_tbl_auto[] = {
	{0x098C, 0xA20D},
	{0x0990, 0x0020},
	{0x098C, 0xA20E},
	{0x0990, 0x0090},
	{0x098C, 0xA103},
	{0x0990, 0x0006},
	{SENSOR_WAIT_MS, 200},	// delay=200
	{0x098C, 0xA20D},
	{0x0990, 0x0020},
	{0x098C, 0xA20E},
	{0x0990, 0x0090},
	{0x098C, 0xA103},
	{0x0990, 0x0005},
	{SENSOR_TABLE_END, 0x0000}		
};

static struct sensor_reg const iso_tbl_100[] = {
	{0x098C, 0xA20D},
	{0x0990, 0x0020},
	{0x098C, 0xA20E},
	{0x0990, 0x0028},
	{0x098C, 0xA103},
	{0x0990, 0x0005},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const iso_tbl_200[] = {
	{0x098C, 0xA20D},
	{0x0990, 0x0040},
	{0x098C, 0xA20E},
	{0x0990, 0x0048},
	{0x098C, 0xA103},
	{0x0990, 0x0005},
	{SENSOR_TABLE_END, 0x0000}		
};

static struct sensor_reg const iso_tbl_400[] = {
	{0x098C, 0xA20D},
	{0x0990, 0x0050},
	{0x098C, 0xA20E},
	{0x0990, 0x0080},
	{0x098C, 0xA103},
	{0x0990, 0x0005},
	{SENSOR_TABLE_END, 0x0000}		
};

static struct sensor_reg const banding_tbl_60[] = {
	{0x098C, 0xA118},
	{0x0990, 0x0002},
	{0x098C, 0xA11E},
	{0x0990, 0x0002},
	{0x098C, 0xA124},
	{0x0990, 0x0002},
	{0x098C, 0xA12A},
	{0x0990, 0x0002},
	{0x098C, 0xA404},
	{0x0990, 0x00A0},
	{0x098C, 0xA103},
	{0x0990, 0x0005},
	{SENSOR_TABLE_END, 0x0000}		
};

static struct sensor_reg const banding_tbl_50[] = {
	{0x098C, 0xA118},
	{0x0990, 0x0002},
	{0x098C, 0xA11E},
	{0x0990, 0x0002},
	{0x098C, 0xA124},
	{0x0990, 0x0002},
	{0x098C, 0xA12A},
	{0x0990, 0x0002},
	{0x098C, 0xA404},
	{0x0990, 0x00E0},
	{0x098C, 0xA103},
	{0x0990, 0x0005},
	{SENSOR_TABLE_END, 0x0000}		
};

static struct sensor_reg const banding_tbl_auto[] = {
	{0x098C, 0xA118},
	{0x0990, 0x0001},
	{0x098C, 0xA11E},
	{0x0990, 0x0001},
	{0x098C, 0xA124},
	{0x0990, 0x0000},
	{0x098C, 0xA12A},
	{0x0990, 0x0001},
	{0x098C, 0xA103},
	{0x0990, 0x0005},
	{SENSOR_TABLE_END, 0x0000}		
};



static struct sensor_reg const coloreffect_tbl_none[] = {
	{0x098C, 0x2759},	// content A
	{0x0990, 0x6440},
	{0x098C, 0x275B},	// content B
	{0x0990, 0x6440},
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005},	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const coloreffect_tbl_mono[] = { /*ok*/
	{0x098C, 0x2759},	// content A
	{0x0990, 0x6441},
	{0x098C, 0x275B},	// content B
	{0x0990, 0x6441},
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005},	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const coloreffect_tbl_sepia[] = { /*ok*/
	{0x098C, 0x2759},	// content A
	{0x0990, 0x6442},
	{0x098C, 0x275B},	// content B
	{0x0990, 0x6442},
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005},	// MCU_DATA_0
	
	{0x098E, 0xE886},
	{0x0990, 0x00D8},
	{0x098E, 0xEC85},
	{0x0990, 0x001F},
	{0x098E, 0xEC86},
	{0x0990, 0x00D8},
	{0x098E, 0x8400},
	{0x0990, 0x0006},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const coloreffect_tbl_negative[] = { /*ok*/
	{0x098C, 0x2759},	// content A
	{0x0990, 0x6443},
	{0x098C, 0x275B},	// content B
	{0x0990, 0x6443},
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005},	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const coloreffect_tbl_solarize[] = { /*ok*/
	{0x098C, 0x2759},	// content A
	{0x0990, 0x6444},
	{0x098C, 0x275B},	// content B
	{0x0990, 0x6444},
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005},	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const mode_tbl_preview[] = { 
	{0x098C, 0xA115},
	{0x0990, 0x0000},
	{0x098C, 0xA103},
	{0x0990, 0x0001},
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg const mode_tbl_snapshot[] = { 
	{0x098C, 0xA115},
	{0x0990, 0x0002},
	{0x098C, 0xA103},
	{0x0990, 0x0002},
	{SENSOR_TABLE_END, 0x0000}
};


static struct mutex yuv_lock;

static int sensor_read_reg(struct i2c_client *client, u16 addr, u16 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[4];

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8) (addr >> 8);;
	data[1] = (u8) (addr & 0xff);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = data + 2;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	swap(*(data+2),*(data+3)); // swap high and low byte to match table format
	memcpy(val, data+2, 2);

	return 0;
}

static int sensor_write_reg(struct i2c_client *client, u16 addr, u16 val)
{
	int err;
	struct i2c_msg msg;
	unsigned char data[4];
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val >> 8);
	data[3] = (u8) (val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 4;
	msg.buf = data;

	do {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			return 0;
		retry++;
		dev_err(&client->dev,"i2c transfer failed, retrying %x %x\n", addr, val);
		msleep(3);
	} while (retry <= SENSOR_MAX_RETRIES);

	return err;
}

static int sensor_write_table(struct i2c_client *client, const struct sensor_reg table[])
{
	int err;
	const struct sensor_reg *next;
	u16 val;

	dev_dbg(&client->dev,"%s\n", __func__);
	for (next = table; next->addr != SENSOR_TABLE_END; next++) {
		if (next->addr == SENSOR_WAIT_MS) {
			msleep(next->val);
			continue;
		}

		val = next->val;

		err = sensor_write_reg(client, next->addr, val);
		if (err)
			return err;
	}

	return 0;
}


struct sensor_info {
	struct i2c_client *i2c_client;
	struct mt9d115_sensor_platform_data *pdata;
	int8_t current_brightness;
	int8_t current_exposure;
};

/*
 * Adjust the configurations according to the value of expusure & brightness
 */
static int sensor_set_exposure_brightness(struct sensor_info *info, int8_t exposure, int8_t brightness)
{
    int err = 0;

    dev_dbg(&info->i2c_client->dev,"exposure=%d, brightness=%d", exposure, brightness);

	err = sensor_write_table(info->i2c_client, brightness_exposure_tbl[exposure][brightness]); 

    return err;
}

/*
 * White Balance Setting
 */
static int sensor_set_wb(struct sensor_info *info, int8_t whitebalance)
{
    int err = 0;

	dev_dbg(&info->i2c_client->dev,"whitebalance %d\n", whitebalance);

	switch(whitebalance) {
		case CAMERA_WB_MODE_AWB:
			err = sensor_write_table(info->i2c_client, wb_auto_tbl);
			break;

		case CAMERA_WB_MODE_INCANDESCENT:
			err = sensor_write_table(info->i2c_client, wb_incandescent_tbl);
			break;

		case CAMERA_WB_MODE_SUNLIGHT:
			err = sensor_write_table(info->i2c_client, wb_daylight_tbl);
			break;

		case CAMERA_WB_MODE_FLUORESCENT:
			err = sensor_write_table(info->i2c_client, wb_flourescant_tbl);
			break;

		case CAMERA_WB_MODE_CLOUDY:
			err = sensor_write_table(info->i2c_client, wb_cloudy_tbl);
			break;

		default:
			break;
	}
	if (err)
		return err;
		
	 /*
	  * Attention
	  *
	  * Time delay of 100ms or more is required by sensor,
	  *
	  * WB config will have no effect after setting 
	  * without time delay of 100ms or more
	  */
	mdelay(100);
		
	return 0;
}


static int sensor_set_contrast(struct sensor_info *info, int8_t contrast)
{
    int err = 0;

    dev_dbg(&info->i2c_client->dev,"contrast=%d\n", contrast);

    switch (contrast)
    {
        case CAMERA_CONTRAST_0:
        {
            err = sensor_write_table(info->i2c_client, contrast_tbl_0);
        }
        break;

        case CAMERA_CONTRAST_1:
        {
            err = sensor_write_table(info->i2c_client, contrast_tbl_1);
        }
        break;

        case CAMERA_CONTRAST_2:
        {
            err = sensor_write_table(info->i2c_client, contrast_tbl_2);
        }
        break;
        
        case CAMERA_CONTRAST_3:
        {
            err = sensor_write_table(info->i2c_client, contrast_tbl_3);
        }
        break; 

        case CAMERA_CONTRAST_4:
        {
            err = sensor_write_table(info->i2c_client, contrast_tbl_4);
        }
        break;

        default:
        {
            dev_err(&info->i2c_client->dev,"parameter error!");
            err = -EFAULT;
        }     
    }

    /*
      * Attention
      *
      * Time delay of 100ms or more is required by sensor,
      *
      * Contrast config will have no effect after setting 
      * without time delay of 100ms or more
      */
    mdelay(100);

	return err;
}

static int sensor_set_brightness(struct sensor_info *info, int8_t brightness)
{
    int err = 0;

    dev_dbg(&info->i2c_client->dev,"brightness=%d\n", brightness);

    info->current_brightness = brightness;

    err = sensor_set_exposure_brightness(info, info->current_exposure, info->current_brightness);

	return err;
}    


static int sensor_set_saturation(struct sensor_info *info, int8_t saturation)
{
    int err = 0;

    dev_dbg(&info->i2c_client->dev,"saturation=%d\n", saturation);

    switch (saturation)
    {
        case CAMERA_SATURATION_0:
        {
            err = sensor_write_table(info->i2c_client, saturation_tbl_0);
        }
        break;

        case CAMERA_SATURATION_1:
        {
            err = sensor_write_table(info->i2c_client, saturation_tbl_1);
        }
        break;

        case CAMERA_SATURATION_2:
        {
            err = sensor_write_table(info->i2c_client, saturation_tbl_2);
        }
        break;
        
        case CAMERA_SATURATION_3:
        {
            err = sensor_write_table(info->i2c_client, saturation_tbl_3);
        }
        break; 

        case CAMERA_SATURATION_4:
        {
            err = sensor_write_table(info->i2c_client, saturation_tbl_4);
        }
        break;

        default:
        {
            dev_err(&info->i2c_client->dev,"parameter error!");
            err = -EFAULT;
        }     
    }

    /*
      * Attention
      *
      * Time delay of 100ms or more is required by sensor,
      *
      * Saturation config will have no effect after setting 
      * without time delay of 100ms or more
      */
    mdelay(100);

	return err;
}    


static int sensor_set_sharpness(struct sensor_info *info, int8_t sharpness)
{
    int err = 0;

    dev_dbg(&info->i2c_client->dev,"sharpness=%d\n", sharpness);

    switch (sharpness)
    {
        case CAMERA_SHARPNESS_0:
        {
            err = sensor_write_table(info->i2c_client, sharpness_tbl_0);
        }
        break;

        case CAMERA_SHARPNESS_1:
        {
            err = sensor_write_table(info->i2c_client, sharpness_tbl_1);
        }
        break;

        case CAMERA_SHARPNESS_2:
        {
            err = sensor_write_table(info->i2c_client, sharpness_tbl_2);
        }
        break;
        
        case CAMERA_SHARPNESS_3:
        {
            err = sensor_write_table(info->i2c_client, sharpness_tbl_3);
        }
        break; 

        case CAMERA_SHARPNESS_4:
        {
            err = sensor_write_table(info->i2c_client, sharpness_tbl_4);
        }
        break;        
        
        default:
        {
            dev_err(&info->i2c_client->dev,"parameter error!");
            err = -EFAULT;
        }     
    }

	return err;
}    



/*
 * ISO Setting
 */
static int sensor_set_iso(struct sensor_info *info, int8_t iso_val)
{
    int err = 0;

    dev_dbg(&info->i2c_client->dev,"iso_val=%d\n", iso_val);

    switch (iso_val)
    {
        case CAMERA_ISO_SET_AUTO:
        {
            err = sensor_write_table(info->i2c_client, iso_tbl_auto);
        }
        break;


        case CAMERA_ISO_SET_100:
        {
            err = sensor_write_table(info->i2c_client, iso_tbl_100);
        }
        break;

        case CAMERA_ISO_SET_200:
        {
            err = sensor_write_table(info->i2c_client, iso_tbl_200);
        }
        break;

        case CAMERA_ISO_SET_400:
        {
            err = sensor_write_table(info->i2c_client, iso_tbl_400);
        }
        break;


        default:
        {
            dev_err(&info->i2c_client->dev,"parameter error!");
            err = -EFAULT;
        }     
    }

    /*
      * Attention
      *
      * Time delay of 100ms or more is required by sensor,
      *
      * ISO config will have no effect after setting 
      * without time delay of 100ms or more
      */
    mdelay(100);

	return err;
} 

/*
 * Antibanding Setting
 */
static int sensor_set_antibanding(struct sensor_info *info, int8_t antibanding)
{
    int err = 0;

    dev_dbg(&info->i2c_client->dev,"antibanding=%d", antibanding);

    switch (antibanding)
    {
        case CAMERA_ANTIBANDING_SET_OFF:
        {
            dev_err(&info->i2c_client->dev,"CAMERA_ANTIBANDING_SET_OFF NOT supported!");
        }
        break;

        case CAMERA_ANTIBANDING_SET_60HZ:
        {
            err = sensor_write_table(info->i2c_client, banding_tbl_60);
        }
        break;
		
        case CAMERA_ANTIBANDING_SET_50HZ:
        {
            err = sensor_write_table(info->i2c_client, banding_tbl_50);
        }
        break;

        case CAMERA_ANTIBANDING_SET_AUTO:
        {
            err = sensor_write_table(info->i2c_client, banding_tbl_auto);
        }
        break;

        default:
        {
            dev_err(&info->i2c_client->dev,"parameter error!");
            err = -EFAULT;
        }     
    }

    /*
      * Attention
      *
      * Time delay of 100ms or more is required by sensor,
      *
      * Antibanding config will have no effect after setting 
      * without time delay of 100ms or more
      */
    mdelay(100);

	return err;
} 

static int sensor_set_exposure_compensation(struct sensor_info *info, int8_t exposure)
{
    int err = 0;

    dev_dbg(&info->i2c_client->dev,"exposure=%d", exposure);

    info->current_exposure = exposure;

    err = sensor_set_exposure_brightness(info, info->current_exposure, info->current_brightness);

    return err;
}

static int sensor_set_coloreffect(struct sensor_info *info, int8_t coloreffect)
{
	int err = 0;
	dev_dbg(&info->i2c_client->dev,"coloreffect %d\n", coloreffect);

	switch(coloreffect) {
		case CAMERA_EFFECT_OFF:
			err = sensor_write_table(info->i2c_client, coloreffect_tbl_none);
			break;

		case CAMERA_EFFECT_MONO:
			err = sensor_write_table(info->i2c_client, coloreffect_tbl_mono);
			break;

		case CAMERA_EFFECT_SEPIA:
			err = sensor_write_table(info->i2c_client, coloreffect_tbl_sepia);
			break;

		case CAMERA_EFFECT_NEGATIVE:
			err = sensor_write_table(info->i2c_client, coloreffect_tbl_negative);
			break;

		case CAMERA_EFFECT_SOLARIZE:
			err = sensor_write_table(info->i2c_client, coloreffect_tbl_solarize);
			break;

		case CAMERA_EFFECT_POSTERIZE:
			err = -EINVAL;
			break;

		default:
			break;
	}
	if (err)
		return err;
		
	/*
	 * Attention
	 *
	 * Time delay of 100ms or more is required by sensor,
	 *
	 * Effect config will have no effect after setting 
	 * without time delay of 100ms or more
	 */
	mdelay(100);
	return 0;
}

static int sensor_set_sensor_mode(struct sensor_info *info, int mode)
{
    int err = 0;

    dev_dbg(&info->i2c_client->dev,"mode: %d", mode);

    switch (mode)
    {
        case SENSOR_PREVIEW_MODE:
        {
			err = sensor_write_table(info->i2c_client, mode_tbl_preview);

            /*
               * To improve efficiency of switch between preview and snapshot mode,
               * decrease time delay from 200ms to 80ms
               *
               * Attention:
               * The process of time delay should be set after process of setting
               * manual mode for autofocus in order to avoid display exception during
               * sensor initialization.
               */
            mdelay(80);
        }
        break;

        case SENSOR_SNAPSHOT_MODE:
        {
			err = sensor_write_table(info->i2c_client, mode_tbl_snapshot);

            /*
               * To improve efficiency of switch between preview and snapshot mode,
               * decrease time delay from 10ms to 0ms
               */
            //msleep(10);
        }
        break;

        default:
        {
            return -EFAULT;
        }
    }

    return 0;
}

static int sensor_get_exposure_time(struct sensor_info *info,unsigned int *exposure_time_denominator)
{
	int err;
	u16 val = 0;

	err = sensor_write_reg(info->i2c_client, 0x098C, 0xA21B);
	if (err)
		return err;

	err = sensor_read_reg(info->i2c_client, 0x0990, &val);
	if (err)
		return err;

	// exposure time = 1/frame rate
	// exposure_time_denominator = frame rate
	// when val under 4, camera frame rate is always 30 fps
	if(val < 4)
		*exposure_time_denominator = 30;
	else
		*exposure_time_denominator = (unsigned int)(120/val);

	return 0;
}


static int sensor_set_mode(struct sensor_info *info, int xres, int yres)
{
	int sensor_mode;
	int err;

	dev_dbg(&info->i2c_client->dev,"xres %u yres %u\n", xres, yres);

	mutex_lock(&yuv_lock);
	
	if (xres == 1600 && yres == 1200)
		sensor_mode = SENSOR_SNAPSHOT_MODE;
	else if (xres == 800 && yres == 600)
		sensor_mode = SENSOR_PREVIEW_MODE;
	else {
		dev_err(&info->i2c_client->dev,"invalid resolution supplied to set mode %d %d\n",xres, yres);
		mutex_unlock(&yuv_lock);
		return -EINVAL;
	}

	err = sensor_set_sensor_mode(info, sensor_mode);

	mutex_unlock(&yuv_lock);
	
	return err;
}

static long sensor_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct sensor_info *info = file->private_data;
	int err=0;

	dev_dbg(&info->i2c_client->dev,"cmd %u\n", cmd);
	switch (cmd) {
		case SENSOR_IOCTL_SET_MODE: {
			struct sensor_mode mode;

			if (copy_from_user(&mode,
					(const void __user *)arg,
					sizeof(struct sensor_mode))) {
				dev_err(&info->i2c_client->dev,"set_mode get mode failed\n");
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"set_mode %d x %d\n",mode.xres,mode.yres);

			return sensor_set_mode(info, mode.xres, mode.yres);
		}

		case SENSOR_IOCTL_GET_STATUS:
			dev_dbg(&info->i2c_client->dev,"get_status\n");
			return 0;

		case SENSOR_IOCTL_SET_COLOR_EFFECT: { /* ok */
			int coloreffect;

			if (copy_from_user(&coloreffect,
					(const void __user *)arg,
					sizeof(coloreffect))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"coloreffect %d\n", coloreffect);

			switch(coloreffect) {
				case YUV_ColorEffect_None:
					err = sensor_set_coloreffect(info, CAMERA_EFFECT_OFF);
					break;

				case YUV_ColorEffect_Mono:
					err = sensor_set_coloreffect(info, CAMERA_EFFECT_MONO);
					break;

				case YUV_ColorEffect_Sepia:
					err = sensor_set_coloreffect(info, CAMERA_EFFECT_SEPIA);				
					break;

				case YUV_ColorEffect_Negative:
					err = sensor_set_coloreffect(info, CAMERA_EFFECT_NEGATIVE);				
					break;

				case YUV_ColorEffect_Solarize:
					err = sensor_set_coloreffect(info, CAMERA_EFFECT_SOLARIZE);								
					break;

				case YUV_ColorEffect_Posterize:
					err = sensor_set_coloreffect(info, CAMERA_EFFECT_POSTERIZE);								
					break;

				default:
					break;
			}
			return err;
		}

		case SENSOR_IOCTL_SET_WHITE_BALANCE: { /*ok*/
			int whitebalance;

			if (copy_from_user(&whitebalance,
					(const void __user *)arg,
					sizeof(whitebalance))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"whitebalance %d\n", whitebalance);

			switch(whitebalance) {
				case YUV_Whitebalance_Auto:
					err = sensor_set_wb(info, CAMERA_WB_MODE_AWB);
					break;

				case YUV_Whitebalance_Incandescent:
					err = sensor_set_wb(info, CAMERA_WB_MODE_INCANDESCENT);
					break;

				case YUV_Whitebalance_Daylight:
					err = sensor_set_wb(info, CAMERA_WB_MODE_SUNLIGHT);
					break;

				case YUV_Whitebalance_Fluorescent:
					err = sensor_set_wb(info, CAMERA_WB_MODE_FLUORESCENT);
					break;

				case YUV_Whitebalance_CloudyDaylight:
					err = sensor_set_wb(info, CAMERA_WB_MODE_CLOUDY);
					break;

				default:
					break;
			}
			return err;
				
		}

		case SENSOR_IOCTL_GET_EXPOSURE_TIME: {
			unsigned int exposure_time_denominator;
			sensor_get_exposure_time(info, &exposure_time_denominator);
			if (copy_to_user((void __user *)arg,
					&exposure_time_denominator,
					sizeof(exposure_time_denominator)))
				return -EFAULT;

			dev_info(&info->i2c_client->dev,"exposure time %d\n", exposure_time_denominator);
			return 0;
		}
		
		case SENSOR_IOCTL_SET_SCENE_MODE:
			dev_dbg(&info->i2c_client->dev,"scene_mode\n");
			return 0;

		case SENSOR_IOCTL_SET_EXPOSURE: {
			int exposure;

			if (copy_from_user(&exposure,
					(const void __user *)arg,
					sizeof(exposure))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"exposure %d\n", exposure);

			switch(((int)exposure)-2) {
				case YUV_Exposure_0:
					err = sensor_set_exposure_compensation(info, CAMERA_EXPOSURE_0);
					break;

				case YUV_Exposure_1:
					err = sensor_set_exposure_compensation(info, CAMERA_EXPOSURE_1);
					break;

				case YUV_Exposure_2:
					err = sensor_set_exposure_compensation(info, CAMERA_EXPOSURE_2);
					break;

				case YUV_Exposure_Negative_1:
					err = sensor_set_exposure_compensation(info, CAMERA_EXPOSURE_3);
					break;

				case YUV_Exposure_Negative_2:
					err = sensor_set_exposure_compensation(info, CAMERA_EXPOSURE_4);
					break;

				default:
					break;
			}
			if (err)
				return err;
			return 0;
		}
		

		case SENSOR_IOCTLEX_SET_ISO: {
			int iso;

			if (copy_from_user(&iso,
					(const void __user *)arg,
					sizeof(iso))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"iso %d\n", iso);
			err = sensor_set_iso(info, iso);
			return err;
		}

		case SENSOR_IOCTLEX_SET_ANTIBANDING: {
			int antibanding;

			if (copy_from_user(&antibanding,
					(const void __user *)arg,
					sizeof(antibanding))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"antibanding %d\n", antibanding);
			err = sensor_set_antibanding(info, antibanding);
			return err;
		}

		case SENSOR_IOCTLEX_SET_BRIGHTNESS: {
			int brightness;

			if (copy_from_user(&brightness,
					(const void __user *)arg,
					sizeof(brightness))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"brightness %d\n", brightness);
			err = sensor_set_brightness(info, brightness);
			return err;
		}

		case SENSOR_IOCTLEX_SET_SATURATION: {
			int saturation;

			if (copy_from_user(&saturation,
					(const void __user *)arg,
					sizeof(saturation))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"saturation %d\n", saturation);
			err = sensor_set_saturation(info, saturation);
			return err;
		}

		case SENSOR_IOCTLEX_SET_CONTRAST: {
			int contrast;

			if (copy_from_user(&contrast,
					(const void __user *)arg,
					sizeof(contrast))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"contrast %d\n", contrast);
			err = sensor_set_contrast(info, contrast);
			return err;
		}

		case SENSOR_IOCTLEX_SET_SHARPNESS: {
			int sharpness;

			if (copy_from_user(&sharpness,
					(const void __user *)arg,
					sizeof(sharpness))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"sharpness %d\n", sharpness);
			err = sensor_set_sharpness(info, sharpness);
			return err;
		}

		case SENSOR_IOCTLEX_SET_EXPOSURE_COMP: {
			int exposure_comp;

			if (copy_from_user(&exposure_comp,
					(const void __user *)arg,
					sizeof(exposure_comp))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"exposure_comp %d\n", exposure_comp);
			err = sensor_set_exposure_compensation(info, exposure_comp);
			return err;
		}


		default:
			dev_err(&info->i2c_client->dev,"default\n");
			return -EINVAL;
	}

	return 0;
}

static struct sensor_info *info = NULL;

extern void extern_tegra_camera_enable_vi(void);
extern void extern_tegra_camera_disable_vi(void);
extern void extern_tegra_camera_clk_set_rate(struct tegra_camera_clk_info *);

static int sensor_initialize(struct sensor_info *info)
{
	int err;
	u16 model_id;
	struct tegra_camera_clk_info clk_info;
	
	dev_dbg(&info->i2c_client->dev,"%s++\n", __func__);


	// set MCLK to 24MHz - Lower or higher frecuencies do not work!
	clk_info.id = TEGRA_CAMERA_MODULE_VI;
	clk_info.clk_id = TEGRA_CAMERA_VI_SENSOR_CLK;
	clk_info.rate = MT9D115_CAMIO_MCLK;
	extern_tegra_camera_clk_set_rate(&clk_info);

	// turn on MCLK and pull down PWDN pin
	extern_tegra_camera_enable_vi();
	mdelay(5);
	
	// Out of standby
	if (info->pdata && info->pdata->power_on)
		info->pdata->power_on(); 
		
	model_id = 0x0000;
	err = sensor_read_reg(info->i2c_client, REG_MT9D115_MODEL_ID, &model_id);
    if (err) {
		dev_err(&info->i2c_client->dev,"failed to read model");
        goto quit;
    }

    dev_dbg(&info->i2c_client->dev,"model_id = 0x%x", model_id);
    if (model_id != MT9D115_MODEL_ID) {
		dev_err(&info->i2c_client->dev,"unexpected model_id = 0x%x, required = 0x%x", model_id, MT9D115_MODEL_ID);
    } 

    // Enable PLL in order to read reg of REG_MT9D115_MODEL_ID_SUB successfully
    err = sensor_write_table(info->i2c_client, pll_tbl);
    if (err) {
		dev_err(&info->i2c_client->dev,"Unable to enable PLL");
        goto quit;
    }

    model_id = 0x0000;
    err = sensor_read_reg(info->i2c_client, REG_MT9D115_MODEL_ID_SUB, &model_id);
    if (err) {
		dev_err(&info->i2c_client->dev,"failed to read submodel");
        return err;
    }
    dev_dbg(&info->i2c_client->dev,"submodel_id = 0x%x\n", model_id);
    if (model_id != MT9D115_MODEL_ID_SUB) {
		dev_err(&info->i2c_client->dev,"unexpected submodel_id = 0x%x, required = 0x%x", model_id, MT9D115_MODEL_ID_SUB);
    }
	
	// Preview & Snapshot Setup 
	err = sensor_write_table(info->i2c_client, prev_snap_tbl);
    if (err) {
		dev_err(&info->i2c_client->dev,"Unable to setup preview and snapshot");
        goto quit;
    }

    /*
     * Set Standby Control Register
     *
     * Enable Interrupt request
     * Exit Soft standby
     */
    err = sensor_write_reg(info->i2c_client, REG_MT9D115_STANDBY_CONTROL, 0x0028);
    if (err) {
		dev_err(&info->i2c_client->dev,"Unable to exit soft standby");
        goto quit;
    }
    mdelay(100);

    /*
     * Enable refresh mode
     * i.e, set value of 0x0006 or 0x0005 to register 0xA103
     */
    err = sensor_write_reg(info->i2c_client, 0x098C, 0xA103);
    if (err) {
		dev_err(&info->i2c_client->dev,"failed to enable refresh mode [1]");
        goto quit;
    }
    err = sensor_write_reg(info->i2c_client, 0x0990, 0x0006);
    if (err) {
		dev_err(&info->i2c_client->dev,"failed to enable refresh mode [2]");
        goto quit;
    }

    mdelay(200);

    /*
     * Enable refresh mode
     * i.e, set value of 0x0006 or 0x0005 to register 0xA103
     */
    err = sensor_write_reg(info->i2c_client, 0x098C, 0xA103);
    if (err) {
		dev_err(&info->i2c_client->dev,"failed to enable refresh mode [3]");
        goto quit;
    }
    err = sensor_write_reg(info->i2c_client,  0x0990, 0x0005);
    if (err) {
		dev_err(&info->i2c_client->dev,"failed to enable refresh mode [4]");
        goto quit;
    }

quit:
	// pull high PWDN pin and turn off MCLK
	if (info->pdata && info->pdata->power_off)
		info->pdata->power_off();
	extern_tegra_camera_disable_vi(); 
	
	dev_dbg(&info->i2c_client->dev,"%s--\n", __func__);
	return err;
}


static int sensor_open(struct inode *inode, struct file *file)
{
	dev_dbg(&info->i2c_client->dev,"%s\n", __func__);
	
	file->private_data = info;
	if (info->pdata && info->pdata->power_on)
		info->pdata->power_on();
		
	return 0;
}

int sensor_release(struct inode *inode, struct file *file)
{
	struct sensor_info *info = file->private_data;
	if (!info)
		return 0;
		
	dev_dbg(&info->i2c_client->dev,"%s\n", __func__);
	
	if (info->pdata && info->pdata->power_off)
		info->pdata->power_off();
	file->private_data = NULL;

	return 0;
}

static const struct file_operations sensor_fileops = {
	.owner = THIS_MODULE,
	.open = sensor_open,
	.unlocked_ioctl = sensor_ioctl,
	.release = sensor_release,
};

static struct miscdevice sensor_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = SENSOR_NAME,
	.fops = &sensor_fileops,
};

/* Emulate ASUS picasso sensors so we can use its Nvidia propietary Camera libs */

/* ----- 5M camera sensor MOCK ----- */

struct sensor_5m_mode {
	int xres;
	int yres;
};

enum {
	YUV_5M_ColorEffect = 0,
	YUV_5M_Whitebalance,
	YUV_5M_SceneMode,
	YUV_5M_Exposure,
	YUV_5M_FlashMode
};

enum {
	YUV_5M_ColorEffect_Invalid = 0,
	YUV_5M_ColorEffect_Aqua,
	YUV_5M_ColorEffect_Blackboard,
	YUV_5M_ColorEffect_Mono,
	YUV_5M_ColorEffect_Negative,
	YUV_5M_ColorEffect_None,
	YUV_5M_ColorEffect_Posterize,
	YUV_5M_ColorEffect_Sepia,
	YUV_5M_ColorEffect_Solarize,
	YUV_5M_ColorEffect_Whiteboard
};

enum {
	YUV_5M_Whitebalance_Invalid = 0,
	YUV_5M_Whitebalance_Auto,
	YUV_5M_Whitebalance_Incandescent,
	YUV_5M_Whitebalance_Fluorescent,
	YUV_5M_Whitebalance_WarmFluorescent,
	YUV_5M_Whitebalance_Daylight,
	YUV_5M_Whitebalance_CloudyDaylight,
	YUV_5M_Whitebalance_Shade,
	YUV_5M_Whitebalance_Twilight,
	YUV_5M_Whitebalance_Custom
};

enum {
	YUV_5M_SceneMode_Invalid = 0,
	YUV_5M_SceneMode_Auto,
	YUV_5M_SceneMode_Action,
	YUV_5M_SceneMode_Portrait,
	YUV_5M_SceneMode_Landscape,
	YUV_5M_SceneMode_Beach,
	YUV_5M_SceneMode_Candlelight,
	YUV_5M_SceneMode_Fireworks,
	YUV_5M_SceneMode_Night,
	YUV_5M_SceneMode_NightPortrait,
	YUV_5M_SceneMode_Party,
	YUV_5M_SceneMode_Snow,
	YUV_5M_SceneMode_Sports,
	YUV_5M_SceneMode_SteadyPhoto,
	YUV_5M_SceneMode_Sunset,
	YUV_5M_SceneMode_Theatre,
	YUV_5M_SceneMode_Barcode
};

enum {
	YUV_5M_FlashControlOn = 0,
	YUV_5M_FlashControlOff,
	YUV_5M_FlashControlAuto,
	YUV_5M_FlashControlRedEyeReduction,
	YUV_5M_FlashControlFillin,
	YUV_5M_FlashControlTorch,
};

#define SENSOR_5M_IOCTL_SET_MODE           _IOW('o', 1, struct sensor_5m_mode)
#define SENSOR_5M_IOCTL_GET_STATUS         _IOR('o', 2, __u8)
#define SENSOR_5M_IOCTL_SET_COLOR_EFFECT   _IOW('o', 3, __u8)
#define SENSOR_5M_IOCTL_SET_WHITE_BALANCE  _IOW('o', 4, __u8)
#define SENSOR_5M_IOCTL_SET_SCENE_MODE     _IOW('o', 5, __u8)
#define SENSOR_5M_IOCTL_SET_AF_MODE        _IOW('o', 6, __u8)
#define SENSOR_5M_IOCTL_GET_AF_STATUS      _IOW('o', 7, __u8)
#define SENSOR_5M_IOCTL_SET_EXPOSURE       _IOW('o', 8, __u8)
#define SENSOR_5M_IOCTL_SET_FLASH_MODE     _IOW('o', 9, __u8)
#define SENSOR_5M_IOCTL_GET_CAPTURE_FRAME_RATE _IOR('o',10, __u8)
#define SENSOR_5M_IOCTL_CAPTURE_CMD        _IOW('o', 11, __u8)
#define SENSOR_5M_IOCTL_GET_FLASH_STATUS   _IOW('o', 12, __u8) 

static long sensor_5m_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct sensor_info *info = file->private_data;
	int err = 0;
	
	dev_dbg(&info->i2c_client->dev,"cmd %u\n", cmd);
	switch (cmd) {
		case SENSOR_5M_IOCTL_SET_MODE: {
			struct sensor_5m_mode mode;

			if (copy_from_user(&mode,
					(const void __user *)arg,
					sizeof(struct sensor_5m_mode))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"yuv5 set_mode %d x %d\n",mode.xres,mode.yres);
			
			return sensor_set_mode(info, mode.xres, mode.yres);
		}

		case SENSOR_5M_IOCTL_GET_STATUS:
			dev_dbg(&info->i2c_client->dev,"yuv5 get_status\n");
			return 0;

		case SENSOR_5M_IOCTL_GET_AF_STATUS:
			dev_dbg(&info->i2c_client->dev,"yuv5 get_af_status\n");
			// 1=NvCameraIspFocusStatus_Locked
			// 2=NvCameraIspFocusStatus_FailedToFind
			// 0=NvCameraIspFocusStatus_Busy
			return 1;


		case SENSOR_5M_IOCTL_SET_AF_MODE:
			dev_dbg(&info->i2c_client->dev,"yuv5 set_af_mode\n");
			return 0;

		case SENSOR_5M_IOCTL_GET_FLASH_STATUS:
			dev_dbg(&info->i2c_client->dev,"yuv5 get_flash_status\n");
			return 1; //ON

		case SENSOR_5M_IOCTL_SET_COLOR_EFFECT: { /*ok*/
			u8 coloreffect;

			if (copy_from_user(&coloreffect,
					(const void __user *)arg,
					sizeof(coloreffect))) {
				return -EFAULT;
			}
			
			dev_dbg(&info->i2c_client->dev,"yuv5 coloreffect %d\n", coloreffect);
			
			switch(coloreffect) {
				case YUV_5M_ColorEffect_None:
					err = sensor_set_coloreffect(info, CAMERA_EFFECT_OFF);
					break;

				case YUV_5M_ColorEffect_Mono:
					err = sensor_set_coloreffect(info, CAMERA_EFFECT_MONO);
					break;

				case YUV_5M_ColorEffect_Sepia:
					err = sensor_set_coloreffect(info, CAMERA_EFFECT_SEPIA);				
					break;

				case YUV_5M_ColorEffect_Negative:
					err = sensor_set_coloreffect(info, CAMERA_EFFECT_NEGATIVE);				
					break;

				case YUV_5M_ColorEffect_Solarize:
					err = sensor_set_coloreffect(info, CAMERA_EFFECT_SOLARIZE);								
					break;

				case YUV_5M_ColorEffect_Posterize:
					err = sensor_set_coloreffect(info, CAMERA_EFFECT_POSTERIZE);								
					break;

				default:
					break;
			}
			return err;
		}

		case SENSOR_5M_IOCTL_SET_WHITE_BALANCE: { /*ok*/
			u8 whitebalance;

			if (copy_from_user(&whitebalance,
					(const void __user *)arg,
					sizeof(whitebalance))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"yuv5 whitebalance %d\n", whitebalance);

			switch(whitebalance) {
				case YUV_5M_Whitebalance_Auto:
					err = sensor_set_wb(info, CAMERA_WB_MODE_AWB);
					break;

				case YUV_5M_Whitebalance_Incandescent:
					err = sensor_set_wb(info, CAMERA_WB_MODE_INCANDESCENT);
					break;

				case YUV_5M_Whitebalance_Daylight:
					err = sensor_set_wb(info, CAMERA_WB_MODE_SUNLIGHT);
					break;

				case YUV_5M_Whitebalance_Fluorescent:
					err = sensor_set_wb(info, CAMERA_WB_MODE_FLUORESCENT);
					break;

				case YUV_5M_Whitebalance_CloudyDaylight:
					err = sensor_set_wb(info, CAMERA_WB_MODE_CLOUDY);
					break;

				default:
					break;
			}
			return err;
				
		}

		case SENSOR_5M_IOCTL_SET_SCENE_MODE: {
			u8 scenemode;
			if (copy_from_user(&scenemode,
					(const void __user *)arg,
					sizeof(scenemode))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"yuv5 scenemode %d\n", scenemode);
			return 0;
		}

		case SENSOR_5M_IOCTL_SET_EXPOSURE: {
			u8 exposure;

			if (copy_from_user(&exposure,
					(const void __user *)arg,
					sizeof(exposure))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"yuv5 exposure %d\n", exposure);
			switch(((int)exposure) - 2) {
				case YUV_Exposure_0:
					err = sensor_set_exposure_compensation(info, CAMERA_EXPOSURE_0);
					break;

				case YUV_Exposure_1:
					err = sensor_set_exposure_compensation(info, CAMERA_EXPOSURE_1);
					break;

				case YUV_Exposure_2:
					err = sensor_set_exposure_compensation(info, CAMERA_EXPOSURE_2);
					break;

				case YUV_Exposure_Negative_1:
					err = sensor_set_exposure_compensation(info, CAMERA_EXPOSURE_3);
					break;

				case YUV_Exposure_Negative_2:
					err = sensor_set_exposure_compensation(info, CAMERA_EXPOSURE_4);
					break;

				default:
					break;
			}
			if (err)
				return err;
			return 0;
		}

		case SENSOR_5M_IOCTL_SET_FLASH_MODE: {
			u8 flashmode;

			if (copy_from_user(&flashmode,
					(const void __user *)arg,
					sizeof(flashmode))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"yuv5 flashmode %d\n", (int)flashmode);
			return 0;
		}

		case SENSOR_5M_IOCTL_GET_CAPTURE_FRAME_RATE: {
			unsigned int capture_frame_rate = 0;
			sensor_get_exposure_time(info, &capture_frame_rate);
			if (copy_to_user((void __user *)arg,
					&capture_frame_rate,
					sizeof(capture_frame_rate))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"yuv5 get_capture_frame_rate: %d fps",capture_frame_rate);
			return 0;
		}
		
		case SENSOR_5M_IOCTL_CAPTURE_CMD:
			dev_dbg(&info->i2c_client->dev,"yuv5 capture cmd\n");
			return 0;

		case SENSOR_IOCTLEX_SET_ISO: {
			int iso;

			if (copy_from_user(&iso,
					(const void __user *)arg,
					sizeof(iso))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"iso %d\n", iso);
			err = sensor_set_iso(info, iso);
			return err;
		}

		case SENSOR_IOCTLEX_SET_ANTIBANDING: {
			int antibanding;

			if (copy_from_user(&antibanding,
					(const void __user *)arg,
					sizeof(antibanding))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"antibanding %d\n", antibanding);
			err = sensor_set_antibanding(info, antibanding);
			return err;
		}

		case SENSOR_IOCTLEX_SET_BRIGHTNESS: {
			int brightness;

			if (copy_from_user(&brightness,
					(const void __user *)arg,
					sizeof(brightness))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"brightness %d\n", brightness);
			err = sensor_set_brightness(info, brightness);
			return err;
		}

		case SENSOR_IOCTLEX_SET_SATURATION: {
			int saturation;

			if (copy_from_user(&saturation,
					(const void __user *)arg,
					sizeof(saturation))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"saturation %d\n", saturation);
			err = sensor_set_saturation(info, saturation);
			return err;
		}

		case SENSOR_IOCTLEX_SET_CONTRAST: {
			int contrast;

			if (copy_from_user(&contrast,
					(const void __user *)arg,
					sizeof(contrast))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"contrast %d\n", contrast);
			err = sensor_set_contrast(info, contrast);
			return err;
		}

		case SENSOR_IOCTLEX_SET_SHARPNESS: {
			int sharpness;

			if (copy_from_user(&sharpness,
					(const void __user *)arg,
					sizeof(sharpness))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"sharpness %d\n", sharpness);
			err = sensor_set_sharpness(info, sharpness);
			return err;
		}

		case SENSOR_IOCTLEX_SET_EXPOSURE_COMP: {
			int exposure_comp;

			if (copy_from_user(&exposure_comp,
					(const void __user *)arg,
					sizeof(exposure_comp))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"exposure_comp %d\n", exposure_comp);
			err = sensor_set_exposure_compensation(info, exposure_comp);
			return err;
		}
			
		default:
			dev_err(&info->i2c_client->dev,"yuv5 default\n");
			return -EINVAL;
	}

	return 0;
}

static int sensor_5m_open(struct inode *inode, struct file *file)
{
	dev_dbg(&info->i2c_client->dev,"%s\n", __func__);
	
	file->private_data = info;
	if (info->pdata && info->pdata->power_on)
		info->pdata->power_on();

	return 0;
}

int sensor_5m_release(struct inode *inode, struct file *file)
{
	struct sensor_info *info = file->private_data;
	if (!info)
		return 0;
		
	dev_dbg(&info->i2c_client->dev,"%s\n", __func__);
	
	if (info->pdata && info->pdata->power_off)
		info->pdata->power_off();
	file->private_data = NULL;

	return 0;
}

static const struct file_operations sensor_5m_fileops = {
	.owner = THIS_MODULE,
	.open = sensor_5m_open,
	.unlocked_ioctl = sensor_5m_ioctl,
	.release = sensor_5m_release,
};

static struct miscdevice sensor_5m_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mt9p111",
	.fops = &sensor_5m_fileops,
};

/* ----- AD5820 focuser MOCK ----- */

#define AD5820_IOCTL_GET_CONFIG   _IOR('o', 1, struct ad5820_config)
#define AD5820_IOCTL_SET_POSITION _IOW('o', 2, u32)

#define SETTLETIME_MS 5
#define FOCAL_LENGTH (4.507f)
#define FNUMBER (2.8f)
#define POS_LOW (0)
#define POS_HIGH (1023)

static const struct ad5820_config {
	__u32 settle_time;
	__u32 actuator_range;
	__u32 pos_low;
	__u32 pos_high;
	float focal_length;
	float fnumber;
	float max_aperture;
} ad5820_cfg = {
	.settle_time = SETTLETIME_MS,
	.focal_length = FOCAL_LENGTH,
	.fnumber = FNUMBER,
	.pos_low = POS_LOW,
	.pos_high = POS_HIGH,
};

static long ad5820_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case AD5820_IOCTL_GET_CONFIG:
	{
		if (copy_to_user((void __user *) arg,
				 &ad5820_cfg,
				 sizeof(ad5820_cfg))) {
			pr_err("%s: 0x%x\n", __func__, __LINE__);
			return -EFAULT;
		}

		break;
	}
	case AD5820_IOCTL_SET_POSITION:
		return 0;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ad5820_open(struct inode *inode, struct file *file)
{
	file->private_data = file;
	return 0;
}

int ad5820_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static const struct file_operations ad5820_fileops = {
	.owner = THIS_MODULE,
	.open = ad5820_open,
	.unlocked_ioctl = ad5820_ioctl,
	.release = ad5820_release,
};

static struct miscdevice ad5820_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ad5820",
	.fops = &ad5820_fileops,
};

/* ----- LTC3216 torch controller MOCK ----- */

#define LTC3216_IOCTL_FLASH_ON   _IOW('o', 1, __u16)
#define LTC3216_IOCTL_FLASH_OFF  _IOW('o', 2, __u16)
#define LTC3216_IOCTL_TORCH_ON   _IOW('o', 3, __u16)
#define LTC3216_IOCTL_TORCH_OFF  _IOW('o', 4, __u16) 

static long ltc3216_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case LTC3216_IOCTL_FLASH_ON:
		break;

	case LTC3216_IOCTL_FLASH_OFF:
		break;
		
	case LTC3216_IOCTL_TORCH_ON:
		break;

	case LTC3216_IOCTL_TORCH_OFF:
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int ltc3216_open(struct inode *inode, struct file *file)
{
	file->private_data = info;
	return 0;
}

int ltc3216_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static const struct file_operations ltc3216_fileops = {
	.owner = THIS_MODULE,
	.open = ltc3216_open,
	.unlocked_ioctl = ltc3216_ioctl,
	.release = ltc3216_release,
};

static struct miscdevice ltc3216_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ltc3216",
	.fops = &ltc3216_fileops,
}; 

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	dev_info(&client->dev,"%s\n", __func__);

	info = devm_kzalloc(&client->dev,
				sizeof(struct sensor_info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev,"Unable to allocate memory\n");
		return -ENOMEM;
	}

	mutex_init(&yuv_lock);

	info->pdata = client->dev.platform_data;
	info->i2c_client = client;
	i2c_set_clientdata(client, info);
	
	// Initialize the image sensor
	if (sensor_initialize(info) != 0) {
		dev_err(&client->dev,"mt9d115 image sensor not detected\n");
		return -ENODEV;
	} 

	misc_register(&sensor_device);
	misc_register(&sensor_5m_device);
	misc_register(&ad5820_device);
	misc_register(&ltc3216_device);
	
	dev_info(&client->dev,"mt9d115 image sensor driver loaded\n");
	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct sensor_info *info = i2c_get_clientdata(client);
	if (!info)
		return 0;
		
	misc_deregister(&ltc3216_device);
	misc_deregister(&ad5820_device);
	misc_deregister(&sensor_5m_device);
	misc_deregister(&sensor_device);

	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{ SENSOR_NAME, 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_i2c_driver = {
	.driver = {
		.name = SENSOR_NAME,
		.owner = THIS_MODULE,
	},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

module_i2c_driver(sensor_i2c_driver);
