/*
 *  shiri_vibrator.c - DC Motor
 *
 * Copyright (C) 2011 Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include "../staging/android/timed_output.h"
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/of_gpio.h>

#include <mach/gpio.h>
#include <plat/gpio-cfg.h>

//#define DEBUG

struct vibrator_platform_data {
	int pwm_ch;
	int max_timeout;
};

struct vibrator_drvdata {
	struct platform_device *pdev;
	struct vibrator_platform_data *pdata;
	struct hrtimer timer;
	struct timed_output_dev dev;
	struct work_struct work;
	struct mutex lock;
	struct wake_lock wklock;
	struct pwm_device *pwm;
	bool running;
	int timeout;
	bool vibrate;
};

#define	VIBRATOR_PWR_ON		0x1
#define VIBRATOR_PWR_OFF	0x0
#define VIBRATOR_MIN_TIMEOUT	50

#define PWM_DUTY			900
#define PWM_PERIOD			38675

static void vibrator_power(int onoff)
{
	struct regulator *regulator = NULL;
	int rc=0;

	regulator = regulator_get(NULL, "vcc_peri_3.3");
	if(regulator == NULL) {
 		pr_err("vibrator regulator get error\n");
 		return;
 	} else {
		pr_info("vibrator regulator turn on\n");
	}
	if(onoff == VIBRATOR_PWR_ON)
		rc = regulator_enable(regulator);
	else
		rc = regulator_disable(regulator);

	if (rc < 0) {
		pr_err("%s: regulator %sable fail %d\n", __func__,
				onoff ? "en" : "dis", rc);

		goto regs_fail;
	}

regs_fail:
	regulator_put(regulator);

	return;
}

static void vibrator_set(struct vibrator_drvdata *ddata, int enable)
{
	u32 duty_ns = 0;
	int rc = 0;

	if (enable) {
#ifdef DEBUG
		pr_err("[VIB]: %s: pwm_enable\n", __func__);
#endif
		duty_ns = (u32)(PWM_PERIOD * PWM_DUTY / 1000);
		rc = pwm_config(ddata->pwm, duty_ns, PWM_PERIOD);
		if (rc)
			pr_err("%s: pwm_config fail\n", __func__);

		rc = pwm_enable(ddata->pwm);
		if (rc)
			pr_err("%s: pwm_enable fail\n", __func__);
	} else {
#ifdef DEBUG
		pr_err("[VIB]: %s: pwm_disable\n", __func__);
#endif
		pwm_disable(ddata->pwm);
	}
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *_timer)
{
	struct vibrator_drvdata *ddata =
		container_of(_timer, struct vibrator_drvdata, timer);

	ddata->timeout = 0;

	schedule_work(&ddata->work);
	return HRTIMER_NORESTART;
}

static void vibrator_work(struct work_struct *_work)
{
	struct vibrator_drvdata *ddata =
		container_of(_work, struct vibrator_drvdata, work);

	if (0 == ddata->timeout) {
		if (!ddata->running)
			return ;

		ddata->running = false;
		vibrator_set(ddata, false);
	} else {
		if (ddata->running)
			return ;

		ddata->running = true;
		vibrator_set(ddata, true);
	}
}

static int vibrator_get_time(struct timed_output_dev *_dev)
{
	struct vibrator_drvdata *ddata =
		container_of(_dev, struct vibrator_drvdata, dev);

	if (hrtimer_active(&ddata->timer)) {
		ktime_t r = hrtimer_get_remaining(&ddata->timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

static void vibrator_enable(struct timed_output_dev *_dev, int timeout)
{
	struct vibrator_drvdata *ddata =
		container_of(_dev, struct vibrator_drvdata, dev);

#ifdef DEBUG
	printk(KERN_WARNING "[VIB] time = %dms\n", timeout);
#endif

	mutex_lock(&ddata->lock);
	cancel_work_sync(&ddata->work);
	hrtimer_cancel(&ddata->timer);
	ddata->timeout = timeout;
	schedule_work(&ddata->work);
	if (timeout > 0) {
		if (timeout > ddata->pdata->max_timeout)
			timeout = ddata->pdata->max_timeout;

		if (timeout < VIBRATOR_MIN_TIMEOUT)
			timeout = VIBRATOR_MIN_TIMEOUT;

		hrtimer_start(&ddata->timer,
			ns_to_ktime((u64)timeout * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	}
	mutex_unlock(&ddata->lock);
}

#ifdef CONFIG_OF
static int vibrator_parse_dt(struct device *dev,
			struct vibrator_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	int rc = 0;
//	printk(KERN_WARNING "%s\n", __func__);

	rc = of_property_read_u32(np, "samsung,pwm-ch", &pdata->pwm_ch);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read pwm channel\n");
		return rc;
	}

	rc = of_property_read_u32(np, "samsung,max-timeout", &pdata->max_timeout);
	if (rc) {
		dev_err(dev, "Unable to read max timeout\n");
		return rc;
	}

	printk(KERN_WARNING "%s pwm-ch:%d max-timeout:%d \n", __func__, pdata->pwm_ch, pdata->max_timeout);

	return 0;
}
#endif

static int vibrator_probe(struct platform_device *pdev)
{
	struct vibrator_drvdata *ddata;
	struct vibrator_platform_data *pdata;
	int ret;

	printk(KERN_WARNING "%s: %s registering\n", __func__, pdev->name);

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct vibrator_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		ret = vibrator_parse_dt(&pdev->dev, pdata);
		if (ret) {
			dev_err(&pdev->dev, "Parsing DT failed(%d)", ret);
			return ret;
		}
	} else
		pdata = pdev->dev.platform_data;

	if (!pdata) {
		dev_err(&pdev->dev, "%s: no platform data\n", __func__);
		return -EINVAL;
	}

	ddata = kzalloc(sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		return -ENOMEM;

	ddata->pdev = pdev;
	ddata->pdata = pdata;
	ddata->vibrate = false;

	ddata->pwm = pwm_request(pdata->pwm_ch, pdev->name);
	if (IS_ERR(ddata->pwm)) {
		pr_err("pwm request failed\n");
		ret = PTR_ERR(ddata->pwm);
		goto pwm_req_fail;
	}

	mutex_init(&ddata->lock);
	wake_lock_init(&ddata->wklock, WAKE_LOCK_SUSPEND, "vibrator");

	hrtimer_init(&ddata->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ddata->timer.function = vibrator_timer_func;

	/* register with timed output class */
	ddata->dev.name = "vibrator";
	ddata->dev.get_time = vibrator_get_time;
	ddata->dev.enable = vibrator_enable;
	ret = timed_output_dev_register(&ddata->dev);
	if (ret < 0) {
		pr_err("timed output register failed %d\n", ret);
		goto setup_fail;
	}

	vibrator_power(VIBRATOR_PWR_ON);		// PMIC LDO17 ON VCC_3.3V_PERI
	INIT_WORK(&ddata->work, vibrator_work);

	pr_debug("%s registered\n", pdev->name);
	return 0;

