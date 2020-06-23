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

/* Sensor I2C Device ID */
#define REG_MT9D115_MODEL_ID    	0x0000
#define MT9D115_MODEL_ID        	0x2580

/* Sensor I2C Device Sub ID */
#define REG_MT9D115_MODEL_ID_SUB    0x31FE
#define MT9D115_MODEL_ID_SUB        0x0011

/* SOC Registers */
#define REG_MT9D115_STANDBY_CONTROL	0x0018
#define		SYSCTL_STANDBY_DONE			(1<<14) 

#define REG_MT9D115_WIDTH      		0x2703
 
#define SENSOR_640_WIDTH_VAL  	0x280
#define SENSOR_800_WIDTH_VAL  	0x320
#define SENSOR_1280_WIDTH_VAL  0x500
#define SENSOR_1600_WIDTH_VAL 	0x640


#define SENSOR_TABLE_END     			0 /* special number to indicate this is end of table */
#define SENSOR_WAIT_MS       			1 /* special number to indicate this is wait time require */
#define SENSOR_WAIT_OOR				2 /* special number to indicate to wait sensor to get out of reset */
#define SENSOR_WAIT_PLL_LOCK		3 /* special number to indicate waiting for PLL lock */
#define SENSOR_WAIT_STDBY_EXIT	4 /* special number to indicate waiting for standby exit */
#define SENSOR_WAIT_PATCH				5 /* special number to indicate waiting for patch end of execution */ 
#define SENSOR_WAIT_SEQUENCER	6 /* special number to indicate waiting for sequencer to enter a given mode */ 

#define SENSOR_MAX_RETRIES   3 /* max counter for retry I2C access */ 

struct sensor_reg {
	u16 addr;
	u16 val;
};

/* Select context B (1600x1200) and capture 10 frames, then revert to ctx A */
static struct sensor_reg mode_1600x1200[] = {
	{0x098C, 0xA115},	// MCU_ADDRESS [SEQ_CAP_MODE]
	{0x0990, 0x0000},	// MCU_DATA_0
	{0x098C, 0xA116},	// MCU_ADDRESS [SEQ_CAP_NUMFRAMES]
	{0x0990, 0x000A},	// MCU_DATA_0
	// Disable avoid to effect MWB
	//{0x098C, 0xA11F},	// MCU_ADDRESS [SEQ_PREVIEW_1_AWB]
	//{0x0990, 0x0001},	// MCU_DATA_0
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0002},	// MCU_DATA_0
	{SENSOR_WAIT_MS, 50},
	{SENSOR_TABLE_END, 0x0000}
}; // End of mode_1600x1200[]

/*  CTS testing will set the largest resolution mode only during testZoom.
 *  Fast switching function will break the testing and fail.
 *  To workaround this, we need to create a 1600x1200 table
 */
static struct sensor_reg CTS_ZoomTest_mode_1600x1200[] = {
	{0x098C, 0x2703},	// Output Width (A)
	{0x0990, 0x0640},	//       = 1600
	{0x098C, 0x2705},	// Output Height (A)
	{0x0990, 0x04B0},	//       = 1200
	{0x098C, 0x2707},	// Output Width (B)
	{0x0990, 0x0640},	//       = 1600
	{0x098C, 0x2709},	// Output Height (B)
	{0x0990, 0x04B0},	//       = 1200
	
	{0x098C, 0x270D},	// Row Start (A)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x270F},	// Column Start (A)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x2711},	// Row End (A)
	{0x0990, 0x04BB},	//       = 1211
	{0x098C, 0x2713},	// Column End (A)
	{0x0990, 0x064B},	//       = 1611
	{0x098C, 0x2715},	// Row Speed (A)
	{0x0990, 0x0111},	//       = 273
	{0x098C, 0x2717},	// Read Mode (A)
	{0x0990, 0x0024},	//       = 36
	//{0x0990, 0x0025},	//       = 37 mirror
	{0x098C, 0x2719},	// sensor_fine_correction (A)
	{0x0990, 0x003A},	//       = 58
	{0x098C, 0x271B},	// sensor_fine_IT_min (A)
	{0x0990, 0x00F6},	//       = 246
	{0x098C, 0x271D},	// sensor_fine_IT_max_margin (A)
	{0x0990, 0x008B},	//       = 139
	{0x098C, 0x271F},	// Frame Lines (A)
	{0x0990, 0x0521},	//       = 1313
	{0x098C, 0x2721},	// Line Length (A)
	{0x0990, 0x08EC},	//       = 2284
	
	{0x098C, 0x2723},	// Row Start (B)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x2725},	// Column Start (B)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x2727},	// Row End (B)
	{0x0990, 0x04BB},	//       = 1211
	{0x098C, 0x2729},	// Column End (B)
	{0x0990, 0x064B},	//       = 1611
	{0x098C, 0x272B},	// Row Speed (B)
	{0x0990, 0x0111},	//       = 273
	{0x098C, 0x272D},	// Read Mode (B)
	{0x0990, 0x0024},	//       = 36
	//{0x0990, 0x0025},	//       = 37 mirror
	{0x098C, 0x272F},	// sensor_fine_correction (B)
	{0x0990, 0x003A},	//       = 58
	{0x098C, 0x2731},	// sensor_fine_IT_min (B)
	{0x0990, 0x00F6},	//       = 246
	{0x098C, 0x2733},	// sensor_fine_IT_max_margin (B)
	{0x0990, 0x008B},	//       = 139
	{0x098C, 0x2735},	// Frame Lines (B)
	{0x0990, 0x0521},	//       = 1313
	{0x098C, 0x2737},	// Line Length (B)
	{0x0990, 0x08EC},	//       = 2284
	
	{0x098C, 0x2739},	// Crop_X0 (A)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x273B},	// Crop_X1 (A)
	{0x0990, 0x063F},	//       = 1599
	{0x098C, 0x273D},	// Crop_Y0 (A)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x273F},	// Crop_Y1 (A)
	{0x0990, 0x04AF},	//       = 1199
	{0x098C, 0x2747},	// Crop_X0 (B)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x2749},	// Crop_X1 (B)
	{0x0990, 0x063F},	//       = 1599
	{0x098C, 0x274B},	// Crop_Y0 (B)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x274D},	// Crop_Y1 (B)
	{0x0990, 0x04AF},	//       = 1199
	
	{0x098C, 0x222D},	// R9 Step
	{0x0990, 0x00C5},	//       = 197
	{0x098C, 0xA408},	// search_f1_50
	{0x0990, 0x0030},	//       = 48
	{0x098C, 0xA409},	// search_f2_50
	{0x0990, 0x0032},	//       = 50
	{0x098C, 0xA40A},// search_f1_60
	{0x0990, 0x003A},	//       = 58
	{0x098C, 0xA40B},// search_f2_60
	{0x0990, 0x003C},	//       = 60
	{0x098C, 0x2411},	// R9_Step_60 (A)
	{0x0990, 0x0099},	//       = 153
	{0x098C, 0x2413},	// R9_Step_50 (A)
	{0x0990, 0x00B8},	//       = 184
	{0x098C, 0x2415},	// R9_Step_60 (B)
	{0x0990, 0x0099},	//       = 153
	{0x098C, 0x2417},	// R9_Step_50 (B)
	{0x0990, 0x00B8},	//       = 184
	
	{SENSOR_TABLE_END, 0x0000}
}; // End of CTS_ZoomTest_mode_1600x1200[]

static struct sensor_reg mode_1280x720[] = {

	// sensor core & flicker timings
	{0x098C, 0x2703},	// Output Width (A)
	{0x0990, 0x0500},	//       = 1280
	{0x098C, 0x2705},	// Output Height (A)
	{0x0990, 0x02D0},	//       = 720
	{0x098C, 0x2707},	// Output Width (B)
	{0x0990, 0x0640},	//       = 1600
	{0x098C, 0x2709},	// Output Height (B)
	{0x0990, 0x04B0},	//       = 1200

	{0x098C, 0x270D},	// Row Start (A)
	{0x0990, 0x00F6},	//       = 246
	{0x098C, 0x270F},	// Column Start (A)
	{0x0990, 0x00A6},	//       = 166
	{0x098C, 0x2711},	// Row End (A)
	{0x0990, 0x03CD},	//       = 973
	{0x098C, 0x2713},	// Column End (A)
	{0x0990, 0x05AD},	//       = 1453
	{0x098C, 0x2715},	// Row Speed (A)
	{0x0990, 0x0111},	//       = 273
	{0x098C, 0x2717},	// Read Mode (A)
	{0x0990, 0x0024},	//       = 36
	//{0x0990, 0x0025},	//       = 37 mirror
	{0x098C, 0x2719},	// sensor_fine_correction (A)
	{0x0990, 0x003A},	//       = 58
	{0x098C, 0x271B},	// sensor_fine_IT_min (A)
	{0x0990, 0x00F6},	//       = 246
	{0x098C, 0x271D},	// sensor_fine_IT_max_margin (A)
	{0x0990, 0x008B},	//       = 139
	{0x098C, 0x271F},	// Frame Lines (A)
	{0x0990, 0x032D},	//       = 813
	{0x098C, 0x2721},	// Line Length (A)
	{0x0990, 0x06F4},	//       = 1780

	{0x098C, 0x2723},	// Row Start (B)
	{0x0990, 0x0004},	//       = 4
	{0x098C, 0x2725},	// Column Start (B)
	{0x0990, 0x0004},	//       = 4
	{0x098C, 0x2727},	// Row End (B)
	{0x0990, 0x04BB},	//       = 1211
	{0x098C, 0x2729},	// Column End (B)
	{0x0990, 0x064B},	//       = 1611
	{0x098C, 0x272B},	// Row Speed (B)
	{0x0990, 0x0111},	//       = 273
	{0x098C, 0x272D},	// Read Mode (B)
	{0x0990, 0x0024},	//       = 36
	//{0x0990, 0x0025},	//       = 37 mirror
	{0x098C, 0x272F},	// sensor_fine_correction (B)
	{0x0990, 0x003A},	//       = 58
	{0x098C, 0x2731},	// sensor_fine_IT_min (B)
	{0x0990, 0x00F6},	//       = 246
	{0x098C, 0x2733},	// sensor_fine_IT_max_margin (B)
	{0x0990, 0x008B},	//       = 139
	{0x098C, 0x2735},	// Frame Lines (B)
	{0x0990, 0x0521},	//       = 1313
	{0x098C, 0x2737},	// Line Length (B)
	{0x0990, 0x08EC},	//       = 2284

