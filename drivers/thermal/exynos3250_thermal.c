/*
 * exynos3250_thermal.c - Samsung EXYNOS TMU (Thermal Management Unit)
 *
 *  Copyright (C) 2011 Samsung Electronics
 *  Donggeun Kim <dg77.kim@samsung.com>
 *  Amit Daniel Kachhap <amit.kachhap@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/platform_data/exynos_thermal.h>
#include <linux/thermal.h>
#include <linux/cpufreq.h>
#include <linux/cpu_cooling.h>
#include <linux/of.h>
#include <linux/suspend.h>
#include <linux/clk.h>
#include <plat/cpu.h>
#include <mach/tmu.h>
#include <mach/cpufreq.h>
#include <mach/exynos-pm.h>
#include "cal_tmu.h"

#define CS_POLICY_CORE		0

static int exynos3_tmu_cpufreq_notifier(struct notifier_block *notifier, unsigned long event, void *v);
static struct notifier_block exynos_cpufreq_nb = {
	.notifier_call = exynos3_tmu_cpufreq_notifier,
};

struct exynos_tmu_data {
	struct exynos_tmu_platform_data *pdata;
	struct resource *mem;
	void __iomem *base;
	int irq;
	struct work_struct irq_work;
	struct mutex lock;
	u8 temp_error1;
	u8 temp_error2;
	struct cal_tmu_data *cal_data;
	struct clk *clk;
};

struct	thermal_trip_point_conf {
	int trip_val[MAX_TRIP_COUNT];
	int trip_count;
	u8 trigger_falling;
};

struct	thermal_cooling_conf {
	struct freq_clip_table freq_data[MAX_TRIP_COUNT];
	int size[THERMAL_TRIP_CRITICAL + 1];
	int freq_clip_count;
};

struct thermal_sensor_conf {
	char name[SENSOR_NAME_LEN];
	int (*read_temperature)(void *data);
	int (*write_emul_temp)(void *drv_data, unsigned long temp);
	struct thermal_trip_point_conf trip_data;
	struct thermal_cooling_conf cooling_data;
	void *private_data;
};

struct exynos_thermal_zone {
	enum thermal_device_mode mode;
	struct thermal_zone_device *therm_dev;
	struct thermal_cooling_device *cool_dev[MAX_COOLING_DEVICE];
	unsigned int cool_dev_size;
	struct thermal_sensor_conf *sensor_conf;
	bool bind;
};

static struct platform_device *exynos_tmu_pdev;
static struct exynos_tmu_data *tmudata;
static struct exynos_thermal_zone *th_zone;

/* Get mode callback functions for thermal zone */
static int exynos_get_mode(struct thermal_zone_device *thermal,
			enum thermal_device_mode *mode)
{
	if (th_zone)
		*mode = th_zone->mode;
	return 0;
}

/* Set mode callback functions for thermal zone */
static int exynos_set_mode(struct thermal_zone_device *thermal,
			enum thermal_device_mode mode)
{
	if (!th_zone->therm_dev) {
		pr_notice("thermal zone not registered\n");
		return 0;
	}

	th_zone->mode = mode;
	thermal_zone_device_update(th_zone->therm_dev);
	return 0;
}


/* Get trip type callback functions for thermal zone */
static int exynos_get_trip_type(struct thermal_zone_device *thermal, int trip,
				 enum thermal_trip_type *type)
{
	int active_size, passive_size;

	active_size = th_zone->sensor_conf->cooling_data.size[THERMAL_TRIP_ACTIVE];
	passive_size = th_zone->sensor_conf->cooling_data.size[THERMAL_TRIP_PASSIVE];

	if (trip < active_size)
		*type = THERMAL_TRIP_ACTIVE;
	else if (trip >= active_size && trip < active_size + passive_size)
		*type = THERMAL_TRIP_PASSIVE;
	else if (trip >= active_size + passive_size)
		*type = THERMAL_TRIP_CRITICAL;
	else
		return -EINVAL;

	return 0;
}

