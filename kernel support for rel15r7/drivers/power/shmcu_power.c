/*
 * power supply driver for a SHMCU
 *
 * Copyright (C) 2012 Eduardo José Tagle <ejtagle@tutopia.com>  
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

//#define DEBUG

#include <linux/module.h>
#include <linux/power/shmcu_power.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/mfd/shmcu.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/reboot.h>
#include <linux/jiffies.h>

#include <asm/irq.h>
 
#define SHMCU_POWER_POLLING_INTERVAL 10000

struct shmcu_power_ctx {
	struct device *			 dev;
	struct dentry *			 debug_file;
	struct delayed_work 	 work;			/* battery status polling */
	struct workqueue_struct *work_queue; 
	
	struct work_struct 		 isr_work;		/* charger state notification ISR */
	struct workqueue_struct *isr_wq; 

	struct mutex 			 lock;			/* mutex protect battery update */
	
	long unsigned int        next_update;	/* Time to next update */
	
	int    dc_charger_gpio;	/* Gpio used to detect if charger is plugged in */ 
	int	   dc_charger_irq;
	int		use_irq;
	
	/* Battery status and information */
	int on;
	int bat_present;
	int bat_status;
	int bat_voltage_now;
	int bat_current_now;
	int bat_current_avg;
	int bat_health;
	int time_remain;
	int charge_full_design;
	int charge_last_full;
	int critical_capacity;
	int capacity_remain;
	int bat_temperature;
	int bat_cap;	/* as percent */
	int bat_type_enum;
	char bat_manu[32];
	char bat_model[32];
	char bat_type[32]; 

};

