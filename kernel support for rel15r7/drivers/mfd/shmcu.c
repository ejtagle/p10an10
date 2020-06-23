/*
 * ShMCU N10 driver
 *
 * Copyright (C) 2012 Eduardo José Tagle <ejtagle@tutopia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
 
//#define DEBUG

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <linux/mfd/shmcu.h>

#include <linux/debugfs.h>
#include <linux/seq_file.h> 

struct shmcu_ctx {
	struct device		*dev;
	struct i2c_client	*client;
	struct gpio_chip	gpio;
	int    				gpio_value;
	int					led_gpio;
	
#ifdef CONFIG_HAS_EARLYSUSPEND	
	struct early_suspend early_suspend;
#endif
}; 

/* returns number of bytes read */
static int shmcu_i2c_read(struct shmcu_ctx* ctx, int v, u8* buf )
{
	switch(v - 1) {
		case 0: 
			return i2c_smbus_read_i2c_block_data(ctx->client,0x11,2,buf);
		case 1:
			{ // Read RTC time
				int ret = i2c_smbus_read_i2c_block_data(ctx->client,0x22,7,buf);
				dev_dbg(&ctx->client->dev,"shmcu_i2c_read: read 0x%02X data, return %d\n",0x22,ret);
				dev_dbg(&ctx->client->dev,"   Date(yyyy/mm/dd) : %2d%2d/%2d/%2d\n",buf[0],buf[1],buf[2],buf[3]);
				dev_dbg(&ctx->client->dev,"   Time(hh:mm:ss) : %2d:%2d:%2d\n",buf[4],buf[5],buf[6]);
				return ret;
			}
		case 5:
			{
				int ret = i2c_smbus_read_i2c_block_data(ctx->client,0x23,7,buf);
				dev_dbg(&ctx->client->dev,"shmcu_i2c_read: read 0x%02X data, return %d\n",0x23,ret);
				dev_dbg(&ctx->client->dev,"   Date(yyyy/mm/dd) : %2d%2d/%2d/%2d\n",buf[0],buf[1],buf[2],buf[3]);
				dev_dbg(&ctx->client->dev,"   Time(hh:mm:ss) : %2d:%2d:%2d\n",buf[4],buf[5],buf[6]);
				return ret;
			}
		case 9:
			return i2c_smbus_read_i2c_block_data(ctx->client,0x1,3,buf); // mcu version
		case 10:
			return i2c_smbus_read_i2c_block_data(ctx->client,0x15,1,buf); 
		case 11:
			return i2c_smbus_read_i2c_block_data(ctx->client,0x12,2,buf);

		case 13:
			return i2c_smbus_read_i2c_block_data(ctx->client,0x13,2,buf);
		case 14:
			return i2c_smbus_read_i2c_block_data(ctx->client,0x10,2,buf);
		case 15:
			return i2c_smbus_read_i2c_block_data(ctx->client,0x14,2,buf);
		case 16:
			return i2c_smbus_read_i2c_block_data(ctx->client,0x16,1,buf);
		case 17:
			return i2c_smbus_read_i2c_block_data(ctx->client,0x32,2,buf);
		case 18:
			return i2c_smbus_read_i2c_block_data(ctx->client,0x33,2,buf);
		case 19:
			return i2c_smbus_read_i2c_block_data(ctx->client,0x35,2,buf);
		case 20:
			return i2c_smbus_read_i2c_block_data(ctx->client,0x36,2,buf);
		case 21:
			return i2c_smbus_read_i2c_block_data(ctx->client,0x37,2,buf);

		case 22:
			return i2c_smbus_read_i2c_block_data(ctx->client,0x34,2,buf);
	}
	return 0;
}

static int shmcu_mcu_version(struct shmcu_ctx* ctx,u8* ver /*[3]*/)
{
	int ret = shmcu_i2c_read(ctx,0xA,ver);
	dev_dbg(&ctx->client->dev,"mcu version: %x.%x.0%x , ret:%d", ver[0],ver[1],ver[2],ret);
	return ret;
}