/* Get trip temperature callback functions for thermal zone */
static int exynos_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				unsigned long *temp)
{
	int active_size, passive_size;

	active_size = th_zone->sensor_conf->cooling_data.size[THERMAL_TRIP_ACTIVE];
	passive_size = th_zone->sensor_conf->cooling_data.size[THERMAL_TRIP_PASSIVE];

	if (trip < 0 || trip > active_size + passive_size)
		return -EINVAL;

	*temp = th_zone->sensor_conf->trip_data.trip_val[trip];
	/* convert the temperature into millicelsius */
	*temp = *temp * MCELSIUS;

	return 0;
}

/* Get critical temperature callback functions for thermal zone */
static int exynos_get_crit_temp(struct thermal_zone_device *thermal,
				unsigned long *temp)
{
	int ret;
	int active_size, passive_size;

	active_size = th_zone->sensor_conf->cooling_data.size[THERMAL_TRIP_ACTIVE];
	passive_size = th_zone->sensor_conf->cooling_data.size[THERMAL_TRIP_PASSIVE];

	/* Panic zone */
	ret = exynos_get_trip_temp(thermal, active_size + passive_size, temp);
	return ret;
}

/* Bind callback functions for thermal zone */
static int exynos_bind(struct thermal_zone_device *thermal,
			struct thermal_cooling_device *cdev)
{
	int ret = 0, i, tab_size, level = THERMAL_CSTATE_INVALID;
	struct freq_clip_table *tab_ptr, *clip_data;
	struct thermal_sensor_conf *data = th_zone->sensor_conf;
	enum thermal_trip_type type = 0;

	tab_ptr = (struct freq_clip_table *)data->cooling_data.freq_data;
	tab_size = data->cooling_data.freq_clip_count;

	if (tab_ptr == NULL || tab_size == 0)
		return -EINVAL;

	/* find the cooling device registered*/
	for (i = 0; i < th_zone->cool_dev_size; i++)
		if (cdev == th_zone->cool_dev[i]) {
			break;
		}

	/* No matching cooling device */
	if (i == th_zone->cool_dev_size)
		return 0;

	/* Bind the thermal zone to the cpufreq cooling device */
	for (i = 0; i < tab_size; i++) {
		clip_data = (struct freq_clip_table *)&(tab_ptr[i]);
		level = cpufreq_cooling_get_level(CS_POLICY_CORE, clip_data->freq_clip_max);
		if (level == THERMAL_CSTATE_INVALID) {
			thermal->cooling_dev_en = false;
			return 0;
		}
		exynos_get_trip_type(th_zone->therm_dev, i, &type);
		switch (type) {
		case THERMAL_TRIP_ACTIVE:
		case THERMAL_TRIP_PASSIVE:
			if (thermal_zone_bind_cooling_device(thermal, i, cdev,
								level, 0)) {
				pr_err("error binding cdev inst %d\n", i);
				thermal->cooling_dev_en = false;
				ret = -EINVAL;
			}
			th_zone->bind = true;
			break;
		default:
			ret = -EINVAL;
		}
	}

	return ret;
}

/* Unbind callback functions for thermal zone */
static int exynos_unbind(struct thermal_zone_device *thermal,
			struct thermal_cooling_device *cdev)
{
	int ret = 0, i, tab_size;
	struct thermal_sensor_conf *data = th_zone->sensor_conf;
	enum thermal_trip_type type = 0;

	if (th_zone->bind == false)
		return 0;

	tab_size = data->cooling_data.freq_clip_count;

	if (tab_size == 0)
		return -EINVAL;

	/* find the cooling device registered*/
	for (i = 0; i < th_zone->cool_dev_size; i++)
		if (cdev == th_zone->cool_dev[i])
			break;

	/* No matching cooling device */
	if (i == th_zone->cool_dev_size)
		return 0;