setup_fail:
	mutex_destroy(&ddata->lock);
	wake_lock_destroy(&ddata->wklock);
	pwm_free(ddata->pwm);
pwm_req_fail:
	kfree(ddata);
	return ret;
}

static int vibrator_remove(struct platform_device *pdev)
{
	struct vibrator_drvdata *ddata = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	timed_output_dev_unregister(&ddata->dev);
	hrtimer_cancel(&ddata->timer);
	mutex_destroy(&ddata->lock);

	wake_lock_destroy(&ddata->wklock);

	pwm_disable(ddata->pwm);
	pwm_free(ddata->pwm);

	kfree(ddata);

	return 0;
}

#ifdef CONFIG_PM
static int vibrator_suspend(struct device *dev)
{
	vibrator_power(VIBRATOR_PWR_OFF);
	return 0;
}

static int vibrator_resume(struct device *dev)
{
	vibrator_power(VIBRATOR_PWR_ON);
	return 0;
}

static SIMPLE_DEV_PM_OPS(vibrator_pm_ops,
			 vibrator_suspend, vibrator_resume);

#define SHIRI_VIBRATOR_PM_OPS (&vibrator_pm_ops)
#else
#define SHIRI_VIBRATOR_PM_OPS NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id vibrator_match[] = {
	{ .compatible = "samsung,shiri-vib", },
	{ },
};
#else
#define vibrator_match NULL
#endif

static struct platform_driver vibrator_driver = {
	.probe	= vibrator_probe,
	.remove = vibrator_remove,
	.driver = {
		.name	= "vibrator",
		.owner	= THIS_MODULE,
		.pm	= SHIRI_VIBRATOR_PM_OPS,
		.of_match_table = of_match_ptr(vibrator_match),
	},
};

static int __init vibrator_init(void)
{
	printk(KERN_WARNING "%s: %s\n", __func__, vibrator_driver.driver.name);
	return platform_driver_register(&vibrator_driver);
}

static void __exit vibrator_exit(void)
{
	platform_driver_unregister(&vibrator_driver);
}

module_init(vibrator_init);
module_exit(vibrator_exit);

MODULE_ALIAS("platform:shiri-vib");
MODULE_DESCRIPTION("Shiri Vibrator Driver");
MODULE_LICENSE("GPL");