	{0x098C, 0x2739},	// Crop_X0 (A)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x273B},	// Crop_X1 (A)
	{0x0990, 0x04FF},	//       = 1279
	{0x098C, 0x273D},	// Crop_Y0 (A)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x273F},	// Crop_Y1 (A)
	{0x0990, 0x02CF},	//       = 719
	{0x098C, 0x2747},	// Crop_X0 (B)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x2749},	// Crop_X1 (B)
	{0x0990, 0x063F},	//       = 1599
	{0x098C, 0x274B},	// Crop_Y0 (B)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x274D},	// Crop_Y1 (B)
	{0x0990, 0x04AF},	//       = 1199

	{0x098C, 0x222D},	// R9 Step
	{0x0990, 0x00C5},	//       = 197
	{0x098C, 0xA408},	// search_f1_50
	{0x0990, 0x0030},	//       = 48
	{0x098C, 0xA409},	// search_f2_50
	{0x0990, 0x0032},	//       = 50
	{0x098C, 0xA40A}, // search_f1_60
	{0x0990, 0x003A},	//       = 58
	{0x098C, 0xA40B},// search_f2_60
	{0x0990, 0x003C},	//       = 60
	{0x098C, 0x2411},	// R9_Step_60 (A)
	{0x0990, 0x00C5},	//       = 197
	{0x098C, 0x2413},	// R9_Step_50 (A)
	{0x0990, 0x00EC},	//       = 236
	{0x098C, 0x2415},	// R9_Step_60 (B)
	{0x0990, 0x0099},	//       = 153
	{0x098C, 0x2417},	// R9_Step_50 (B)
	{0x0990, 0x00B8},	//       = 184
	
	{SENSOR_TABLE_END, 0x0000}
}; // End of mode_1280x720[]

// ===============================================================================================

static struct sensor_reg mode_800x600[] = {
	{0x098C, 0x2703},	// Output Width (A)
	{0x0990, 0x0320},	//       = 800
	{0x098C, 0x2705},	// Output Height (A)
	{0x0990, 0x0258},	//       = 600

	{0x098C, 0x2707},	// Output Width (B)
	{0x0990, 0x0640},	//       = 1600
	{0x098C, 0x2709},	// Output Height (B)
	{0x0990, 0x04B0},	//       = 1200

	{0x098C, 0x270D},	// Row Start (A)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x270F},	// Column Start (A)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x2711},	// Row End (A)
	{0x0990, 0x04BD},	//       = 1213
	{0x098C, 0x2713},	// Column End (A)
	{0x0990, 0x064D},	//       = 1613
	{0x098C, 0x2715},	// Row Speed (A)
	{0x0990, 0x0111},	//       = 273
	{0x098C, 0x2717},	// Read Mode (A)
	{0x0990, 0x046C},	//       = 1132
	//{0x990, 0x046D},	//      = 1133 (Mirror)
	{0x098C, 0x2719},	// sensor_fine_correction (A)
	{0x0990, 0x005A},	//       = 90
	{0x098C, 0x271B},	// sensor_fine_IT_min (A)
	{0x0990, 0x01BE},	//       = 446
	{0x098C, 0x271D},	// sensor_fine_IT_max_margin (A)
	{0x0990, 0x0131},	//       = 305
	{0x098C, 0x271F},	// Frame Lines (A)
	{0x0990, 0x02C3},	//       = 707
	{0x098C, 0x2721},	// Line Length (A)
	{0x0990, 0x07BC},	//       = 1980

	{0x098C, 0x2723},	// Row Start (B)
	{0x0990, 0x0004},	//       = 4
	{0x098C, 0x2725},	// Column Start (B)
	{0x0990, 0x0004},	//       = 4
	{0x098C, 0x2727},	// Row End (B)
	{0x0990, 0x04BB},	//       = 1211
	{0x098C, 0x2729},	// Column End (B)
	{0x0990, 0x064B},	//       = 1611
	{0x098C, 0x272B},	// Row Speed (B)
	{0x0990, 0x0111},	//       = 273
	{0x098C, 0x272D},	// Read Mode (B)
	{0x0990, 0x0024},	//       = 36
	//{0x0990, 0x0025},	//       = 37 mirror
	{0x098C, 0x272F},	// sensor_fine_correction (B)
	{0x0990, 0x003A},	//       = 58
	{0x098C, 0x2731},	// sensor_fine_IT_min (B)
	{0x0990, 0x00F6},	//       = 246
	{0x098C, 0x2733},	// sensor_fine_IT_max_margin (B)
	{0x0990, 0x008B},	//       = 139
	{0x098C, 0x2735},	// Frame Lines (B)
	{0x0990, 0x0521},	//       = 1313
	{0x098C, 0x2737},	// Line Length (B)
	{0x0990, 0x08EC},	//       = 2284

	{0x098C, 0x2739},	// Crop_X0 (A)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x273B},	// Crop_X1 (A)
	{0x0990, 0x031F},	//       = 799
	{0x098C, 0x273D},	// Crop_Y0 (A)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x273F},	// Crop_Y1 (A)
	{0x0990, 0x0257},	//       = 599
	{0x098C, 0x2747},	// Crop_X0 (B)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x2749},	// Crop_X1 (B)
	{0x0990, 0x063F},	//       = 1599
	{0x098C, 0x274B},	// Crop_Y0 (B)
	{0x0990, 0x0000},	//       = 0
	{0x098C, 0x274D},	// Crop_Y1 (B)
	{0x0990, 0x04AF},	//       = 1199

	{0x098C, 0x222D},	// R9 Step
	{0x0990, 0x00D4},	//       = 212
	{0x098C, 0xA408},	// search_f1_50
	{0x0990, 0x0034},	//       = 52
	{0x098C, 0xA409},	// search_f2_50
	{0x0990, 0x0036},	//       = 54
	{0x098C, 0xA40A},	// search_f1_60
	{0x0990, 0x003E},	//       = 62
	{0x098C, 0xA40B},	// search_f2_60
	{0x0990, 0x0040},	//       = 64
	{0x098C, 0x2411},	// R9_Step_60 (A)
	{0x0990, 0x00D4},	//       = 212
	{0x098C, 0x2413},	// R9_Step_50 (A)
	{0x0990, 0x00FF},	//       = 255

	{SENSOR_TABLE_END, 0x0000}
}; // End of mode_800x600[]

// ===============================================================================================

static struct sensor_reg yuv_sensor_init[] = {
	// reset the sensor
	{0x001A, 0x0001},
	{SENSOR_WAIT_MS, 1},
	{0x001A, 0x0000},
	{SENSOR_WAIT_OOR, 0},

	// program the on-chip PLL
	{0x0014, 0x21F9},	// PLL Control: BYPASS PLL = 8697

	// mipi timing for YUV422 (clk_txfifo_wr = 85/42.5Mhz; clk_txfifo_rd = 63.75Mhz)
	{0x0010, 0x0115},	// PLL Dividers = 277
	{0x0012, 0x00F5},	// wcd = 8

	{0x0014, 0x2545},	// PLL Control: TEST_BYPASS on = 9541
	{0x0014, 0x2547},	// PLL Control: PLL_ENABLE on = 9543
	{0x0014, 0x2447},	// PLL Control: SEL_LOCK_DET on

	{SENSOR_WAIT_MS, 10},	// Delay 10ms to allow PLL to lock
	{0x0014, 0x2047},	// PLL Control: PLL_BYPASS off
	{0x0014, 0x2046},	// PLL Control: TEST_BYPASS off

	// select output interface
	{0x001A, 0x0050},	// BITFIELD=0x001A, 0x0200, 0 // disable Parallel
	{0x001A, 0x0058},	// BITFIELD=0x001A, 0x0008, 1 // MIPI

	// enable powerup stop
	{0x0018, 0x402D},	// LOAD=MCU Powerup Stop Enable
	{SENSOR_WAIT_MS, 10},

	{0x0018, 0x402C},	// LOAD=GO // release MCU from standby
	{SENSOR_WAIT_STDBY_EXIT, 0},

	// Errata for Silicon Rev
	{0x098C, 0xAB36},	// MCU_ADDRESS [HG_CLUSTERDC_TH]
	{0x0990, 0x0014},	// MCU_DATA_0
	{0x098C, 0x2B66},	// MCU_ADDRESS [HG_CLUSTER_DC_BM]
	{0x0990, 0x2AF8},	// MCU_DATA_0
	{0x098C, 0xAB20},	// MCU_ADDRESS [HG_LL_SAT1]
	{0x0990, 0x0080},	// MCU_DATA_0
	{0x098C, 0xAB24},	// MCU_ADDRESS [HG_LL_SAT2]
	{0x0990, 0x0000},	// MCU_DATA_0
	{0x098C, 0xAB21},	// MCU_ADDRESS [HG_LL_INTERPTHRESH1]
	{0x0990, 0x000A},	// MCU_DATA_0
	{0x098C, 0xAB25},	// MCU_ADDRESS [HG_LL_INTERPTHRESH2]
	{0x0990, 0x002A},	// MCU_DATA_0
	{0x098C, 0xAB22},	// MCU_ADDRESS [HG_LL_APCORR1]
	{0x0990, 0x0007},	// MCU_DATA_0
	{0x098C, 0xAB26},	// MCU_ADDRESS [HG_LL_APCORR2]
	{0x0990, 0x0001},	// MCU_DATA_0
	{0x098C, 0xAB23},	// MCU_ADDRESS [HG_LL_APTHRESH1]
	{0x0990, 0x0004},	// MCU_DATA_0
	{0x098C, 0xAB27},	// MCU_ADDRESS [HG_LL_APTHRESH2]
	{0x0990, 0x0009},	// MCU_DATA_0
	{0x098C, 0x2B28},	// MCU_ADDRESS [HG_LL_BRIGHTNESSSTART]
	{0x0990, 0x0BB8},	// MCU_DATA_0
	{0x098C, 0x2B2A},	// MCU_ADDRESS [HG_LL_BRIGHTNESSSTOP]
	{0x0990, 0x2968},	// MCU_DATA_0
	{0x098C, 0xAB2C},	// MCU_ADDRESS [HG_NR_START_R]
	{0x0990, 0x00FF},	// MCU_DATA_0
	{0x098C, 0xAB30},	// MCU_ADDRESS [HG_NR_STOP_R]
	{0x0990, 0x00FF},	// MCU_DATA_0
	{0x098C, 0xAB2D},	// MCU_ADDRESS [HG_NR_START_G]
	{0x0990, 0x00FF},	// MCU_DATA_0
	{0x098C, 0xAB31},	// MCU_ADDRESS [HG_NR_STOP_G]
	{0x0990, 0x00FF},	// MCU_DATA_0
	{0x098C, 0xAB2E},	// MCU_ADDRESS [HG_NR_START_B]
	{0x0990, 0x00FF},	// MCU_DATA_0
	{0x098C, 0xAB32},	// MCU_ADDRESS [HG_NR_STOP_B]
	{0x0990, 0x00FF},	// MCU_DATA_0
	{0x098C, 0xAB2F},	// MCU_ADDRESS [HG_NR_START_OL]
	{0x0990, 0x000A},	// MCU_DATA_0
	{0x098C, 0xAB33},	// MCU_ADDRESS [HG_NR_STOP_OL]
	{0x0990, 0x0006},	// MCU_DATA_0
	{0x098C, 0xAB34},	// MCU_ADDRESS [HG_NR_GAINSTART]
	{0x0990, 0x0020},	// MCU_DATA_0
	{0x098C, 0xAB35},	// MCU_ADDRESS [HG_NR_GAINSTOP]
	{0x0990, 0x0091},	// MCU_DATA_0
	{0x098C, 0xA765},	// MCU_ADDRESS [MODE_COMMONMODESETTINGS_FILTER_MODE]
	{0x0990, 0x0006},	// MCU_DATA_0
	