	/* Bind the thermal zone to the cpufreq cooling device */
	for (i = 0; i < tab_size; i++) {
		exynos_get_trip_type(th_zone->therm_dev, i, &type);
		switch (type) {
		case THERMAL_TRIP_ACTIVE:
		case THERMAL_TRIP_PASSIVE:
			if (thermal_zone_unbind_cooling_device(thermal, i,
								cdev)) {
				pr_err("error unbinding cdev inst=%d\n", i);
				ret = -EINVAL;
			}
			th_zone->bind = false;
			break;
		default:
			ret = -EINVAL;
		}
	}
	return ret;
}

/* Get temperature callback functions for thermal zone */
static int exynos_get_temp(struct thermal_zone_device *thermal,
			unsigned long *temp)
{
	void *data;

	if (!th_zone->sensor_conf) {
		pr_info("Temperature sensor not initialised\n");
		return -EINVAL;
	}
	data = th_zone->sensor_conf->private_data;
	*temp = th_zone->sensor_conf->read_temperature(data);
	/* convert the temperature into millicelsius */
	*temp = *temp * MCELSIUS;
	return 0;
}

/* Get the temperature trend */
static int exynos_get_trend(struct thermal_zone_device *thermal,
			int trip, enum thermal_trend *trend)
{
	int ret;
	unsigned long trip_temp;

	ret = exynos_get_trip_temp(thermal, trip, &trip_temp);
	if (ret < 0)
		return ret;

	if (thermal->temperature >= trip_temp)
		*trend = THERMAL_TREND_RAISE_FULL;
	else
		*trend = THERMAL_TREND_DROP_FULL;

	return 0;
}

/* Operation callback functions for thermal zone */
static struct thermal_zone_device_ops const exynos_dev_ops = {
	.bind = exynos_bind,
	.unbind = exynos_unbind,
	.get_temp = exynos_get_temp,
	.get_trend = exynos_get_trend,
	.get_mode = exynos_get_mode,
	.set_mode = exynos_set_mode,
	.get_trip_type = exynos_get_trip_type,
	.get_trip_temp = exynos_get_trip_temp,
	.get_crit_temp = exynos_get_crit_temp,
};

/*
 * This function may be called from interrupt based temperature sensor
 * when threshold is changed.
 */
static void exynos_report_trigger(void)
{
	unsigned int i;
	char data[10];
	char *envp[] = { data, NULL };
	enum thermal_trip_type type = 0;

	if (!th_zone || !th_zone->therm_dev)
		return;
	if (th_zone->bind == false) {
		for (i = 0; i < th_zone->cool_dev_size; i++) {
			if (!th_zone->cool_dev[i])
				continue;
			exynos_bind(th_zone->therm_dev,
					th_zone->cool_dev[i]);
		}
	}

	thermal_zone_device_update(th_zone->therm_dev);

	mutex_lock(&th_zone->therm_dev->lock);
	/* Find the level for which trip happened */
	for (i = 0; i < th_zone->sensor_conf->trip_data.trip_count; i++) {
		if (th_zone->therm_dev->last_temperature <
			th_zone->sensor_conf->trip_data.trip_val[i] * MCELSIUS)
			break;
	}

	if (th_zone->mode == THERMAL_DEVICE_ENABLED) {
		exynos_get_trip_type(th_zone->therm_dev, i, &type);
		if (type == THERMAL_TRIP_ACTIVE)
			th_zone->therm_dev->passive_delay = ACTIVE_INTERVAL;
		else
			th_zone->therm_dev->passive_delay = PASSIVE_INTERVAL;
	}

	snprintf(data, sizeof(data), "%u", i);
	kobject_uevent_env(&th_zone->therm_dev->device.kobj, KOBJ_CHANGE, envp);
	mutex_unlock(&th_zone->therm_dev->lock);
}

/* Un-Register with the in-kernel thermal management */
static void exynos_unregister_thermal(void)
{
	int i;

	if (!th_zone)
		return;

	if (th_zone->therm_dev)
		thermal_zone_device_unregister(th_zone->therm_dev);

	for (i = 0; i < th_zone->cool_dev_size; i++) {
		if (th_zone->cool_dev[i])
			cpufreq_cooling_unregister(th_zone->cool_dev[i]);
	}

	kfree(th_zone);
	pr_info("Exynos: Kernel Thermal management unregistered\n");
}