static int shmcu_i2c_write(struct shmcu_ctx* ctx,u8 cmd,u8* buf)
{
	if (cmd == 0x15) {
		dev_dbg(&ctx->client->dev,"COLD BOOT: %02d%02d",buf[0],buf[1]);
		return i2c_smbus_write_i2c_block_data(ctx->client,0x20,2,buf);
	} 
	if (cmd < 0x15) {
		if (cmd == 3) {
			return i2c_smbus_write_i2c_block_data(ctx->client,0x22,7,buf);
		}
		if (cmd == 5) {
			return i2c_smbus_write_i2c_block_data(ctx->client,0x23,7,buf);
		}
		return 0;
	}
	if (cmd == 0x18) {
		int ret = i2c_smbus_write_i2c_block_data(ctx->client,0x25,1,buf);
		dev_dbg(&ctx->client->dev,"write %02d data, return %d",buf[0],ret);
		return ret;
	}
	if (cmd == 0x63) {
		int i =0 ; // CHECKME!!
		if (buf[0] == 0) {
			return 0;
		}
		while (buf[i] != 0xA) {
			i++;
		};
		return i2c_master_send(ctx->client,buf,i);
	}
	
	if (cmd == 0x16) {
		return i2c_smbus_write_i2c_block_data(ctx->client,0x23,1,buf);
	}
	
	return 0;
}

static int shmcu_host_status(struct shmcu_ctx* ctx,int status)
{
	u8 buf [1];
	dev_dbg(&ctx->client->dev,"host status:\n");
	
	if (status == 0) {
		dev_dbg(&ctx->client->dev,"boot");
		buf[0] = 2;
		return shmcu_i2c_write(ctx,0x16,buf);
	}

	if (status == 1) {
		dev_dbg(&ctx->client->dev,"suspend");
		buf[0] = 3;
		return shmcu_i2c_write(ctx,0x16,buf);
		
	}
	
	dev_dbg(&ctx->client->dev,"resume");
	buf[0] = 4;
	return shmcu_i2c_write(ctx,0x16,buf);

}

static int shmcu_cold_boot(struct shmcu_ctx* ctx,int time)
{
	u8 buf[2];

	buf[0] = 1;
	buf[1] = time;
	return shmcu_i2c_write(ctx,0x15,buf);
}

int shmcu_read(struct device* dev, int v,u8* buf)
{
	struct shmcu_ctx* ctx = dev_get_drvdata(dev); 
	return shmcu_i2c_read(ctx,v,buf);
}
EXPORT_SYMBOL_GPL(shmcu_read);

int shmcu_write(struct device* dev, int v,u8* buf)
{
	struct shmcu_ctx* ctx = dev_get_drvdata(dev); 
	return shmcu_i2c_write(ctx,v,buf);
}
EXPORT_SYMBOL_GPL(shmcu_write);
	
#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
static int shmcu_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct shmcu_ctx *ctx = i2c_get_clientdata(client);
	shmcu_host_status(ctx,1);
	return 0;
}

static int shmcu_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct shmcu_ctx *ctx = i2c_get_clientdata(client);
	shmcu_host_status(ctx,2);
	return 0;
}
#endif

static int shmcu_gpio_get(struct gpio_chip *gc, unsigned offset)
{
	struct shmcu_ctx *ctx = container_of(gc, struct shmcu_ctx, gpio);
	return !!((ctx->gpio_value >> offset) & 1);
}

static void shmcu_gpio_set(struct gpio_chip *gc, unsigned offset,
			      int value)
{
	struct shmcu_ctx *ctx = container_of(gc, struct shmcu_ctx, gpio);
	u8 val;
	if (value) {
		ctx->gpio_value |= (1 << offset);
		val = 1;
	} else {
		ctx->gpio_value &= ~(1 << offset);
		val = 0;
	}

	dev_dbg(ctx->dev, "Setting led GPIO to %d\n", val);
	shmcu_i2c_write(ctx,0x18,&val);
	gpio_set_value(ctx->led_gpio,val);
}

static int shmcu_gpio_input(struct gpio_chip *gc, unsigned offset)
{
	return -ENOSYS;
}