	{0x098C, 0x2719},	// sensor_fine_correction (A)
	{0x0990, 0x005A},	//       = 90
	{0x098C, 0x271B},	// sensor_fine_IT_min (A)
	{0x0990, 0x01BE},	//       = 446
	{0x098C, 0x271D},	// sensor_fine_IT_max_margin (A)
	{0x0990, 0x0131},	//       = 305
	{0x098C, 0x271F},	// Frame Lines (A)
	{0x0990, 0x02C3},	//       = 707
	{0x098C, 0x2721},	// Line Length (A)
	{0x0990, 0x07BC},	//       = 1980

	{0x098C, 0x272F},	// sensor_fine_correction (B)
	{0x0990, 0x003A},	//       = 58
	{0x098C, 0x2731},	// sensor_fine_IT_min (B)
	{0x0990, 0x00F6},	//       = 246
	{0x098C, 0x2733},	// sensor_fine_IT_max_margin (B)
	{0x0990, 0x008B},	//       = 139
	{0x098C, 0x2735},	// Frame Lines (B)
	{0x0990, 0x0521},	//       = 1313
	{0x098C, 0x2737},	// Line Length (B)
	{0x0990, 0x08EC},	//       = 2284

	{0x098C, 0x222D},	// R9 Step
	{0x0990, 0x00D4},	//       = 212
	{0x098C, 0xA408},	// search_f1_50
	{0x0990, 0x0033},	//       = 51
	{0x098C, 0xA409},	// search_f2_50
	{0x0990, 0x0036},	//       = 54
	{0x098C, 0xA40A},// search_f1_60
	{0x0990, 0x003D},	//       = 61
	{0x098C, 0xA40B},// search_f2_60
	{0x0990, 0x0040},	//       = 64
	{0x098C, 0x2411},	// R9_Step_60 (A)
	{0x0990, 0x00D4},	//       = 212
	{0x098C, 0x2413},	// R9_Step_50 (A)
	{0x0990, 0x00FF},	//       = 255
	{0x098C, 0x2415},	// R9_Step_60 (B)
	{0x0990, 0x00A2},	//       = 162
	{0x098C, 0x2417},	// R9_Step_50 (B)
	{0x0990, 0x00C2},	//       = 194

	// Output format
	{0x098C, 0x2755},	// MCU_ADDRESS [MODE_OUTPUT_FORMAT_A]
	{0x0990, 0x0200},	// default value 0x0000
	{0x098C, 0x2757},	// MCU_ADDRESS [MODE_OUTPUT_FORMAT_B]
	{0x0990, 0x0200},	// default value 0x0000

	{0x098C, 0xA404},	// FD Mode
	{0x0990, 0x0010},	//       = 16
	{0x098C, 0xA40D},	// Stat_min
	{0x0990, 0x0002},	//       = 2
	{0x098C, 0xA40E},	// Stat_max
	{0x0990, 0x0003},	//       = 3
	{0x098C, 0xA410},	// Min_amplitude
	{0x0990, 0x000A},	//       = 10