/* Register with the in-kernel thermal management */
static int exynos_register_thermal(struct thermal_sensor_conf *sensor_conf)
{
	int ret, count = 0;
	struct cpumask mask_val;

	if (!sensor_conf || !sensor_conf->read_temperature) {
		pr_err("Temperature sensor not initialised\n");
		return -EINVAL;
	}

	th_zone = kzalloc(sizeof(struct exynos_thermal_zone), GFP_KERNEL);
	if (!th_zone)
		return -ENOMEM;

	th_zone->sensor_conf = sensor_conf;
	cpumask_clear(&mask_val);
	cpumask_set_cpu(0, &mask_val);

	for (count = 0; count < EXYNOS_ZONE_COUNT; count++) {
		th_zone->cool_dev[count] = cpufreq_cooling_register(&mask_val);
		if (IS_ERR(th_zone->cool_dev[count])) {
			 pr_err("Failed to register cpufreq cooling device\n");
			 ret = -EINVAL;
			 th_zone->cool_dev_size = count;
			 goto err_unregister;
		 }
	}

	th_zone->cool_dev_size = count;

	th_zone->therm_dev = thermal_zone_device_register(sensor_conf->name,
			th_zone->sensor_conf->trip_data.trip_count, 0, NULL, &exynos_dev_ops, NULL, PASSIVE_INTERVAL,
			IDLE_INTERVAL);

	if (IS_ERR(th_zone->therm_dev)) {
		pr_err("Failed to register thermal zone device\n");
		ret = PTR_ERR(th_zone->therm_dev);
		goto err_unregister;
	}
	th_zone->mode = THERMAL_DEVICE_ENABLED;

	pr_info("Exynos: Kernel Thermal management registered\n");

	return 0;

err_unregister:
	exynos_unregister_thermal();
	return ret;
}

/*
 * TMU treats temperature as a mapped temperature code.
 * The temperature is converted differently depending on the calibration type.
 */
static int temp_to_code(struct exynos_tmu_data *data, u8 temp)
{
	struct exynos_tmu_platform_data *pdata = data->pdata;
	int temp_code;

	if (temp > MAX_TEMP)
		temp_code = MAX_TEMP;
	else if (temp < MIN_TEMP)
		temp_code = MIN_TEMP;

	switch (pdata->cal_type) {
	case TYPE_TWO_POINT_TRIMMING:
		temp_code = (temp - 25) *
		    (data->temp_error2 - data->temp_error1) /
		    (85 - 25) + data->temp_error1;
		break;
	case TYPE_ONE_POINT_TRIMMING:
		temp_code = temp + data->temp_error1 - 25;
		break;
	default:
		temp_code = temp + EXYNOS_TMU_DEF_CODE_TO_TEMP_OFFSET;
		break;
	}

	return temp_code;
}

static int exynos_tmu_initialize(struct platform_device *pdev, int id)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct exynos_tmu_platform_data *pdata = data->pdata;
	unsigned int status;
	unsigned int rising_threshold = 0, falling_threshold = 0;
	int ret = 0, threshold_code, i, trigger_levs = 0;
	int timeout = 20000;

	mutex_lock(&data->lock);
	clk_enable(data->clk);

	while (1) {
		status = readb(data->base + EXYNOS_TMU_REG_STATUS);
		if (status)
			break;

		timeout--;
		if (!timeout) {
			pr_err("%s: timeout TMU busy\n", __func__);
			ret = -EBUSY;
			goto out;
		}

		cpu_relax();
		usleep_range(1, 2);
	};

	/* Count trigger levels to be enabled */
	for (i = 0; i < MAX_THRESHOLD_LEVS; i++)
		if (pdata->trigger_levels[i])
			trigger_levs++;

	/* Write temperature code for rising and falling threshold */
	for (i = 0; i < trigger_levs; i++) {
		threshold_code = temp_to_code(data,
				pdata->trigger_levels[i]);
		if (threshold_code < 0) {
			ret = threshold_code;
			goto out;
		}
		rising_threshold |= threshold_code << 8 * i;
		if (pdata->threshold_falling) {
			threshold_code = temp_to_code(data,
					pdata->trigger_levels[i] -
					pdata->threshold_falling);
			if (threshold_code > 0)
				falling_threshold |=
					threshold_code << 8 * i;
		}
	}

	writel(rising_threshold, data->base + EXYNOS_THD_TEMP_RISE);
	writel(falling_threshold, data->base + EXYNOS_THD_TEMP_FALL);
	writel(EXYNOS_TMU_CLEAR_RISE_INT | EXYNOS_TMU_CLEAR_FALL_INT, data->base + EXYNOS_TMU_REG_INTCLEAR);