static enum power_supply_property shmcu_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property shmcu_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_EMPTY,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static int shmcu_power_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct shmcu_power_ctx *ctx = dev_get_drvdata(psy->dev->parent);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = ctx->on;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int shmcu_battery_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct shmcu_power_ctx *ctx = dev_get_drvdata(psy->dev->parent);

	switch (psp) {
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = ctx->bat_health;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = ctx->bat_status;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = ctx->bat_cap;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = ctx->bat_present;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = ctx->bat_voltage_now;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = ctx->bat_current_now;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = ctx->bat_current_avg;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		val->intval = ctx->time_remain;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = ctx->charge_full_design;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = ctx->charge_last_full;
		break;
	case POWER_SUPPLY_PROP_CHARGE_EMPTY:
		val->intval = ctx->critical_capacity;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = ctx->capacity_remain;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = ctx->bat_temperature;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = ctx->bat_manu;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = ctx->bat_model;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = ctx->bat_type_enum;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static char *shmcu_power_supplied_to[] = {
	"battery",
};

static struct power_supply shmcu_bat_psy = {
	.name			= "battery",
	.type			= POWER_SUPPLY_TYPE_BATTERY,
	.properties		= shmcu_battery_props,
	.num_properties	= ARRAY_SIZE(shmcu_battery_props),
	.get_property	= shmcu_battery_get_property,
};

static struct power_supply shmcu_ac_psy = {
	.name 			= "ac",
	.type 			= POWER_SUPPLY_TYPE_MAINS,
	.supplied_to 	= shmcu_power_supplied_to,
	.num_supplicants= ARRAY_SIZE(shmcu_power_supplied_to),
	.properties 	= shmcu_power_props,
	.num_properties = ARRAY_SIZE(shmcu_power_props),
	.get_property 	= shmcu_power_get_property,
}; 

static inline struct device *to_shmcu_dev(struct device *dev)
{
	return dev->parent;
}

static int shmcu_get_bat_perc(struct shmcu_power_ctx* ctx,u8* buf /*1*/)
{
	struct device *shmcu_dev = to_shmcu_dev(ctx->dev);
	return shmcu_read(shmcu_dev, 0xB ,buf);
}

static int shmcu_get_bat_volt(struct shmcu_power_ctx* ctx,u8* buf /*2*/)
{
	struct device *shmcu_dev = to_shmcu_dev(ctx->dev);
	return shmcu_read(shmcu_dev,0x1,buf);
}

static int shmcu_get_bat_curr(struct shmcu_power_ctx* ctx,u8* buf /*2*/)
{
	struct device *shmcu_dev = to_shmcu_dev(ctx->dev);
	return shmcu_read(shmcu_dev,0xC,buf);
}

static int shmcu_get_bat_temp(struct shmcu_power_ctx* ctx,u8* buf /*2*/)
{
	struct device *shmcu_dev = to_shmcu_dev(ctx->dev);
	return shmcu_read(shmcu_dev,0xF,buf);
}

static int shmcu_get_bat_brc(struct shmcu_power_ctx* ctx,u8* buf /*2*/)
{
	struct device *shmcu_dev = to_shmcu_dev(ctx->dev);
	return shmcu_read(shmcu_dev,0xE,buf);
}

static int shmcu_get_bat_bfcc(struct shmcu_power_ctx* ctx,u8* buf /*2*/)
{
	struct device *shmcu_dev = to_shmcu_dev(ctx->dev);
	return shmcu_read(shmcu_dev,0x10,buf);
}

static int shmcu_get_bat_health(struct shmcu_power_ctx* ctx,u8* buf /*2*/)
{
	struct device *shmcu_dev = to_shmcu_dev(ctx->dev);
	return shmcu_read(shmcu_dev,0x1,buf);
}

static int shmcu_check_function(struct shmcu_power_ctx* ctx)
{
	u8 buf[2];
	struct device *shmcu_dev = to_shmcu_dev(ctx->dev);
	shmcu_read(shmcu_dev,0xC,buf);
	return (buf[0] == 0x12 && buf[1] == 0x12) ? 1 : 0;
}

#define shswap16(x) ((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF))

/* Get battery manufacturer data */
static void get_bat_mfg_data(struct shmcu_power_ctx *ctx)
{	
	int v;
	
	v = 0;
	shmcu_get_bat_brc(ctx,(u8*)&v);
	v = shswap16(v);
	v *= 1000;
	ctx->critical_capacity = v;
	

	v = 0;
	shmcu_get_bat_bfcc(ctx,(u8*)&v);
	v = shswap16(v);
	v *= 1000;
	ctx->charge_full_design = v;
	ctx->charge_last_full = v;
	
	/* Validate critical capacity. If it does make not sense, estimate it
	   as 10% of full capacity */
	if (ctx->critical_capacity == 0 || 
		ctx->critical_capacity >= (ctx->charge_full_design>>1)) {
		/* Seems to be incorrect. Estimate it as 10% of full design */
		dev_dbg(ctx->dev,"critical capacity seems wrong. Estimate it as 10%% of total\n");
		ctx->critical_capacity = ctx->charge_full_design / 10;
	}
	
	strcpy(ctx->bat_manu,"Unknown");
	strcpy(ctx->bat_model,"Unknown");
	strcpy(ctx->bat_type,"LIPOLY");
	ctx->bat_type_enum = POWER_SUPPLY_TECHNOLOGY_LIPO;
}


/* This fn is the only one that should be called by the driver to update
   the battery status */
static int shmcu_update_status(struct shmcu_power_ctx *ctx,bool force_update)
{
	int bat_status_changed = 0,v;
	
	/* Do not accept to update too often - NVEC seems not to be able to handle
	   too much pressure */
	if (!force_update && ((int)(ctx->next_update - jiffies)) > 0)
		return 0;

	dev_dbg(ctx->dev, "Updating battery info: %lu, %lu\n",ctx->next_update,jiffies);
	
	/* get exclusive access to the battery */
	mutex_lock(&ctx->lock);	

	/* Get bat health */
	v = 0;
	shmcu_get_bat_health(ctx,(u8*)&v);
	v = shswap16(v);
	dev_dbg(ctx->dev, "Bat health: %d\n",v);
	
	v -= 3200;
	if (v < 1000) {
		v = POWER_SUPPLY_HEALTH_GOOD;
	} else
	if (v > 4200) {
		v = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else {
		v = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}
	if (v != ctx->bat_health) {
		bat_status_changed = 1;
	}
	ctx->bat_health = v;
	
	/* Get remaining capacity */	
	if (shmcu_check_function(ctx)) {
	
		dev_dbg(ctx->dev, "Estimating capacity using voltage\n");
		v = 0;
		shmcu_get_bat_volt(ctx,(u8*)&v);
		v = shswap16(v);
		v = (v - 3400) / 7;
		if (v < 0) v = 0;
		else if (v > 100) v = 100;
		ctx->bat_cap = v;
	
	} else {
	
		dev_dbg(ctx->dev, "Estimating capacity using percent\n");
		v = 0;
		shmcu_get_bat_perc(ctx,(u8*)&v);
		if (v != ctx->capacity_remain) {
			bat_status_changed = 1;
		}
		ctx->bat_cap = v;
	}
	ctx->capacity_remain = v * ctx->charge_last_full / 100;

	v = 0;
	shmcu_get_bat_curr(ctx,(u8*)&v);
	v = shswap16(v);
	if (v & 0x8000) v = -100;
	else if (v & 0x0001) v = 100;
	else v = 0;

	v *= 1000;
	if (v != ctx->bat_current_now) {
		bat_status_changed = 1;
	}
	ctx->bat_current_now = v;
	ctx->bat_current_avg = v;

	
	v = 0;
	/* Get battery status */
	if (gpio_get_value(ctx->dc_charger_gpio)) {
		// Charging unless 100%
		v = (ctx->capacity_remain > 99) ? 
				POWER_SUPPLY_STATUS_FULL : 
				POWER_SUPPLY_STATUS_CHARGING;
	} else {
		v = (ctx->bat_current_now < 0) ?
				POWER_SUPPLY_STATUS_DISCHARGING :
				POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
	
	ctx->bat_present = 1;
	if (v != ctx->bat_status) {
		bat_status_changed = 1;
	}
	ctx->bat_status = v;

	v = 0;
	shmcu_get_bat_volt(ctx,(u8*)&v);
	v = shswap16(v);
	v *= 1000;
	if (v != ctx->bat_voltage_now) {
		bat_status_changed = 1;
	}
	ctx->bat_voltage_now = v;

	/* AC status via sys req */
	{
		int ac = 0;	

		/* Validate results... If battery is charging, then the AC adapter 
		   MUST be present, and if battery is not charging, it must not be*/
		if (ctx->bat_status == POWER_SUPPLY_STATUS_CHARGING) {
			ac = 1;
		} else
		if (ctx->bat_status == POWER_SUPPLY_STATUS_DISCHARGING) {
			ac = 0;
		}
			
		if (ctx->on != ac) {
			ctx->on = ac;
			power_supply_changed(&shmcu_ac_psy);
		}
	}

	v = 0;
	shmcu_get_bat_temp(ctx,(u8*)&v);
	v = shswap16(v);
	if (v != ctx->bat_temperature) {
		bat_status_changed = 1;
	}
	ctx->bat_temperature = v;
	
	v = ctx->bat_cap * 360;
	if (v != ctx->time_remain) {
		bat_status_changed = 1;
	}
	ctx->time_remain = v;
	
	if (bat_status_changed)
		power_supply_changed(&shmcu_bat_psy);
	
	/* Allow next update 10 seconds from now */
	ctx->next_update = jiffies + msecs_to_jiffies(SHMCU_POWER_POLLING_INTERVAL/2);
		
	mutex_unlock(&ctx->lock);		
	
	return 0;
} 

#ifdef CONFIG_DEBUG_FS
static int bat_debug_show(struct seq_file *s, void *data)
{
	struct shmcu_power_ctx *ctx = s->private;
	const char* bat_status = "unknown";
	
	int ret = shmcu_update_status(ctx,true);
	if (ret)
		return ret; 
		
	/* Convert battery status */
	switch (ctx->bat_status) {
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		bat_status = "not charging";
		break;
	case POWER_SUPPLY_STATUS_CHARGING:
		bat_status = "charging";
		break;
	case POWER_SUPPLY_STATUS_DISCHARGING:
		bat_status = "discharging";
		break;
	default:
		break;
	}
	
	seq_printf(s, "AC charger is %s\n", ctx->on ? "on" : "off");
	seq_printf(s, "Battery is %s\n", ctx->bat_present ? "present" : "absent");
	seq_printf(s, "Battery is %s\n", bat_status);
	seq_printf(s, "Battery voltage is %d mV\n", ctx->bat_voltage_now / 1000);	
	seq_printf(s, "Battery current is %d mA\n", ctx->bat_current_now / 1000);	
	seq_printf(s, "Battery average current is %d mA\n", ctx->bat_current_avg / 1000);	
	seq_printf(s, "Battery remaining time is %d minutes\n", ctx->time_remain/60);	
	seq_printf(s, "Battery full charge by design is %d mAh\n", ctx->charge_full_design/1000);	
	seq_printf(s, "Battery last full charge was %d mAh\n", ctx->charge_last_full/1000);		
	seq_printf(s, "Battery critical capacity is %d mAh\n", ctx->critical_capacity/1000);		
	seq_printf(s, "Battery capacity remaining is %d mAh\n", ctx->capacity_remain/1000);		
	seq_printf(s, "Battery temperature is %d degrees C\n", ctx->bat_temperature/10);		
	seq_printf(s, "Battery current capacity is %d %%\n", ctx->bat_cap);		
	seq_printf(s, "Battery manufacturer is %s\n", ctx->bat_manu);			
	seq_printf(s, "Battery model is %s\n", ctx->bat_model);			
	seq_printf(s, "Battery chemistry is %s\n", ctx->bat_type);			
	
	return 0;
}

static int debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, bat_debug_show, inode->i_private);
}

static const struct file_operations bat_debug_fops = {
	.open		= debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct dentry *shmcu_create_debugfs(struct shmcu_power_ctx *ctx)
{
	ctx->debug_file = debugfs_create_file("charger", 0666, 0, ctx,
						 &bat_debug_fops);
	return ctx->debug_file;
}

static void shmcu_remove_debugfs(struct shmcu_power_ctx *ctx)
{
	debugfs_remove(ctx->debug_file);
}
#else
static inline struct dentry *shmcu_create_debugfs(struct shmcu_power_ctx *ctx)
{
	return NULL;
}
static inline void shmcu_remove_debugfs(struct shmcu_power_ctx *ctx)
{
}
#endif 

static void shmcu_work_func(struct work_struct *work)
{
	struct shmcu_power_ctx *ctx = container_of((struct delayed_work*)work,
		struct shmcu_power_ctx, work);

	/* Update power supply status */
	shmcu_update_status(ctx,false);
	
	queue_delayed_work(ctx->work_queue, &ctx->work,
				 msecs_to_jiffies(SHMCU_POWER_POLLING_INTERVAL));
}   

static void shmcu_isr_work_func(struct work_struct *isr_work)
{
	struct shmcu_power_ctx *ctx = container_of(isr_work, struct shmcu_power_ctx, isr_work);

	/* Update power supply status */
	shmcu_update_status(ctx,true);
		
	enable_irq(ctx->dc_charger_irq);
} 

static irqreturn_t shmcu_irq_handler(int irq, void *dev_id)
{
	struct shmcu_power_ctx *ctx = dev_id;

	disable_irq_nosync(ctx->dc_charger_irq);
	
	queue_work(ctx->isr_wq, &ctx->isr_work);
	return IRQ_HANDLED;
} 
  
static int __devinit shmcu_power_probe(struct platform_device *pdev)
{
	struct shmcu_power_ctx *power;
	struct shmcu_power_platform_data *pdata = pdev->dev.platform_data;
	int ret;

	if (pdata == NULL) {
		dev_err(&pdev->dev, "no platform data\n");
		return -EINVAL; 
	}			


	power = kzalloc(sizeof(*power), GFP_KERNEL);
	if (power == NULL) {
		dev_err(&pdev->dev, "no memory for context\n");
		return -ENOMEM;
	}
	dev_set_drvdata(&pdev->dev, power);
	platform_set_drvdata(pdev, power);
	
	power->dev = &pdev->dev;
	power->dc_charger_gpio = pdata->dc_charger_gpio; 	
	power->dc_charger_irq = pdata->dc_charger_irq; 	
 
	/* Init the protection mutex */
	mutex_init(&power->lock);   
	
	/* Register power supplies */
	ret = power_supply_register(&pdev->dev, &shmcu_bat_psy);
	if (ret)
		goto err_ps_register;

	ret = power_supply_register(&pdev->dev, &shmcu_ac_psy);
	if (ret)
		goto err_ps_register2; 
		
		
	power->debug_file = shmcu_create_debugfs(power);
		
	power->work_queue = create_singlethread_workqueue("shmcu_power_wq");
	if (!power->work_queue) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "could not create workqueue\n");
		goto err_ps_register4;
	}
	INIT_DELAYED_WORK(&power->work, shmcu_work_func);
	
	power->isr_wq = create_singlethread_workqueue("shmcu_power_isr_wq");
	if (!power->isr_wq) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "could not create isr workqueue\n");
		goto err_ps_register5;
	}
	
	INIT_WORK(&power->isr_work, shmcu_isr_work_func);

	if (power->dc_charger_irq) {
		ret = request_irq(power->dc_charger_irq, shmcu_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "shmcu_power_dc_charger_isr", power);
		if (!ret) {
			power->use_irq = 1;
		} else {
			dev_err(&pdev->dev, "request_irq failed\n");
		}
	}
	
	/* Finally, get the chargergpio, if specified */
	if (power->dc_charger_gpio) {
		gpio_request(power->dc_charger_gpio,"DC charger");
		gpio_direction_input(power->dc_charger_gpio);
	}

	get_bat_mfg_data(power);
	
	/* The first time, we update the battery info as soon as possible */
	power->next_update = jiffies;
	queue_delayed_work(power->work_queue, &power->work,
		 msecs_to_jiffies(100));

	dev_info(&pdev->dev, "SHMCU power controller driver registered\n");
	
	return 0;
	