static int shmcu_gpio_output(struct gpio_chip *gc, unsigned offset,
				int value)
{
	struct shmcu_ctx *ctx = container_of(gc, struct shmcu_ctx, gpio);
	u8 val;
	
	if (value) {
		ctx->gpio_value |= (1 << offset);
		val = 1;
	} else {
		ctx->gpio_value &= ~(1 << offset);
		val = 0;
	}

	dev_dbg(ctx->dev, "Setting led GPIO to %d\n", val);
	shmcu_i2c_write(ctx,0x18,&val);
	gpio_set_value(ctx->led_gpio,val);
	return 0;
}

static int shmcu_gpio_init(struct shmcu_ctx *ctx, int gpio_base)
{
	int ret;

	if (!gpio_base)
		return 0;

	ctx->gpio.owner		= THIS_MODULE;
	ctx->gpio.label		= ctx->client->name;
	ctx->gpio.dev		= ctx->dev;
	ctx->gpio.base		= gpio_base;
	ctx->gpio.ngpio		= 1;
	ctx->gpio.can_sleep	= 1;

	ctx->gpio.direction_input	= shmcu_gpio_input;
	ctx->gpio.direction_output	= shmcu_gpio_output;
	ctx->gpio.set				= shmcu_gpio_set;
	ctx->gpio.get				= shmcu_gpio_get;

	ret = gpiochip_add(&ctx->gpio);
	if (ret)
		dev_warn(ctx->dev, "GPIO registration failed: %d\n", ret);
	return ret;
} 

static int __remove_subdev(struct device *dev, void *unused)
{
	platform_device_unregister(to_platform_device(dev));
	return 0;
}

static int shmcu_remove_subdevs(struct shmcu_ctx *ctx)
{
	return device_for_each_child(ctx->dev, NULL, __remove_subdev);
}
 
static int __devinit shmcu_add_subdevs(struct shmcu_ctx *ctx,
					  struct shmcu_platform_data* pdata)
{
	struct shmcu_subdev_info *subdev;
	struct platform_device *pdev;
	int i, ret = 0;

	for (i = 0; i < pdata->num_subdevs; i++) {
		subdev = &pdata->subdevs[i];

		pdev = platform_device_alloc(subdev->name, subdev->id);

		pdev->dev.parent = ctx->dev;
		pdev->dev.platform_data = subdev->platform_data;

		ret = platform_device_add(pdev);
		if (ret)
			goto failed;
	}
	return 0;

failed:
	shmcu_remove_subdevs(ctx);
	return ret;
}  

struct shmcu_ctx *g_shmcu = NULL;

/* Power down by using shmcu */
void shmcu_poweroff(void)
{
	if (g_shmcu != NULL) {
		shmcu_cold_boot(g_shmcu,10);
	} else {
		pr_emerg("shmcu not initialized. Unable to shutdown\n");
	}
}
EXPORT_SYMBOL(shmcu_poweroff);

/* Restart by using shmcu */
void shmcu_restart(void)
{
	if (g_shmcu != NULL) {
		shmcu_cold_boot(g_shmcu,10);
	} else {
		pr_emerg("shmcu not initialized. Unable to restart\n");
	}
}
EXPORT_SYMBOL(shmcu_restart);
 
#ifdef CONFIG_HAS_EARLYSUSPEND
static void shmcu_early_suspend(struct early_suspend *h);
static void shmcu_late_resume(struct early_suspend *h);
#endif


static int __devinit shmcu_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct shmcu_ctx *shmcu;
	struct shmcu_platform_data *pdata = client->dev.platform_data;
	int ret = 0;
	u8 buf[3];
	
	dev_info(&client->dev,"SHMCU Driver\n");

	if (!pdata) {
		dev_err(&client->dev,"no platform data\n");
		return -EIO;
	}

	shmcu = kzalloc(sizeof(struct shmcu_ctx), GFP_KERNEL);
	if (shmcu == NULL)
		return -ENOMEM;

	shmcu->client = client;
	shmcu->dev = &client->dev;
	shmcu->led_gpio = pdata->led_gpio;
	i2c_set_clientdata(client, shmcu);
	
	/* Get version */
	if (shmcu_mcu_version(shmcu,buf) <= 0) {
		dev_err(&client->dev,"SHMCU not found\n");
		return -EIO;
	}
	
	dev_info(&client->dev,"SHMCU version: %x.%x.0%x", buf[0],buf[1],buf[2]);

	/* Notify the SHMCU to boot */
	shmcu_host_status(shmcu,0);
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	shmcu->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1;
	shmcu->early_suspend.suspend = shmcu_early_suspend;
	shmcu->early_suspend.resume = shmcu_late_resume;
	register_early_suspend(&shmcu->early_suspend);