out:
	clk_disable(data->clk);
	mutex_unlock(&data->lock);

	return ret;
}

static void exynos_tmu_get_efuse(struct platform_device *pdev, int id)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct exynos_tmu_platform_data *pdata = data->pdata;
	unsigned int trim_info;
	int timeout = 5;

	mutex_lock(&data->lock);
	clk_enable(data->clk);

	__raw_writel(EXYNOS_TRIMINFO_RELOAD1,
			data->base + EXYNOS_TRIMINFO_CONFIG);
	__raw_writel(EXYNOS_TRIMINFO_RELOAD2,
			data->base + EXYNOS_TRIMINFO_CONTROL);
	while (readl(data->base + EXYNOS_TRIMINFO_CONTROL) & EXYNOS_TRIMINFO_RELOAD1) {
		if (!timeout) {
			pr_err("Thermal TRIMINFO register reload failed\n");
			break;
		}
		timeout--;
		cpu_relax();
		usleep_range(5, 10);
	}

	/* Save trimming info in order to perform calibration */
	trim_info = readl(data->base + EXYNOS_TMU_REG_TRIMINFO);
	data->temp_error1 = trim_info & EXYNOS_TMU_TRIM_TEMP_MASK;
	data->temp_error2 = ((trim_info >> 8) & EXYNOS_TMU_TRIM_TEMP_MASK);

	if ((EFUSE_MIN_VALUE > data->temp_error1) || (data->temp_error1 > EFUSE_MAX_VALUE) ||
			(data->temp_error1 == 0))
		data->temp_error1 = pdata->efuse_value;

	clk_disable(data->clk);
	mutex_unlock(&data->lock);
}

static void exynos_tmu_control(struct platform_device *pdev, int id, bool on)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	mutex_lock(&data->lock);
	clk_enable(data->clk);
	cal_tmu_control(data->cal_data, id, on);
	clk_disable(data->clk);
	mutex_unlock(&data->lock);
}

static int exynos_tmu_read(struct exynos_tmu_data *data)
{
	int temp, max = INT_MIN, min = INT_MAX;

	mutex_lock(&data->lock);
	clk_enable(data->clk);
	temp = cal_tmu_read(data->cal_data, 0);

	if (temp > max)
		max = temp;
	if (temp < min)
		min = temp;

	clk_disable(data->clk);
	mutex_unlock(&data->lock);

	return max;
}

static void exynos_tmu_work(struct work_struct *work)
{
	struct exynos_tmu_data *data = container_of(work,
			struct exynos_tmu_data, irq_work);

	mutex_lock(&data->lock);
	clk_enable(data->clk);
	writel(EXYNOS_TMU_CLEAR_RISE_INT | EXYNOS_TMU_CLEAR_FALL_INT,
			data->base + EXYNOS_TMU_REG_INTCLEAR);
	clk_disable(data->clk);
	mutex_unlock(&data->lock);
	exynos_report_trigger();
	enable_irq(data->irq);
}