	// Refresh sequencer mode
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0006},	// MCU_DATA_0
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005},	// MCU_DATA_0

	// Load lens correction
	// LSC updated
	// Lens register settings for A-2031SOC (MT9D115) REV1
	{0x364E, 0x0750},	// P_GR_P0Q0
	{0x3650, 0x014D},	// P_GR_P0Q1
	{0x3652, 0x6D92},	// P_GR_P0Q2
	{0x3654, 0xF6CF},	// P_GR_P0Q3
	{0x3656, 0xE153},	// P_GR_P0Q4
	{0x3658, 0x00D0},	// P_RD_P0Q0
	{0x365A, 0x6B0C},	// P_RD_P0Q1
	{0x365C, 0x0213},	// P_RD_P0Q2
	{0x365E, 0xABED},	// P_RD_P0Q3
	{0x3660, 0xF293},	// P_RD_P0Q4
	{0x3662, 0x0230},	// P_BL_P0Q0
	{0x3664, 0x264C},	// P_BL_P0Q1
	{0x3666, 0x37D2},	// P_BL_P0Q2
	{0x3668, 0x580B},	// P_BL_P0Q3
	{0x366A, 0x88F3},	// P_BL_P0Q4
	{0x366C, 0x0190},	// P_GB_P0Q0
	{0x366E, 0x31EC},	// P_GB_P0Q1
	{0x3670, 0x6D12},	// P_GB_P0Q2
	{0x3672, 0xDBAE},	// P_GB_P0Q3
	{0x3674, 0xF2F3},	// P_GB_P0Q4
	{0x3676, 0x6E06},	// P_GR_P1Q0
	{0x3678, 0xE36E},	// P_GR_P1Q1
	{0x367A, 0x0EE8},	// P_GR_P1Q2
	{0x367C, 0x2B10},	// P_GR_P1Q3
	{0x367E, 0x08F2},	// P_GR_P1Q4
	{0x3680, 0x4A2C},	// P_RD_P1Q0
	{0x3682, 0x8D2E},	// P_RD_P1Q1
	{0x3684, 0x7D11},	// P_RD_P1Q2
	{0x3686, 0x42F2},	// P_RD_P1Q3
	{0x3688, 0xFDD4},	// P_RD_P1Q4
	{0x368A, 0xB04D},	// P_BL_P1Q0
	{0x368C, 0xCC2C},	// P_BL_P1Q1
	{0x368E, 0xE06F},	// P_BL_P1Q2
	{0x3690, 0xB38F},	// P_BL_P1Q3
	{0x3692, 0x3533},	// P_BL_P1Q4
	{0x3694, 0xFFAD},	// P_GB_P1Q0
	{0x3696, 0x088F},	// P_GB_P1Q1
	{0x3698, 0x8E90},	// P_GB_P1Q2
	{0x369A, 0xB9D1},	// P_GB_P1Q3
	{0x369C, 0x3673},	// P_GB_P1Q4
	{0x369E, 0x2773},	// P_GR_P2Q0
	{0x36A0, 0xA351},	// P_GR_P2Q1
	{0x36A2, 0xA1B6},	// P_GR_P2Q2
	{0x36A4, 0x4BF4},	// P_GR_P2Q3
	{0x36A6, 0x6438},	// P_GR_P2Q4
	{0x36A8, 0x2D53},	// P_RD_P2Q0
	{0x36AA, 0x9DEF},	// P_RD_P2Q1
	{0x36AC, 0xA916},	// P_RD_P2Q2
	{0x36AE, 0x10B3},	// P_RD_P2Q3
	{0x36B0, 0x6818},	// P_RD_P2Q4
	{0x36B2, 0x0EF3},	// P_BL_P2Q0
	{0x36B4, 0x3BB0},	// P_BL_P2Q1
	{0x36B6, 0xFDF5},	// P_BL_P2Q2
	{0x36B8, 0xE894},	// P_BL_P2Q3
	{0x36BA, 0x57F8},	// P_BL_P2Q4
	{0x36BC, 0x2A13},	// P_GB_P2Q0
	{0x36BE, 0x5931},	// P_GB_P2Q1
	{0x36C0, 0x9D76},	// P_GB_P2Q2
	{0x36C2, 0xF333},	// P_GB_P2Q3
	{0x36C4, 0x0059},	// P_GB_P2Q4
	{0x36C6, 0xCE2E},	// P_GR_P3Q0
	{0x36C8, 0x726E},	// P_GR_P3Q1
	{0x36CA, 0x7DD3},	// P_GR_P3Q2
	{0x36CC, 0x2D6E},	// P_GR_P3Q3
	{0x36CE, 0xAA15},	// P_GR_P3Q4
	{0x36D0, 0x2370},	// P_RD_P3Q0
	{0x36D2, 0x1472},	// P_RD_P3Q1
	{0x36D4, 0x8196},	// P_RD_P3Q2
	{0x36D6, 0x88D5},	// P_RD_P3Q3
	{0x36D8, 0x1079},	// P_RD_P3Q4
	{0x36DA, 0x7E0D},	// P_BL_P3Q0
	{0x36DC, 0xB24F},	// P_BL_P3Q1
	{0x36DE, 0x6A93},	// P_BL_P3Q2
	{0x36E0, 0x4875},	// P_BL_P3Q3
	{0x36E2, 0x8478},	// P_BL_P3Q4
	{0x36E4, 0x2ECF},	// P_GB_P3Q0
	{0x36E6, 0xCD72},	// P_GB_P3Q1
	{0x36E8, 0x1AB4},	// P_GB_P3Q2
	{0x36EA, 0x30F6},	// P_GB_P3Q3
	{0x36EC, 0xA696},	// P_GB_P3Q4
	{0x36EE, 0x98D5},	// P_GR_P4Q0
	{0x36F0, 0x12F5},	// P_GR_P4Q1
	{0x36F2, 0x65B8},	// P_GR_P4Q2
	{0x36F4, 0xF557},	// P_GR_P4Q3
	{0x36F6, 0xC830},	// P_GR_P4Q4
	{0x36F8, 0x8835},	// P_RD_P4Q0
	{0x36FA, 0x6B4F},	// P_RD_P4Q1
	{0x36FC, 0x5398},	// P_RD_P4Q2
	{0x36FE, 0x3716},	// P_RD_P4Q3
	{0x3700, 0xD679},	// P_RD_P4Q4
	{0x3702, 0x8195},	// P_BL_P4Q0
	{0x3704, 0xA553},	// P_BL_P4Q1
	{0x3706, 0x60B8},	// P_BL_P4Q2
	{0x3708, 0x39B8},	// P_BL_P4Q3
	{0x370A, 0xB25A},	// P_BL_P4Q4
	{0x370C, 0x9DB5},	// P_GB_P4Q0
	{0x370E, 0xD275},	// P_GB_P4Q1
	{0x3710, 0x3298},	// P_GB_P4Q2
	{0x3712, 0x2A78},	// P_GB_P4Q3
	{0x3714, 0xA6F9},	// P_GB_P4Q4
	{0x3644, 0x0320},	// POLY_ORIGIN_C
	{0x3642, 0x0264},	// POLY_ORIGIN_R
	{0x3210, 0x01B8}, 	
	
	// Kernel setting
	{0x098C, 0xAB36},	// MCU_ADDRESS [HG_CLUSTERDC_TH]
	{0x0990, 0x0014},	// MCU_DATA_0
	{0x098C, 0x2B66},	// MCU_ADDRESS [HG_CLUSTER_DC_BM]
	{0x0990, 0x2AF8},	// MCU_DATA_0
	{0x098C, 0xAB20},	// MCU_ADDRESS [HG_LL_SAT1]
	{0x0990, 0x0080},	// MCU_DATA_0
	{0x098C, 0xAB24},	// MCU_ADDRESS [HG_LL_SAT2]
	{0x0990, 0x0000},	// MCU_DATA_0
	{0x098C, 0xAB21},	// MCU_ADDRESS [HG_LL_INTERPTHRESH1]
	{0x0990, 0x000A},	// MCU_DATA_0
	{0x098C, 0xAB25},	// MCU_ADDRESS [HG_LL_INTERPTHRESH2]
	{0x0990, 0x002A},	// MCU_DATA_0
	{0x098C, 0xAB22},	// MCU_ADDRESS [HG_LL_APCORR1]
	{0x0990, 0x0007},	// MCU_DATA_0
	{0x098C, 0xAB26},	// MCU_ADDRESS [HG_LL_APCORR2]
	{0x0990, 0x0001},	// MCU_DATA_0
	{0x098C, 0xAB23},	// MCU_ADDRESS [HG_LL_APTHRESH1]
	{0x0990, 0x0004},	// MCU_DATA_0
	{0x098C, 0xAB27},	// MCU_ADDRESS [HG_LL_APTHRESH2]
	{0x0990, 0x0009},	// MCU_DATA_0
	{0x098C, 0x2B28},	// MCU_ADDRESS [HG_LL_BRIGHTNESSSTART]
	{0x0990, 0x0BB8},	// MCU_DATA_0
	{0x098C, 0x2B2A},	// MCU_ADDRESS [HG_LL_BRIGHTNESSSTOP]
	{0x0990, 0x2968},	// MCU_DATA_0
	{0x098C, 0xAB2C},	// MCU_ADDRESS [HG_NR_START_R]
	{0x0990, 0x00FF},	// MCU_DATA_0
	{0x098C, 0xAB30},	// MCU_ADDRESS [HG_NR_STOP_R]
	{0x0990, 0x00FF},	// MCU_DATA_0
	{0x098C, 0xAB2D},	// MCU_ADDRESS [HG_NR_START_G]
	{0x0990, 0x00FF},	// MCU_DATA_0
	{0x098C, 0xAB31},	// MCU_ADDRESS [HG_NR_STOP_G]
	{0x0990, 0x00FF},	// MCU_DATA_0
	{0x098C, 0xAB2E},	// MCU_ADDRESS [HG_NR_START_B]
	{0x0990, 0x00FF},	// MCU_DATA_0
	{0x098C, 0xAB32},	// MCU_ADDRESS [HG_NR_STOP_B]
	{0x0990, 0x00FF},	// MCU_DATA_0
	{0x098C, 0xAB2F},	// MCU_ADDRESS [HG_NR_START_OL]
	{0x0990, 0x000A},	// MCU_DATA_0
	{0x098C, 0xAB33},	// MCU_ADDRESS [HG_NR_STOP_OL]
	{0x0990, 0x0006},	// MCU_DATA_0
	{0x098C, 0xAB34},	// MCU_ADDRESS [HG_NR_GAINSTART]
	{0x0990, 0x0020},	// MCU_DATA_0
	{0x098C, 0xAB35},	// MCU_ADDRESS [HG_NR_GAINSTOP]
	{0x0990, 0x0091},	// MCU_DATA_0
	{0x098C, 0xA765},	// MCU_ADDRESS [MODE_COMMONMODESETTINGS_FILTER_MODE]
	{0x0990, 0x0006},	// MCU_DATA_0

	// CCM
	{0x098C, 0x2306},	// MCU_ADDRESS [AWB_CCM_L_0]
	{0x0990, 0x01D6},	// MCU_DATA_0
	{0x098C, 0x2308},	// MCU_ADDRESS [AWB_CCM_L_1]
	{0x0990, 0xFF89},	// MCU_DATA_0
	{0x098C, 0x230A},	// MCU_ADDRESS [AWB_CCM_L_2]
	{0x0990, 0xFFA1},	// MCU_DATA_0
	{0x098C, 0x230C},	// MCU_ADDRESS [AWB_CCM_L_3]
	{0x0990, 0xFF73},	// MCU_DATA_0
	{0x098C, 0x230E},	// MCU_ADDRESS [AWB_CCM_L_4]
	{0x0990, 0x019C},	// MCU_DATA_0
	{0x098C, 0x2310},	// MCU_ADDRESS [AWB_CCM_L_5]
	{0x0990, 0xFFF1},	// MCU_DATA_0
	{0x098C, 0x2312},	// MCU_ADDRESS [AWB_CCM_L_6]
	{0x0990, 0xFFB0},	// MCU_DATA_0
	{0x098C, 0x2314},	// MCU_ADDRESS [AWB_CCM_L_7]
	{0x0990, 0xFF2D},	// MCU_DATA_0
	{0x098C, 0x2316},	// MCU_ADDRESS [AWB_CCM_L_8]
	{0x0990, 0x0223},	// MCU_DATA_0
	{0x098C, 0x2318},	// MCU_ADDRESS [AWB_CCM_L_9]
	{0x0990, 0x0024},	// MCU_DATA_0
	{0x098C, 0x231A},	// MCU_ADDRESS [AWB_CCM_L_10]
	{0x0990, 0x0038},	// MCU_DATA_0
	{0x098C, 0x231C},	// MCU_ADDRESS [AWB_CCM_RL_0]
	{0x0990, 0xFFCD}, // MCU_DATA_0
	{0x098C, 0x231E},	// MCU_ADDRESS [AWB_CCM_RL_1]
	{0x0990, 0x0023},	// MCU_DATA_0
	{0x098C, 0x2320},	// MCU_ADDRESS [AWB_CCM_RL_2]
	{0x0990, 0x0010},	// MCU_DATA_0
	{0x098C, 0x2322},	// MCU_ADDRESS [AWB_CCM_RL_3]
	{0x0990, 0x0026},	// MCU_DATA_0
	{0x098C, 0x2324},	// MCU_ADDRESS [AWB_CCM_RL_4]
	{0x0990, 0xFFE9},	// MCU_DATA_0
	{0x098C, 0x2326},	// MCU_ADDRESS [AWB_CCM_RL_5]
	{0x0990, 0xFFF1},	// MCU_DATA_0
	{0x098C, 0x2328},	// MCU_ADDRESS [AWB_CCM_RL_6]
	{0x0990, 0x003A},	// MCU_DATA_0
	{0x098C, 0x232A},	// MCU_ADDRESS [AWB_CCM_RL_7]
	{0x0990, 0x005D},	// MCU_DATA_0
	{0x098C, 0x232C},	// MCU_ADDRESS [AWB_CCM_RL_8]
	{0x0990, 0xFF69},	// MCU_DATA_0
	{0x098C, 0x232E},	// MCU_ADDRESS [AWB_CCM_RL_9]
	{0x0990, 0x0004},	// MCU_DATA_0
	{0x098C, 0x2330},	// MCU_ADDRESS [AWB_CCM_RL_10]
	{0x0990, 0xFFF4},	// MCU_DATA_0

	// AE setting
	{0x098C, 0xA117},	// MCU_ADDRESS [SEQ_PREVIEW_0_AE]
	{0x0990, 0x0002},	// MCU_DATA_0
	{0x098C, 0xA11D},	// MCU_ADDRESS [SEQ_PREVIEW_1_AE]
	{0x0990, 0x0002},	// MCU_DATA_0
	{0x098C, 0xA129},	// MCU_ADDRESS [SEQ_PREVIEW_3_AE]
	{0x0990, 0x0002},	// MCU_DATA_0
	{0x098C, 0xA216},	// MCU_ADDRESS [AE_MAXGAIN23]
	{0x0990, 0x0080},	// MCU_DATA_0
	{0x098C, 0xA20E},	// MCU_ADDRESS [AE_MAX_VIRTGAIN]
	{0x0990, 0x0080},	// MCU_DATA_0
	{0x098C, 0x2212},	// MCU_ADDRESS [AE_MAX_DGAIN_AE1]
	{0x0990, 0x00A4},	// MCU_DATA_0
	
	// GAMMA Settings
	{0x098C, 0xAB37},	// MCU_ADDRESS [HG_GAMMA_MORPH_CTRL]
	{0x0990, 0x0003},	// MCU_DATA_0
	{0x098C, 0x2B38},	// MCU_ADDRESS [HG_GAMMASTARTMORPH]
	{0x0990, 0x2968},	// MCU_DATA_0
	{0x098C, 0x2B3A},	// MCU_ADDRESS [HG_GAMMASTOPMORPH]
	{0x0990, 0x2D50},	// MCU_DATA_0
	{0x098C, 0x2B62},	// MCU_ADDRESS [HG_FTB_START_BM]
	{0x0990, 0xFFFE},	// MCU_DATA_0
	{0x098C, 0x2B64},	// MCU_ADDRESS [HG_FTB_STOP_BM]
	{0x0990, 0xFFFF},	// MCU_DATA_0

	{0x098C, 0xAB3C},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_0]
	{0x0990, 0x0000},	// MCU_DATA_0
	{0x098C, 0xAB3D},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_1]
	{0x0990, 0x0005},	// MCU_DATA_0
	{0x098C, 0xAB3E},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_2]
	{0x0990, 0x000F},	// MCU_DATA_0
	{0x098C, 0xAB3F},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_3]
	{0x0990, 0x0027},	// MCU_DATA_0
	{0x098C, 0xAB40},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_4]
	{0x0990, 0x0047},	// MCU_DATA_0
	{0x098C, 0xAB41},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_5]
	{0x0990, 0x005D},	// MCU_DATA_0
	{0x098C, 0xAB42},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_6]
	{0x0990, 0x0070},	// MCU_DATA_0
	{0x098C, 0xAB43},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_7]
	{0x0990, 0x0081},	// MCU_DATA_0
	{0x098C, 0xAB44},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_8]
	{0x0990, 0x0090},	// MCU_DATA_0
	{0x098C, 0xAB45},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_9]
	{0x0990, 0x009E},	// MCU_DATA_0
	{0x098C, 0xAB46},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_10]
	{0x0990, 0x00AB},	// MCU_DATA_0
	{0x098C, 0xAB47},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_11]
	{0x0990, 0x00B7},	// MCU_DATA_0
	{0x098C, 0xAB48},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_12]
	{0x0990, 0x00C3},	// MCU_DATA_0
	{0x098C, 0xAB49},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_13]
	{0x0990, 0x00CE},	// MCU_DATA_0
	{0x098C, 0xAB4A},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_14]
	{0x0990, 0x00D8},	// MCU_DATA_0
	{0x098C, 0xAB4B},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_15]
	{0x0990, 0x00E3},	// MCU_DATA_0
	{0x098C, 0xAB4C},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_16]
	{0x0990, 0x00EC},	// MCU_DATA_0
	{0x098C, 0xAB4D},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_17]
	{0x0990, 0x00F6},	// MCU_DATA_0
	{0x098C, 0xAB4E},	// MCU_ADDRESS [HG_GAMMA_TABLE_A_18]
	{0x0990, 0x00FF},	// MCU_DATA_0

	{0x098C, 0xAB4F},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_0]
	{0x0990, 0x0000},	// MCU_DATA_0
	{0x098C, 0xAB50},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_1]
	{0x0990, 0x0005},	// MCU_DATA_0
	{0x098C, 0xAB51},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_2]
	{0x0990, 0x000F},	// MCU_DATA_0
	{0x098C, 0xAB52},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_3]
	{0x0990, 0x0027},	// MCU_DATA_0
	{0x098C, 0xAB53},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_4]
	{0x0990, 0x0047},	// MCU_DATA_0
	{0x098C, 0xAB54},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_5]
	{0x0990, 0x005D},	// MCU_DATA_0
	{0x098C, 0xAB55},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_6]
	{0x0990, 0x0070},	// MCU_DATA_0
	{0x098C, 0xAB56},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_7]
	{0x0990, 0x0081},	// MCU_DATA_0
	{0x098C, 0xAB57},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_8]
	{0x0990, 0x0090},	// MCU_DATA_0
	{0x098C, 0xAB58},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_9]
	{0x0990, 0x009E},	// MCU_DATA_0
	{0x098C, 0xAB59},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_10]
	{0x0990, 0x00AB},	// MCU_DATA_0
	{0x098C, 0xAB5A},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_11]
	{0x0990, 0x00B7},	// MCU_DATA_0
	{0x098C, 0xAB5B},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_12]
	{0x0990, 0x00C3},	// MCU_DATA_0
	{0x098C, 0xAB5C},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_13]
	{0x0990, 0x00CE},	// MCU_DATA_0
	{0x098C, 0xAB5D},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_14]
	{0x0990, 0x00D8},	// MCU_DATA_0
	{0x098C, 0xAB5E},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_15]
	{0x0990, 0x00E3},	// MCU_DATA_0
	{0x098C, 0xAB5F},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_16]
	{0x0990, 0x00EC},	// MCU_DATA_0
	{0x098C, 0xAB60},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_17]
	{0x0990, 0x00F6},	// MCU_DATA_0
	{0x098C, 0xAB61},	// MCU_ADDRESS [HG_GAMMA_TABLE_B_18]
	{0x0990, 0x00FF},	// MCU_DATA_0
	
	// SOC2031 patch
	{0x098C, 0x0415},
	{0x0990, 0xF601},
	{0x0992, 0x42C1},
	{0x0994, 0x0326},
	{0x0996, 0x11F6},
	{0x0998, 0x0143},
	{0x099A, 0xC104},
	{0x099C, 0x260A},
	{0x099E, 0xCC04},

	{0x098C, 0x0425},
	{0x0990, 0x33BD},
	{0x0992, 0xA362},
	{0x0994, 0xBD04},
	{0x0996, 0x3339},
	{0x0998, 0xC6FF},
	{0x099A, 0xF701},
	{0x099C, 0x6439},
	{0x099E, 0xFE01},

	{0x098C, 0x0435},
	{0x0990, 0x6918},
	{0x0992, 0xCE03},
	{0x0994, 0x25CC},
	{0x0996, 0x0013},
	{0x0998, 0xBDC2},
	{0x099A, 0xB8CC},
	{0x099C, 0x0489},
	{0x099E, 0xFD03},

	{0x098C, 0x0445},
	{0x0990, 0x27CC},
	{0x0992, 0x0325},
	{0x0994, 0xFD01},
	{0x0996, 0x69FE},
	{0x0998, 0x02BD},
	{0x099A, 0x18CE},
	{0x099C, 0x0339},
	{0x099E, 0xCC00},

	{0x098C, 0x0455},
	{0x0990, 0x11BD},
	{0x0992, 0xC2B8},
	{0x0994, 0xCC04},
	{0x0996, 0xC8FD},
	{0x0998, 0x0347},
	{0x099A, 0xCC03},
	{0x099C, 0x39FD},
	{0x099E, 0x02BD},

	{0x098C, 0x0465},
	{0x0990, 0xDE00},
	{0x0992, 0x18CE},
	{0x0994, 0x00C2},
	{0x0996, 0xCC00},
	{0x0998, 0x37BD},
	{0x099A, 0xC2B8},
	{0x099C, 0xCC04},
	{0x099E, 0xEFDD},

	{0x098C, 0x0475},
	{0x0990, 0xE6CC},
	{0x0992, 0x00C2},
	{0x0994, 0xDD00},
	{0x0996, 0xC601},
	{0x0998, 0xF701},
	{0x099A, 0x64C6},
	{0x099C, 0x03F7},
	{0x099E, 0x0165},

	{0x098C, 0x0485},
	{0x0990, 0x7F01},
	{0x0992, 0x6639},
	{0x0994, 0x3C3C},
	{0x0996, 0x3C34},
	{0x0998, 0xCC32},
	{0x099A, 0x3EBD},
	{0x099C, 0xA558},
	{0x099E, 0x30ED},

	{0x098C, 0x0495},
	{0x0990, 0x04BD},
	{0x0992, 0xB2D7},
	{0x0994, 0x30E7},
	{0x0996, 0x06CC},
	{0x0998, 0x323E},
	{0x099A, 0xED00},
	{0x099C, 0xEC04},
	{0x099E, 0xBDA5},

	{0x098C, 0x04A5},
	{0x0990, 0x44CC},
	{0x0992, 0x3244},
	{0x0994, 0xBDA5},
	{0x0996, 0x585F},
	{0x0998, 0x30ED},
	{0x099A, 0x02CC},
	{0x099C, 0x3244},
	{0x099E, 0xED00},

	{0x098C, 0x04B5},
	{0x0990, 0xF601},
	{0x0992, 0xD54F},
	{0x0994, 0xEA03},
	{0x0996, 0xAA02},
	{0x0998, 0xBDA5},
	{0x099A, 0x4430},
	{0x099C, 0xE606},
	{0x099E, 0x3838},

	{0x098C, 0x04C5},
	{0x0990, 0x3831},
	{0x0992, 0x39BD},
	{0x0994, 0xD661},
	{0x0996, 0xF602},
	{0x0998, 0xF4C1},
	{0x099A, 0x0126},
	{0x099C, 0x0BFE},
	{0x099E, 0x02BD},

	{0x098C, 0x04D5},
	{0x0990, 0xEE10},
	{0x0992, 0xFC02},
	{0x0994, 0xF5AD},
	{0x0996, 0x0039},
	{0x0998, 0xF602},
	{0x099A, 0xF4C1},
	{0x099C, 0x0226},
	{0x099E, 0x0AFE},

	{0x098C, 0x04E5},
	{0x0990, 0x02BD},
	{0x0992, 0xEE10},
	{0x0994, 0xFC02},
	{0x0996, 0xF7AD},
	{0x0998, 0x0039},
	{0x099A, 0x3CBD},
	{0x099C, 0xB059},
	{0x099E, 0xCC00},

	{0x098C, 0x04F5},
	{0x0990, 0x28BD},
	{0x0992, 0xA558},
	{0x0994, 0x8300},
	{0x0996, 0x0027},
	{0x0998, 0x0BCC},
	{0x099A, 0x0026},
	{0x099C, 0x30ED},
	{0x099E, 0x00C6},

	{0x098C, 0x0505},
	{0x0990, 0x03BD},
	{0x0992, 0xA544},
	{0x0994, 0x3839},

	// Execute the patch
	{0x098C, 0x2006},	// MCU_ADDRESS [MON_ARG1]
	{0x0990, 0x0415},	// MCU_DATA_0
	{0x098C, 0xA005},	// MCU_ADDRESS [MON_CMD]
	{0x0990, 0x0001},	// MCU_DATA_0
	{SENSOR_WAIT_PATCH, 0},	// POLL MON_PATCH_ID_0 => 0x01

	// LOAD=Continue
	// clear powerup stop bit
	// BITFIELD=0x0018, 0x0004, 0
	{0x0018, 0x0028},	// wait for sequencer to enter preview state
	{SENSOR_WAIT_SEQUENCER, 3},
	
	// Refresh sequencer mode
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0006},	// MCU_DATA_0
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005},	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg ColorEffect_None[] = {
	{0x098C, 0x2759},	// content A
	{0x0990, 0x0000},
	{0x098C, 0x275B},	// content B
	{0x0990, 0x0000},
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0006},	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg ColorEffect_Mono[] = {
	{0x098C, 0x2759},	// content A
	{0x0990, 0x0001},
	{0x098C, 0x275B},	// content B
	{0x0990, 0x0001},
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0006},	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg ColorEffect_Sepia[] = {
	{0x098C, 0x2759},	// content A
	{0x0990, 0x0002},
	{0x098C, 0x275B},	// content B
	{0x0990, 0x0002},
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0006},	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg ColorEffect_Negative[] = {
	{0x098C, 0x2759},	// content A
	{0x0990, 0x0003},
	{0x098C, 0x275B},	// content B
	{0x0990, 0x0003},
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0006},	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg ColorEffect_Solarize[] = {
	{0x098C, 0x2759},	// content A
	{0x0990, 0x0004},
	{0x098C, 0x275B},	// content B
	{0x0990, 0x0004},
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0006},	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