#endif

	gpio_request(pdata->led_gpio,"shmcu-led");
	gpio_direction_output(pdata->led_gpio,0);

	/* Register gpio */
	ret = shmcu_gpio_init(shmcu, pdata->gpio_base);
	if (ret) {
		dev_err(&client->dev, "GPIO registration failed: %d\n", ret);
		goto err_gpio_init;
	} 
	
	/* Register subdevices */
	ret = shmcu_add_subdevs(shmcu, pdata);
	if(ret) {
		dev_err(&client->dev, "error adding subdevices\n");
		goto err_add_devs;
	}
	
	/* Keep a pointer to the shmcu chip structure */
	g_shmcu = shmcu; 
	
	dev_info(&client->dev, "SHMCU controller driver registered\n");  
	return 0;
	
err_add_devs:
	if (pdata->gpio_base) {
		ret = gpiochip_remove(&shmcu->gpio);
		if (ret)
			dev_err(&client->dev, "Can't remove gpio chip: %d\n", ret);
	} 
	
err_gpio_init:
	gpio_free(pdata->led_gpio);
	
#ifdef CONFIG_HAS_EARLYSUSPEND	
	unregister_early_suspend(&shmcu->early_suspend);
#endif 


	kfree(shmcu);
	return ret; 	
	
}

static int __devexit shmcu_i2c_remove(struct i2c_client *client)
{
	struct shmcu_ctx *shmcu = i2c_get_clientdata(client);
	struct shmcu_platform_data *pdata = client->dev.platform_data;
	
	/* No more pointer to the nvec chip structure */
	g_shmcu = NULL;

	/* Remove subdevices */
	shmcu_remove_subdevs(shmcu); 
	
	if (pdata->gpio_base) {
		int ret = gpiochip_remove(&shmcu->gpio);
		if (ret)
			dev_err(&client->dev, "Can't remove gpio chip: %d\n", ret);
	} 
	
	gpio_free(pdata->led_gpio);
	
#ifdef CONFIG_HAS_EARLYSUSPEND	
	unregister_early_suspend(&shmcu->early_suspend);
#endif 

	i2c_set_clientdata(client, NULL);
		
	kfree(shmcu);
	return 0;
}

#if !defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_PM)
static const struct dev_pm_ops shmcu_pm_ops = {
	.suspend	= shmcu_suspend,
	.resume		= shmcu_resume,
};
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void shmcu_early_suspend(struct early_suspend *h)
{
	struct shmcu_ctx *ctx;
	ctx = container_of(h, struct shmcu_ctx, early_suspend);
	shmcu_suspend(&ctx->client->dev);
}

static void shmcu_late_resume(struct early_suspend *h)
{
	struct shmcu_ctx *ctx;
	ctx = container_of(h, struct shmcu_ctx, early_suspend);
	shmcu_resume(&ctx->client->dev);
}
#endif

static const struct i2c_device_id shmcu_id_table[] = {
	{ "shmcu", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, shmcu_id_table);

static struct i2c_driver shmcu_driver = {
	.driver	= {
		.name	= "shmcu",
		.owner	= THIS_MODULE,
#if !defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_PM)
		.pm 	= &shmcu_pm_ops,
#endif
	},
	.probe		= shmcu_i2c_probe,
	.remove		= __devexit_p(shmcu_i2c_remove),
	.id_table	= shmcu_id_table,
};

static int __init shmcu_init(void)
{
	return i2c_add_driver(&shmcu_driver);
}
subsys_initcall(shmcu_init);

static void __exit shmcu_exit(void)
{
	i2c_del_driver(&shmcu_driver);
}
module_exit(shmcu_exit);

MODULE_DESCRIPTION("shmcu core driver");
MODULE_AUTHOR("Eduardo José Tagle <ejtagle@tutopia.com>");
MODULE_LICENSE("GPL");