static irqreturn_t exynos_tmu_irq(int irq, void *id)
{
	struct exynos_tmu_data *data = id;

	pr_debug("[TMUIRQ] irq = %d\n", irq);

	disable_irq_nosync(data->irq);
	schedule_work(&data->irq_work);

	return IRQ_HANDLED;
}
static struct thermal_sensor_conf exynos_sensor_conf = {
	.name			= "exynos-therm",
	.read_temperature	= (int (*)(void *))exynos_tmu_read,
};

#if defined(CONFIG_SOC_EXYNOS3250)
static struct exynos_tmu_platform_data exynos3_tmu_data = {
	.threshold_falling = 2,
	.trigger_levels[0] = 80,
	.trigger_levels[1] = 85,
	.trigger_levels[2] = 95,
	.trigger_levels[3] = 105,
	.trigger_level0_en = 1,
	.trigger_level1_en = 1,
	.trigger_level2_en = 1,
	.trigger_level3_en = 1,
	.gain = 8,
	.reference_voltage = 16,
	.noise_cancel_mode = 0,
	.cal_type = TYPE_ONE_POINT_TRIMMING,
	.efuse_value = 55,
	.freq_tab[0] = {
		.freq_clip_max = 900 * 1000,
		.temp_level = 80,
	},
	.freq_tab[1] = {
		.freq_clip_max = 800 * 1000,
		.temp_level = 85,
	},
	.freq_tab[2] = {
		.freq_clip_max = 700 * 1000,
		.temp_level = 95,
	},
	.freq_tab[3] = {
		.freq_clip_max = 600 * 1000,
		.temp_level = 105,
	},
	.size[THERMAL_TRIP_ACTIVE] = 3,
	.size[THERMAL_TRIP_PASSIVE] = 1,
	.freq_tab_count = 4,
};
#define EXYNOS3250_TMU_DRV_DATA (&exynos3_tmu_data)
#else
#define EXYNOS3250_TMU_DRV_DATA (NULL)
#endif

#ifdef CONFIG_OF
static const struct of_device_id exynos_tmu_match[] = {
	{
		.compatible = "samsung,exynos3250-tmu",
		.data = (void *)EXYNOS3250_TMU_DRV_DATA,
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_tmu_match);
#endif

static inline struct  exynos_tmu_platform_data *exynos_get_driver_data(
			struct platform_device *pdev)
{
#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(exynos_tmu_match, pdev->dev.of_node);
		if (!match)
			return NULL;
		return (struct exynos_tmu_platform_data *) match->data;
	}
#endif
	return (struct exynos_tmu_platform_data *)
			platform_get_device_id(pdev)->driver_data;
}

static void exynos_set_cal_data(struct exynos_tmu_data *data)
{
	data->cal_data->base[0] = data->base;
	data->cal_data->temp_error1[0] = data->temp_error1;
	data->cal_data->temp_error2[0] = data->temp_error2;

	data->cal_data->gain = data->pdata->gain;
	data->cal_data->reference_voltage = data->pdata->reference_voltage;
	data->cal_data->noise_cancel_mode = data->pdata->noise_cancel_mode;
	data->cal_data->cal_type = data->pdata->cal_type;

	data->cal_data->trigger_level_en[0] = data->pdata->trigger_level0_en;
	data->cal_data->trigger_level_en[1] = data->pdata->trigger_level1_en;
	data->cal_data->trigger_level_en[2] = data->pdata->trigger_level2_en;
	data->cal_data->trigger_level_en[3] = data->pdata->trigger_level3_en;
	data->cal_data->trigger_level_en[4] = data->pdata->trigger_level4_en;
	data->cal_data->trigger_level_en[5] = data->pdata->trigger_level5_en;
	data->cal_data->trigger_level_en[6] = data->pdata->trigger_level6_en;
	data->cal_data->trigger_level_en[7] = data->pdata->trigger_level7_en;
}