// Sensor ISP Not Support this function
static struct sensor_reg ColorEffect_Posterize[] = {
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg Whitebalance_Auto[] = {
	{0x098C, 0xA115},	// MCU_ADDRESS [SEQ_CAP_MODE]
	{0x0990, 0x0000},	// MCU_DATA_0
	{0x098C, 0xA11F},	// MCU_ADDRESS [SEQ_PREVIEW_1_AWB]
	{0x0990, 0x0000},	// MCU_DATA_0
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005},	// MCU_DATA_0
	{SENSOR_WAIT_MS, 100},
	{0x098C, 0xA102},	// MCU_ADDRESS
	{0x0990, 0x000F},	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg Whitebalance_Incandescent[] = {
	{0x098C, 0xA115},	// MCU_ADDRESS [SEQ_CAP_MODE]
	{0x0990, 0x0000},	// MCU_DATA_0
	{0x098C, 0xA11F},	// MCU_ADDRESS [SEQ_PREVIEW_1_AWB]
	{0x0990, 0x0000},	// MCU_DATA_0
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005},	// MCU_DATA_0
	{SENSOR_WAIT_MS, 100},
	{0x098C, 0xA102},	// MCU_ADDRESS [SEQ_MODE]
	{0x0990, 0x000B},	// MCU_DATA_0
	{0x098C, 0xA34E},	// MCU_ADDRESS [AWB_GAIN_R]
	{0x0990, 0x007C},	// MCU_DATA_0
	{0x098C, 0xA34F},	// MCU_ADDRESS [AWB_GAIN_G]
	{0x0990, 0x0080},	// MCU_DATA_0
	{0x098C, 0xA350},	// MCU_ADDRESS [AWB_GAIN_B]
	{0x0990, 0x009F},	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg Whitebalance_Daylight[] = {
	{0x098C, 0xA115},	// MCU_ADDRESS [SEQ_CAP_MODE]
	{0x0990, 0x0000},	// MCU_DATA_0
	{0x098C, 0xA11F},	// MCU_ADDRESS [SEQ_PREVIEW_1_AWB]
	{0x0990, 0x0000},	// MCU_DATA_0
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005},	// MCU_DATA_0
	{SENSOR_WAIT_MS, 100},
	{0x098C, 0xA102},	// MCU_ADDRESS [SEQ_MODE]
	{0x0990, 0x000B},	// MCU_DATA_0
	{0x098C, 0xA34E},	// MCU_ADDRESS [AWB_GAIN_R]
	{0x0990, 0x00A3},	// MCU_DATA_0
	{0x098C, 0xA34F},	// MCU_ADDRESS [AWB_GAIN_G]
	{0x0990, 0x0080},	// MCU_DATA_0
	{0x098C, 0xA350},	// MCU_ADDRESS [AWB_GAIN_B]
	{0x0990, 0x0072},	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg Whitebalance_Fluorescent[] = {
	{0x098C, 0xA115},	// MCU_ADDRESS [SEQ_CAP_MODE]
	{0x0990, 0x0000},	// MCU_DATA_0
	{0x098C, 0xA11F},	// MCU_ADDRESS [SEQ_PREVIEW_1_AWB]
	{0x0990, 0x0000},	// MCU_DATA_0
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005},	// MCU_DATA_0
	{SENSOR_WAIT_MS, 100},
	{0x098C, 0xA102},	// MCU_ADDRESS [SEQ_MODE]
	{0x0990, 0x000B},	// MCU_DATA_0
	{0x098C, 0xA34E},	// MCU_ADDRESS [AWB_GAIN_R]
	{0x0990, 0x008D},	// MCU_DATA_0
	{0x098C, 0xA34F},	// MCU_ADDRESS [AWB_GAIN_G]
	{0x0990, 0x007C},	// MCU_DATA_0
	{0x098C, 0xA350},	// MCU_ADDRESS [AWB_GAIN_B]
	{0x0990, 0x00A1},	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg Whitebalance_CloudyDaylight[] = {
	{0x098C, 0xA115},	// MCU_ADDRESS [SEQ_CAP_MODE]
	{0x0990, 0x0000},	// MCU_DATA_0
	{0x098C, 0xA11F},	// MCU_ADDRESS [SEQ_PREVIEW_1_AWB]
	{0x0990, 0x0000},	// MCU_DATA_0
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005},	// MCU_DATA_0
	{SENSOR_WAIT_MS, 100},
	{0x098C, 0xA102},	// MCU_ADDRESS [SEQ_MODE]
	{0x0990, 0x000B},	// MCU_DATA_0
	{0x098C, 0xA34E},	// MCU_ADDRESS [AWB_GAIN_R]
	{0x0990, 0x00B3},	// MCU_DATA_0
	{0x098C, 0xA34F},	// MCU_ADDRESS [AWB_GAIN_G]
	{0x0990, 0x0080},	// MCU_DATA_0
	{0x098C, 0xA350},	// MCU_ADDRESS [AWB_GAIN_B]
	{0x0990, 0x0072},	// MCU_DATA_0
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg Exposure_2[] = {
	{0x098C, 0xA24F},
	{0x0990, 0x0096},	// 150
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg Exposure_1[] = {
	{0x098C, 0xA24F},
	{0x0990, 0x0078},	// 120
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg Exposure_0[] = {
	{0x098C, 0xA24F},
	{0x0990, 0x005C},	// 92
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg Exposure_Negative_1[] = {
	{0x098C, 0xA24F},
	{0x0990, 0x003C},	// 60
	{SENSOR_TABLE_END, 0x0000}
};

static struct sensor_reg Exposure_Negative_2[] = {
	{0x098C, 0xA24F},
	{0x0990, 0x001E},	// 30
	{SENSOR_TABLE_END, 0x0000}
};


enum {
	SENSOR_MODE_1600x1200,
	SENSOR_MODE_1280x720,
	SENSOR_MODE_800x600,
	SENSOR_MODE_INIT,
};

static struct sensor_reg *mode_table[] = {
	[SENSOR_MODE_1600x1200] = mode_1600x1200,
	[SENSOR_MODE_1280x720]  = mode_1280x720,
	[SENSOR_MODE_800x600]   = mode_800x600,
	[SENSOR_MODE_INIT]   	= yuv_sensor_init,
};


static struct mutex yuv_lock;
struct sensor_info {
	int mode;
	int open_count;
	int init_pending;
	int flash_mode;
	int flash_status;
	u16 coarse_time;
	struct i2c_client *i2c_client;
	struct mt9d115_sensor_platform_data *pdata;
};


static int sensor_read_reg(struct sensor_info *info, u16 addr, u16 *val)
{
	struct i2c_client *client = info->i2c_client;
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

static int sensor_write_reg(struct sensor_info *info, u16 addr, u16 val)
{
	struct i2c_client *client = info->i2c_client;
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
		dev_err(&client->dev,"i2c transfer failed, retrying %x %x", addr, val);
		msleep(3);
	} while (retry <= SENSOR_MAX_RETRIES);

	return err;
}

static int poll_current_state(struct sensor_info *info,u16 expected_state)
{
	struct i2c_client *client = info->i2c_client;
	u16 state = 0;
	int timeout = 150;
	dev_dbg(&client->dev,"check current seq_state for %u", expected_state);
	do {
		sensor_write_reg(info, 0x098c, 0xa104);  	// MCU_ADDRESS[SEQ_STATE]
		sensor_read_reg(info, 0x0990, &state);   // MCU_DATA_0 value
		dev_dbg(&client->dev,"MCU_DATA_0 = %u", state);
		msleep(1);
	} while (--timeout > 0 && state != expected_state);
	
	return (state != expected_state) ? -EIO : 0;
}

static int poll_patch_state(struct sensor_info *info)
{
	struct i2c_client *client = info->i2c_client;
	u16 state = 0;
	int timeout = 150;
	dev_dbg(&client->dev,"check current mon_patch_id_0");
	do {
		msleep(100);
		sensor_write_reg(info, 0x098c, 0xA024);  	// MCU_ADDRESS[MON_PATCH_ID_0]
		sensor_read_reg(info, 0x0990, &state);   // MCU_DATA_0 value
		dev_dbg(&client->dev,"MCU_DATA_0 = %u", state);
	} while (--timeout > 0 && state == 0);
	
	return (state == 0) ? -EIO : 0;
}

static int sensor_write_table(struct sensor_info *info, const struct sensor_reg table[])
{
	struct i2c_client *client = info->i2c_client;
	int err;
	const struct sensor_reg *next;
	u16 val;

	dev_dbg(&client->dev,"%s", __func__);
	for (next = table; next->addr != SENSOR_TABLE_END; next++) {
		if (next->addr == SENSOR_WAIT_MS) {
			msleep(next->val);
			continue;
		} else
		if (next->addr == SENSOR_WAIT_OOR) {
			u16 ctrl;
			int retry = 0;
			// poll & wait for sensor to go out of reset
			do {
				if (retry++ > 100) {
					dev_err(&client->dev,"mt9d115 did not go out of reset! (0x%04x)", ctrl);
					return -ENODEV;
				}
				msleep(10);
				sensor_read_reg(info, REG_MT9D115_MODEL_ID, &ctrl);
			} while (ctrl != MT9D115_MODEL_ID ); 
			
			continue;
		} else
		if (next->addr == SENSOR_WAIT_STDBY_EXIT) {
			u16 ctrl;
			int retry = 0;
			// poll and wait for standby to exit (STANDBY_BIT cleared)
			do {
				if (retry++ > 10) {
					dev_err(&client->dev,"Unable to make mt9d115 sensor exit standby (0x%04x)", ctrl);
					return -ENODEV;
				}
				msleep(100);
				sensor_read_reg(info, REG_MT9D115_STANDBY_CONTROL, &ctrl);

			} while ((ctrl & SYSCTL_STANDBY_DONE) != 0);
 
			continue;
		} else
		if (next->addr == SENSOR_WAIT_PATCH) {
			if (poll_patch_state(info))  {
				dev_err(&client->dev,"mt9d115 timed out executing patch");
				return -EIO;
			}
			continue;
		} else
		if (next->addr == SENSOR_WAIT_SEQUENCER) {
			if (poll_current_state(info, next->val))  {
				dev_err(&client->dev,"Unable to make mt9d115 sequencer enter mode %d", next->val);
				return -EIO;
			}
			continue;
		} 

		val = next->val;

		err = sensor_write_reg(info, next->addr, val);
		if (err)
			return err;
	}

	return 0;
}

static int get_sensor_current_width(struct sensor_info *info, u16 *val)
{
	int err;

	err = sensor_write_reg(info, 0x098c, REG_MT9D115_WIDTH);
	if (err)
		return err;

	err = sensor_read_reg(info, 0x0990, val);
	if (err)
		return err;

	return 0;
}

static int sensor_get_exposure_time(struct sensor_info *info, unsigned int *exposure_time_denominator)
{
	int err;
	u16 val = 0;

	err = sensor_write_reg(info, 0x098C, 0xA21B);
	if (err)
		return err;

	err = sensor_read_reg(info, 0x0990, &val);
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

static int ltc3216_turn_on_flash(struct sensor_info *info)
{
	if (info->pdata && info->pdata->flash_on)
		info->pdata->flash_on();
	return 0;
}

static int ltc3216_turn_off_flash(struct sensor_info *info)
{
	if (info->pdata && info->pdata->flash_off)
		info->pdata->flash_off();
	return 0;
}

static int sensor_set_mode(struct sensor_info *info, int xres, int yres)
{
	int sensor_table;
	int err;
	u16 val;

	dev_dbg(&info->i2c_client->dev,"xres %u yres %u", xres, yres);

	mutex_lock(&yuv_lock);
	
	// Assume we are going back to preview, so turn off the flash LED
	if (info->flash_status) {
		ltc3216_turn_off_flash(info);
		info->flash_status = 0;
	} 
	
	if (xres >= 1600 && yres >= 1200)
		sensor_table = SENSOR_MODE_1600x1200;
	else if (xres == 1280 && yres == 720)
		sensor_table = SENSOR_MODE_1280x720;
	else if (xres == 800 && yres == 600)
		sensor_table = SENSOR_MODE_800x600;
	else {
		dev_err(&info->i2c_client->dev,"invalid resolution supplied to set mode %d %d",xres, yres);
		mutex_unlock(&yuv_lock);
		return -EINVAL;
	}

	if (xres >= 1600 && yres >= 1200) {
		// Get context a coarse time
		sensor_read_reg(info, 0x3012, &(info->coarse_time));
		dev_dbg(&info->i2c_client->dev,"get context a coarse time = %d", info->coarse_time);
	}

	err = get_sensor_current_width(info, &val);

	// if no table is programming before and request set to 1600x1200, then
	// we must use 1600x1200 table to fix CTS testing issue
	if (!(val == SENSOR_1280_WIDTH_VAL || val == SENSOR_800_WIDTH_VAL || val == SENSOR_1600_WIDTH_VAL) && 
			sensor_table == SENSOR_MODE_1600x1200) {
		err = sensor_write_table(info, CTS_ZoomTest_mode_1600x1200);
		dev_dbg(&info->i2c_client->dev,"1600x1200 cts table");
	} else {
	
		// check already program the sensor mode, Aptina support Context B fast switching capture mode back to preview mode
		// we don't need to re-program the sensor mode for 640x480 table
		if (!((val == SENSOR_800_WIDTH_VAL && sensor_table == SENSOR_MODE_800x600)
			||(val == SENSOR_1280_WIDTH_VAL && sensor_table == SENSOR_MODE_1280x720))) {

			err = sensor_write_table(info, mode_table[sensor_table]);
			if (err) {
				mutex_unlock(&yuv_lock);
				return err;
			}

			if (xres >= 1600 && yres >= 1200) {
				// don't use double in the kernel
				long val = (long)(info->coarse_time);
				val = (val * 1648) / 2284;

				// Set context b coarse time
				sensor_write_reg(info, 0x3012, (u16)val);
				sensor_write_reg(info, 0x301A, 0x12CE);
				sensor_write_reg(info, 0x3400, 0x7A20);
			}

			// polling sensor to confirm it's already in capture flow or in preview flow.
			// this can avoid frame mismatch issue due to inproper delay
			if (sensor_table == SENSOR_MODE_1600x1200) {

				// Check if we must turn on the flash or not...
				unsigned int exposure_time_denominator = 0;
				
				dev_dbg(&info->i2c_client->dev,"flash_mode=%d", info->flash_mode);

				sensor_get_exposure_time(info, &exposure_time_denominator);
				
				if ( info->flash_mode == YUV_5M_FlashControlOn ||
					(info->flash_mode == YUV_5M_FlashControlAuto && exposure_time_denominator < 15)
					) {
					ltc3216_turn_on_flash(info);
					info->flash_status = 1;
				} 
			
				// check current seq_state for capture
				poll_current_state(info,7);
			} else {
				// check current seq_state for preview
				poll_current_state(info,3);
			}
		} else {
			// check current seq_state for preview
			poll_current_state(info,3);
		}
	}
	info->mode = sensor_table;

	mutex_unlock(&yuv_lock);
	
	return 0;
}

static int sensor_delayedinit(struct sensor_info* info)
{
	int err = 0;
	// Power up sensor
	dev_dbg(&info->i2c_client->dev,"yuv5 delayed_init sensor powerup");
	if (info->pdata && info->pdata->power_on)
		info->pdata->power_on();
	
	// Write the initialization table - Returning from powerdown means we lose memory settings...
	dev_dbg(&info->i2c_client->dev,"yuv5 delayed_init sensor initialization");
	err = sensor_write_table(info, mode_table[SENSOR_MODE_INIT]);
	if (err) {
		dev_err(&info->i2c_client->dev,"yuv5 delayed_init sensor initialization failed");
	} else {
		info->mode = SENSOR_MODE_INIT;
		
		// Set default resolution
		dev_dbg(&info->i2c_client->dev,"yuv5 delayed_init sensor set default resolution");
		err = sensor_set_mode(info, 800, 600);
		if (err) {
			dev_err(&info->i2c_client->dev,"yuv5 delayed_init sensor set default resolution failed");
		}
	}
	
	return err;
}

static int sensor_get_flash_status(struct sensor_info *info)
{
	return info->flash_status;
} 

static long sensor_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct sensor_info *info = file->private_data;
	int err=0;

	// If sensor initialization is pending, perform it now. VI clock is set to the proper frequency
	if (info->init_pending) { 
		err = sensor_delayedinit(info);
		if (err)
			return err;
		info->init_pending = false;
	}
	
	dev_dbg(&info->i2c_client->dev,"cmd %u", cmd);
	switch (cmd) {
		case SENSOR_IOCTL_SET_MODE: {
			struct sensor_mode mode;

			if (copy_from_user(&mode,
					(const void __user *)arg,
					sizeof(struct sensor_mode))) {
				dev_err(&info->i2c_client->dev,"set_mode get mode failed");
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"set_mode %d x %d",mode.xres,mode.yres);

			return sensor_set_mode(info, mode.xres, mode.yres);
		}

		case SENSOR_IOCTL_GET_STATUS:
			dev_dbg(&info->i2c_client->dev,"get_status");
			return 0;

		case SENSOR_IOCTL_SET_COLOR_EFFECT: {
			int coloreffect;

			if (copy_from_user(&coloreffect,
					(const void __user *)arg,
					sizeof(coloreffect))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"coloreffect %d", coloreffect);

			switch(coloreffect) {
				case YUV_ColorEffect_None:
					err = sensor_write_table(info, ColorEffect_None);
					break;

				case YUV_ColorEffect_Mono:
					err = sensor_write_table(info, ColorEffect_Mono);
					break;

				case YUV_ColorEffect_Sepia:
					err = sensor_write_table(info, ColorEffect_Sepia);
					break;

				case YUV_ColorEffect_Negative:
					err = sensor_write_table(info, ColorEffect_Negative);
					break;

				case YUV_ColorEffect_Solarize:
					err = sensor_write_table(info, ColorEffect_Solarize);
					break;

				case YUV_ColorEffect_Posterize:
					err = sensor_write_table(info, ColorEffect_Posterize);
					break;

				default:
					break;
			}
			if (err)
				return err;
			return 0;
		}

		case SENSOR_IOCTL_SET_WHITE_BALANCE: {
			int whitebalance;

			if (copy_from_user(&whitebalance,
					(const void __user *)arg,
					sizeof(whitebalance))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"whitebalance %d", whitebalance);

			switch(whitebalance) {
				case YUV_Whitebalance_Auto:
					err = sensor_write_table(info, Whitebalance_Auto);
					break;

				case YUV_Whitebalance_Incandescent:
					err = sensor_write_table(info, Whitebalance_Incandescent);
					break;

				case YUV_Whitebalance_Daylight:
					err = sensor_write_table(info, Whitebalance_Daylight);
					break;

				case YUV_Whitebalance_Fluorescent:
					err = sensor_write_table(info, Whitebalance_Fluorescent);
					break;

				case YUV_Whitebalance_CloudyDaylight:
					err = sensor_write_table(info, Whitebalance_CloudyDaylight);
					break;

				default:
					break;
			}
			if (err)
				return err;
			return 0;
		}

		case SENSOR_IOCTL_GET_EXPOSURE_TIME: {
			unsigned int exposure_time_denominator;
			sensor_get_exposure_time(info, &exposure_time_denominator);
			if (copy_to_user((void __user *)arg,
					&exposure_time_denominator,
					sizeof(exposure_time_denominator)))
				return -EFAULT;

			dev_info(&info->i2c_client->dev,"exposure time %d", exposure_time_denominator);
			return 0;
		}
		
		case SENSOR_IOCTL_SET_SCENE_MODE:
			dev_dbg(&info->i2c_client->dev,"scene_mode");
			return 0;

		case SENSOR_IOCTL_SET_EXPOSURE: {
			int exposure;

			if (copy_from_user(&exposure,
					(const void __user *)arg,
					sizeof(exposure))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"exposure %d", exposure);

			switch(((int)exposure)-2) {
				case YUV_Exposure_0:
					err = sensor_write_table(info, Exposure_0);
					break;

				case YUV_Exposure_1:
					err = sensor_write_table(info, Exposure_1);
					break;

				case YUV_Exposure_2:
					err = sensor_write_table(info, Exposure_2);
					break;

				case YUV_Exposure_Negative_1:
					err = sensor_write_table(info, Exposure_Negative_1);
					break;

				case YUV_Exposure_Negative_2:
					err = sensor_write_table(info, Exposure_Negative_2);
					break;

				default:
					break;
			}
			if (err)
				return err;
			return 0;
		}

		default:
			dev_err(&info->i2c_client->dev,"default");
			return -EINVAL;
	}

	return 0;
}

static struct sensor_info *info = NULL;

static int sensor_open(struct inode *inode, struct file *file)
{
	dev_dbg(&info->i2c_client->dev,"%s", __func__);
	
	file->private_data = info;
	
	// If this is the first open, schedule a delayed initialization, as soon as Omx sets the right VI clock frequency
	if (++info->open_count == 1) {
		info->init_pending = true;
	}
	
	return 0;
}

int sensor_release(struct inode *inode, struct file *file)
{
	struct sensor_info *info = file->private_data;
	if (!info)
		return 0;
		
	dev_dbg(&info->i2c_client->dev,"%s", __func__);

	// If no pending opens, turn off sensor
	if (--info->open_count == 0) {
	
		// Turn off flash if turned on
		if (info->flash_status) {
			ltc3216_turn_off_flash(info);
			info->flash_status = 0;
		} 
	
		// Turn off sensor
		if (info->pdata && info->pdata->power_off) {
			dev_dbg(&info->i2c_client->dev,"turn off sensor");
			info->pdata->power_off();
		}
	}
	
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


static long sensor_5m_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct sensor_info *info = file->private_data;
	int err = 0;
	
	// If sensor initialization is pending, perform it now. VI clock is set to the proper frequency
	if (info->init_pending) { 
		err = sensor_delayedinit(info);
		if (err)
			return err;
		info->init_pending = false;
	}
	
	dev_dbg(&info->i2c_client->dev,"cmd %u", cmd);
	switch (cmd) {
		case SENSOR_5M_IOCTL_SET_MODE: {
			struct sensor_5m_mode mode;

			if (copy_from_user(&mode,
					(const void __user *)arg,
					sizeof(struct sensor_5m_mode))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"yuv5 set_mode %d x %d",mode.xres,mode.yres);
			
			return sensor_set_mode(info, mode.xres, mode.yres);
		}

		case SENSOR_5M_IOCTL_GET_STATUS:
			dev_dbg(&info->i2c_client->dev,"yuv5 get_status");
			return 0;

		case SENSOR_5M_IOCTL_GET_AF_STATUS:
			dev_dbg(&info->i2c_client->dev,"yuv5 get_af_status");
			// 1=NvCameraIspFocusStatus_Locked
			// 2=NvCameraIspFocusStatus_FailedToFind
			// 0=NvCameraIspFocusStatus_Busy
			return 1;


		case SENSOR_5M_IOCTL_SET_AF_MODE:
			dev_dbg(&info->i2c_client->dev,"yuv5 set_af_mode");
			return 0;

		case SENSOR_5M_IOCTL_GET_FLASH_STATUS:
			dev_dbg(&info->i2c_client->dev,"yuv5 get_flash_status");
			return sensor_get_flash_status(info);

		case SENSOR_5M_IOCTL_SET_COLOR_EFFECT: {
			u8 coloreffect;

			if (copy_from_user(&coloreffect,
					(const void __user *)arg,
					sizeof(coloreffect))) {
				return -EFAULT;
			}
			
			dev_dbg(&info->i2c_client->dev,"yuv5 coloreffect %d", coloreffect);
			
			switch(coloreffect) {
				case YUV_5M_ColorEffect_None:
					err = sensor_write_table(info, ColorEffect_None);
					break;

				case YUV_5M_ColorEffect_Mono:
					err = sensor_write_table(info, ColorEffect_Mono);
					break;

				case YUV_5M_ColorEffect_Sepia:
					err = sensor_write_table(info, ColorEffect_Sepia);
					break;

				case YUV_5M_ColorEffect_Negative:
					err = sensor_write_table(info, ColorEffect_Negative);
					break;

				case YUV_5M_ColorEffect_Solarize:
					err = sensor_write_table(info, ColorEffect_Solarize);
					break;

				case YUV_5M_ColorEffect_Posterize:
					err = sensor_write_table(info, ColorEffect_Posterize);
					break;

				default:
					break;
			}
			if (err)
				return err;
			return 0;
		}

		case SENSOR_5M_IOCTL_SET_WHITE_BALANCE: {
			u8 whitebalance;

			if (copy_from_user(&whitebalance,
					(const void __user *)arg,
					sizeof(whitebalance))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"yuv5 whitebalance %d", whitebalance);

			switch(whitebalance) {
				case YUV_5M_Whitebalance_Auto:
					err = sensor_write_table(info, Whitebalance_Auto);
					break;

				case YUV_5M_Whitebalance_Incandescent:
					err = sensor_write_table(info, Whitebalance_Incandescent);
					break;

				case YUV_5M_Whitebalance_Daylight:
					err = sensor_write_table(info, Whitebalance_Daylight);
					break;

				case YUV_5M_Whitebalance_Fluorescent:
					err = sensor_write_table(info, Whitebalance_Fluorescent);
					break;

				case YUV_5M_Whitebalance_CloudyDaylight:
					err = sensor_write_table(info, Whitebalance_CloudyDaylight);
					break;

				default:
					break;
			}
			if (err)
				return err;
			return 0;
		}

		case SENSOR_5M_IOCTL_SET_SCENE_MODE: {
			u8 scenemode;
			if (copy_from_user(&scenemode,
					(const void __user *)arg,
					sizeof(scenemode))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"yuv5 scenemode %d", scenemode);
			return 0;
		}

		case SENSOR_5M_IOCTL_SET_EXPOSURE: {
			u8 exposure;

			if (copy_from_user(&exposure,
					(const void __user *)arg,
					sizeof(exposure))) {
				return -EFAULT;
			}
			dev_dbg(&info->i2c_client->dev,"yuv5 exposure %d", exposure);
			switch(((int)exposure) - 2) {
				case YUV_Exposure_0:
					err = sensor_write_table(info, Exposure_0);
					break;

				case YUV_Exposure_1:
					err = sensor_write_table(info, Exposure_1);
					break;

				case YUV_Exposure_2:
					err = sensor_write_table(info, Exposure_2);
					break;

				case YUV_Exposure_Negative_1:
					err = sensor_write_table(info, Exposure_Negative_1);
					break;

				case YUV_Exposure_Negative_2:
					err = sensor_write_table(info, Exposure_Negative_2);
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
			dev_dbg(&info->i2c_client->dev,"yuv5 flashmode %d", (int)flashmode);
			
			info->flash_mode = (int) flashmode;

			switch (flashmode) {
				case YUV_5M_FlashControlOn:
					break;

				case YUV_5M_FlashControlOff:
					ltc3216_turn_off_flash(info);
					break;

				case YUV_5M_FlashControlAuto:
					break;

				case YUV_5M_FlashControlTorch:
					ltc3216_turn_on_flash(info);
					break;

				default:
					break;
			} 
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
			dev_dbg(&info->i2c_client->dev,"yuv5 capture cmd");
			return 0;

		default:
			dev_err(&info->i2c_client->dev,"yuv5 default");
			return -EINVAL;
	}

	return 0;
}

static int sensor_5m_open(struct inode *inode, struct file *file)
{
	dev_dbg(&info->i2c_client->dev,"%s", __func__);
	
	file->private_data = info;
	
	// If this is the first open, schedule a delayed initialization, as soon as Omx sets the right VI clock frequency
	if (++info->open_count == 1) {
		info->init_pending = true;
	}
	
	return 0;
}

int sensor_5m_release(struct inode *inode, struct file *file)
{
	struct sensor_info *info = file->private_data;
	if (!info)
		return 0;
		
	dev_dbg(&info->i2c_client->dev,"%s", __func__);
	
	// If no pending opens, turn off sensor
	if (--info->open_count == 0) {
	
		// Turn off flash if turned on
		if (info->flash_status) {
			ltc3216_turn_off_flash(info);
			info->flash_status = 0;
		} 

		// Turn off sensor
		if (info->pdata && info->pdata->power_off) {
			dev_dbg(&info->i2c_client->dev,"turn off sensor");
			info->pdata->power_off();
		}
	}
	
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
			pr_err("%s: 0x%x", __func__, __LINE__);
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
	struct sensor_info *info = file->private_data;
	
	switch (cmd) {
	case LTC3216_IOCTL_FLASH_ON:
		ltc3216_turn_on_flash(info);
		break;

	case LTC3216_IOCTL_FLASH_OFF:
		ltc3216_turn_off_flash(info);
		break;
		
	case LTC3216_IOCTL_TORCH_ON:
		ltc3216_turn_on_flash(info);
		break;

	case LTC3216_IOCTL_TORCH_OFF:
		ltc3216_turn_off_flash(info);
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

extern void extern_tegra_camera_enable_vi(void);
extern void extern_tegra_camera_disable_vi(void);
extern void extern_tegra_camera_clk_set_rate(struct tegra_camera_clk_info *);

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tegra_camera_clk_info clk_info;
	int ret = 0;
	
	dev_info(&client->dev,"%s", __func__);

	info = devm_kzalloc(&client->dev,
				sizeof(struct sensor_info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev,"Unable to allocate memory");
		return -ENOMEM;
	}

	mutex_init(&yuv_lock);

	info->pdata = client->dev.platform_data;
	info->i2c_client = client;
	i2c_set_clientdata(client, info);
	
	// set MCLK to 24MHz - Lower or higher frecuencies do not work!
	clk_info.id = TEGRA_CAMERA_MODULE_VI;
	clk_info.clk_id = TEGRA_CAMERA_VI_SENSOR_CLK;
	clk_info.rate = 24000000;
	extern_tegra_camera_clk_set_rate(&clk_info);

	// turn on MCLK and pull down PWDN pin
	extern_tegra_camera_enable_vi();
	if (info->pdata && info->pdata->power_on)
		info->pdata->power_on(); 

	// Detect if the image sensor is present
	ret = sensor_write_table(info, mode_table[SENSOR_MODE_INIT]);

	// pull high PWDN pin and turn off MCLK
	if (info->pdata && info->pdata->power_off)
		info->pdata->power_off();
	extern_tegra_camera_disable_vi(); 

	// If failure, do not register sensor
	if (ret != 0) {
		dev_err(&client->dev,"mt9d115 image sensor not detected");
		return -ENODEV;
	}
	
	misc_register(&sensor_device);
	misc_register(&sensor_5m_device);
	misc_register(&ad5820_device);
	misc_register(&ltc3216_device);
	
	dev_info(&client->dev,"mt9d115 image sensor driver loaded");
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
