/*
 * SHMCU: N10 battery and RTC manager
 *
 * Copyright (C) 2012 Eduardo José Tagle <ejtagle@hotmail.com> 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#ifndef __LINUX_MFD_SHMCU
#define __LINUX_MFD_SHMCU


struct shmcu_subdev_info {
	char* name;				/* subdevice name */
	int	  id;				/* subdevice id */
	void* platform_data;	/* Associated platform data */
};
	
struct shmcu_platform_data {
	int		gpio_base;						/* base gpios */
	int		led_gpio;						/* LED gpio */
	struct shmcu_subdev_info *subdevs;		/* subdevices to register */
	int 					 num_subdevs;	/* Number of subdevices */
	
};

extern int shmcu_read(struct device* dev, int v,u8* buf);

extern int shmcu_write(struct device* dev, int v,u8* buf);

extern void shmcu_poweroff(void);

extern void shmcu_restart(void);


#endif