static void exynos_tmu_regdump(struct platform_device *pdev, int id)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	unsigned int reg_data;

	mutex_lock(&data->lock);

	reg_data = readl(data->base + EXYNOS_TMU_REG_TRIMINFO);
	pr_info("TRIMINFO[%d] = 0x%x\n", id, reg_data);
	reg_data = readl(data->base + EXYNOS_TMU_REG_CONTROL);
	pr_info("TMU_CONTROL[%d] = 0x%x\n", id, reg_data);
	reg_data = readl(data->base + EXYNOS_TMU_REG_CURRENT_TEMP);
	pr_info("CURRENT_TEMP[%d] = 0x%x\n", id, reg_data);
	reg_data = readl(data->base + EXYNOS_THD_TEMP_RISE);
	pr_info("THRESHOLD_TEMP_RISE[%d] = 0x%x\n", id, reg_data);
	reg_data = readl(data->base + EXYNOS_THD_TEMP_FALL);
	pr_info("THRESHOLD_TEMP_FALL[%d] = 0x%x\n", id, reg_data);
	reg_data = readl(data->base + EXYNOS_TMU_REG_INTEN);
	pr_info("INTEN[%d] = 0x%x\n", id, reg_data);
	reg_data = readl(data->base + EXYNOS_TMU_REG_INTCLEAR);
	pr_info("INTCLEAR[%d] = 0x%x\n", id, reg_data);

	mutex_unlock(&data->lock);
}

static int exynos3_tmu_cpufreq_notifier(struct notifier_block *notifier, unsigned long event, void *v)
{
	int ret = 0;

	switch (event) {
	case CPUFREQ_INIT_COMPLETE:
		ret = exynos_register_thermal(&exynos_sensor_conf);

		if (ret) {
			dev_err(&exynos_tmu_pdev->dev, "Failed to register thermal interface\n");
			exynos_cpufreq_init_unregister_notifier(&exynos_cpufreq_nb);
			platform_set_drvdata(exynos_tmu_pdev, NULL);

			if (tmudata->irq)
				free_irq(tmudata->irq, tmudata);
			kfree(tmudata);

			return ret;
		}
		break;
	}
	return 0;
}