err_ps_register5:	
	cancel_delayed_work_sync(&power->work);
	destroy_workqueue(power->work_queue);
	
err_ps_register4:
	power_supply_unregister(&shmcu_ac_psy);
	
err_ps_register2:	
	power_supply_unregister(&shmcu_bat_psy);
	
err_ps_register:
	
	mutex_destroy(&power->lock); 
	kfree(power);

	return ret; 	
}

static int __devexit shmcu_power_remove(struct platform_device *dev)
{
	struct shmcu_power_ctx *power = platform_get_drvdata(dev);
	
	if (power->use_irq)
		free_irq(power->dc_charger_irq,power);
		
	if (power->dc_charger_gpio)
		gpio_free(power->dc_charger_gpio);
		
	destroy_workqueue(power->isr_wq);
	cancel_delayed_work_sync(&power->work);
	destroy_workqueue(power->work_queue);
	shmcu_remove_debugfs(power);
	
	power_supply_unregister(&shmcu_bat_psy);
	power_supply_unregister(&shmcu_ac_psy);
	mutex_destroy(&power->lock); 
	kfree(power);

	return 0;
}

static int shmcu_power_resume(struct platform_device *dev)
{
	struct shmcu_power_ctx *power = platform_get_drvdata(dev);
		
	power->next_update = jiffies;
	queue_delayed_work(power->work_queue, &power->work,
		msecs_to_jiffies(SHMCU_POWER_POLLING_INTERVAL));
	if (power->use_irq)
		enable_irq(power->dc_charger_irq);
	return 0;
}

static int shmcu_power_suspend(struct platform_device *dev, pm_message_t mesg)
{
	struct shmcu_power_ctx *power = platform_get_drvdata(dev);
	if (power->use_irq)
		disable_irq_nosync(power->dc_charger_irq);
	cancel_delayed_work_sync(&power->work);
	return 0;
} 

static struct platform_driver shmcu_power_driver = {
	.probe = shmcu_power_probe,
	.remove = __devexit_p(shmcu_power_remove), 
	.suspend = shmcu_power_suspend,
	.resume = shmcu_power_resume, 	
	.driver = {
		.name = "shmcu-power",
		.owner = THIS_MODULE,
	}
};

static int __init shmcu_power_init(void)
{
	return platform_driver_register(&shmcu_power_driver);
}

static void __exit shmcu_power_exit(void)
{
	platform_driver_unregister(&shmcu_power_driver);
} 

module_init(shmcu_power_init);
module_exit(shmcu_power_exit);

MODULE_AUTHOR("Eduardo José Tagle <ejtagle@tutopia.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SHMCU battery and AC driver");
MODULE_ALIAS("platform:shmcu-power");