static int exynos_tmu_probe(struct platform_device *pdev)
{
	struct exynos_tmu_data *data;
	struct exynos_tmu_platform_data *pdata = pdev->dev.platform_data;
	int ret, i;

	exynos_tmu_pdev = pdev;

	if (!pdata)
		pdata = exynos_get_driver_data(pdev);

	if (!pdata) {
		dev_err(&pdev->dev, "No platform init data supplied.\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(struct exynos_tmu_data),
					GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "Failed to allocate driver structure\n");
		return -ENOMEM;
	}

	data->cal_data = devm_kzalloc(&pdev->dev, sizeof(struct cal_tmu_data),
					GFP_KERNEL);
	if (!data->cal_data) {
		dev_err(&pdev->dev, "Failed to allocate cal data structure\n");
		return -ENOMEM;
	}

	exynos_cpufreq_init_register_notifier(&exynos_cpufreq_nb);
	INIT_WORK(&data->irq_work, exynos_tmu_work);

	data->clk = devm_clk_get(&pdev->dev, "tmu_apbif");
        if (IS_ERR(data->clk)) {
                dev_err(&pdev->dev, "Failed to get clock\n");
                return  PTR_ERR(data->clk);
        }

	ret = clk_prepare(data->clk);
        if (ret) {
                dev_err(&pdev->dev, "Failed to prepare clock\n");
                return ret;
        }

	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0) {
		ret = data->irq;
		dev_err(&pdev->dev, "Failed to get platform irq\n");
		goto err_get_irq;
	}

	ret = request_irq(data->irq, exynos_tmu_irq,
			IRQF_TRIGGER_RISING, "exynos_tmu", data);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq: %d\n", data->irq);
		goto err_request_irq;
	}

	data->mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!data->mem) {
		ret = -ENOENT;
		dev_err(&pdev->dev, "Failed to get platform resource\n");
		goto err_get_resource;
	}

	data->base = devm_request_and_ioremap(&pdev->dev, data->mem);
	if (IS_ERR(data->base)) {
		ret = PTR_ERR(data->base);
		dev_err(&pdev->dev, "Failed to ioremap memory\n");
		goto err_io_remap;
	}

	data->pdata = pdata;
	tmudata = data;
	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);

	/* Save the eFuse value before initializing TMU */
	exynos_tmu_get_efuse(pdev, 0);

	exynos_set_cal_data(data);

	ret = exynos_tmu_initialize(pdev, 0);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize TMU\n");
		goto err_tmu;
	}

	exynos_tmu_control(pdev, 0, true);
	exynos_tmu_regdump(pdev, 0);

	/* Register the sensor with thermal management interface */
	(&exynos_sensor_conf)->private_data = data;
	exynos_sensor_conf.trip_data.trip_count = pdata->trigger_level0_en +
			pdata->trigger_level1_en + pdata->trigger_level2_en +
						pdata->trigger_level3_en;

	for (i = 0; i < exynos_sensor_conf.trip_data.trip_count; i++) {
		exynos_sensor_conf.trip_data.trip_val[i] =
			pdata->threshold + pdata->trigger_levels[i];
	}

	exynos_sensor_conf.trip_data.trigger_falling = pdata->threshold_falling;

	exynos_sensor_conf.cooling_data.freq_clip_count =
						pdata->freq_tab_count;
	for (i = 0; i < pdata->freq_tab_count; i++) {
		exynos_sensor_conf.cooling_data.freq_data[i].freq_clip_max =
					pdata->freq_tab[i].freq_clip_max;
		exynos_sensor_conf.cooling_data.freq_data[i].temp_level =
					pdata->freq_tab[i].temp_level;
		if (pdata->freq_tab[i].mask_val) {
			exynos_sensor_conf.cooling_data.freq_data[i].mask_val =
				pdata->freq_tab[i].mask_val;
		} else
			exynos_sensor_conf.cooling_data.freq_data[i].mask_val =
				cpu_all_mask;
	}

	exynos_sensor_conf.cooling_data.size[THERMAL_TRIP_ACTIVE] = pdata->size[THERMAL_TRIP_ACTIVE];
	exynos_sensor_conf.cooling_data.size[THERMAL_TRIP_PASSIVE] = pdata->size[THERMAL_TRIP_PASSIVE];

	return 0;

err_tmu:
	platform_set_drvdata(pdev, NULL);
err_io_remap:
err_get_resource:
	if (data->irq)
		free_irq(data->irq, data);
err_request_irq:
err_get_irq:
	clk_unprepare(data->clk);
	return ret;
}

static int exynos_tmu_remove(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	exynos_tmu_control(pdev, 0, false);

	exynos_unregister_thermal();

	clk_unprepare(data->clk);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int exynos_tmu_suspend(struct device *dev)
{
	exynos_tmu_control(to_platform_device(dev), 0, false);

	return 0;
}

static int exynos_tmu_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	exynos_tmu_get_efuse(pdev, 0);
	exynos_set_cal_data(data);
	exynos_tmu_initialize(pdev, 0);
	exynos_tmu_control(pdev, 0, true);
	exynos_tmu_regdump(pdev, 0);

	return 0;
}

static SIMPLE_DEV_PM_OPS(exynos_tmu_pm,
			 exynos_tmu_suspend, exynos_tmu_resume);
#define EXYNOS_TMU_PM	(&exynos_tmu_pm)
#else
#define EXYNOS_TMU_PM	NULL
#endif

static struct platform_driver exynos_tmu_driver = {
	.driver = {
		.name   = "exynos-tmu",
		.owner  = THIS_MODULE,
		.pm     = EXYNOS_TMU_PM,
		.of_match_table = of_match_ptr(exynos_tmu_match),
	},
	.probe = exynos_tmu_probe,
	.remove	= exynos_tmu_remove,
};

module_platform_driver(exynos_tmu_driver);

MODULE_DESCRIPTION("EXYNOS TMU Driver");
MODULE_AUTHOR("Donggeun Kim <dg77.kim@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:exynos-tmu");
