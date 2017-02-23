/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/suspend.h>
#include <linux/opp.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/devfreq.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/reboot.h>
#include <linux/kobject.h>
#include <linux/clk.h>
#include <linux/mutex.h>

#include <mach/regs-clock.h>
#include <mach/devfreq.h>
#include <mach/asv-exynos.h>

#include "exynos3250_ppmu.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define SAFE_INT_VOLT(x)	(x + 25000)
#define INT_TIMEOUT_VAL		10000
#define MAX_INTFREQ (CONFIG_EXYNOS3250_MAX_INTFREQ * 1000)

static struct device *int_dev;
static struct pm_qos_request exynos3250_int_qos;
static struct pm_qos_request exynos3250_miflock_qos;
static struct pm_qos_request exynos3250_boot_int_qos;
cputime64_t int_pre_time;

static LIST_HEAD(int_dvfs_list);

struct devfreq_data_int {
	struct device *dev;
	struct devfreq *devfreq;
	struct opp *curr_opp;
	struct regulator *vdd_int;
	struct exynos3250_ppmu_handle *ppmu;
	struct mutex lock;
};

enum int_bus_idx {
	LV_0,
	LV_1,
	LV_2,
	LV_3,
	LV_4,
	LV_5,
	LV_END,
};

struct int_bus_opp_table {
	unsigned int idx;
	unsigned long clk;
	unsigned long volt;
	cputime64_t time_in_state;
};

struct int_bus_opp_table int_bus_opp_list[] = {
	{LV_0, 135000, 1000000, 0},
	{LV_1, 134000, 1000000, 0},
	{LV_2, 133000, 1000000, 0},
	{LV_3, 100000, 1000000, 0},
	{LV_4,  80000,  900000, 0},
	{LV_5,  50000,  900000, 0},
};

struct int_regs_value {
	void __iomem *target_reg;
	unsigned int reg_value;
	unsigned int reg_mask;
	void __iomem *wait_reg;
	unsigned int wait_mask;
};

struct int_clkdiv_info {
	struct list_head list;
	unsigned int lv_idx;
	struct int_regs_value lbus;
	struct int_regs_value rbus;
	struct int_regs_value top;
	struct int_regs_value acp0;
	struct int_regs_value acp1;
	struct int_regs_value mfc;
};

static unsigned int exynos3250_int_lbus_div[][2] = {
/* ACLK_GDL, ACLK_GPL */
	{2, 1},		/* L0 */
	{2, 1},		/* L1 */
	{2, 1},		/* L2 */
	{3, 1},		/* L3 */
	{3, 1},		/* L4 */
	{3, 1},		/* L5 */
};

static unsigned int exynos3250_int_rbus_div[][2] = {
/* ACLK_GDR, ACLK_GPR */
	{2, 1},		/* L0 */
	{2, 1},		/* L1 */
	{2, 1},		/* L2 */
	{3, 1},		/* L3 */
	{3, 1},		/* L4 */
	{3, 1},		/* L5 */
};

static unsigned int exynos3250_int_top_div[][5] = {
/* ACLK_266, ACLK_160, ACLK_200, ACLK_100, ACLK_400 */
	{2, 3, 2, 7, 0},	/* L0 */ /*ACLK266 is only 267MHz*/
	{3, 3, 2, 7, 0},	/* L1 */
	{7, 3, 2, 7, 7},	/* L2 */
	{7, 4, 3, 7, 7},	/* L3 */
	{7, 5, 4, 7, 7},	/* L4 */
	{7, 7, 7, 7, 7},	/* L5 */
};

static unsigned int exynos3250_int_acp0_div[][5] = {
/* ACLK_ACP, PCLK_ACP, ACP_DMC, ACP_DMCD, ACP_DMCP */
	{ 7, 1, 3, 1, 1},	/* LV0 */
	{ 7, 1, 3, 1, 1},	/* LV1 */
	{ 7, 1, 3, 1, 1},	/* LV2 */
	{ 7, 1, 3, 1, 1},	/* LV3 */
	{ 7, 1, 3, 1, 1},	/* LV4 */
	{ 7, 1, 3, 1, 1},	/* LV5 */
};

static unsigned int exynos3250_int_mfc_div[] = {
/* SCLK_MFC */
	1,	/* L0 */
	1,	/* L1 */
	1,	/* L2 */
	2,	/* L3 */
	3,	/* L4 */
	4,	/* L5 */
};

static void exynos3250_int_update_state(unsigned int target_freq)
{
	cputime64_t cur_time = get_jiffies_64();
	cputime64_t tmp_cputime;
	unsigned int target_idx = LV_0;
	unsigned int i;

	/*
	 * Find setting value with target frequency
	 */
	for (i = LV_0; i < LV_END; i++) {
		if (int_bus_opp_list[i].clk == target_freq)
			target_idx = int_bus_opp_list[i].idx;
	}

	tmp_cputime = cur_time - int_pre_time;

	int_bus_opp_list[target_idx].time_in_state =
		int_bus_opp_list[target_idx].time_in_state + tmp_cputime;

	int_pre_time = cur_time;
}

static unsigned int exynos3250_int_set_div(enum int_bus_idx target_idx)
{
	unsigned int tmp;
	unsigned int timeout;
	struct int_clkdiv_info *target_int_clkdiv;

	list_for_each_entry(target_int_clkdiv, &int_dvfs_list, list) {
		if (target_int_clkdiv->lv_idx == target_idx)
			break;
	}
	/*
	 * Setting for DIV
	 */
	tmp = __raw_readl(target_int_clkdiv->lbus.target_reg);
	tmp &= ~target_int_clkdiv->lbus.reg_mask;
	tmp |= target_int_clkdiv->lbus.reg_value;
	__raw_writel(tmp, target_int_clkdiv->lbus.target_reg);

	tmp = __raw_readl(target_int_clkdiv->rbus.target_reg);
	tmp &= ~target_int_clkdiv->rbus.reg_mask;
	tmp |= target_int_clkdiv->rbus.reg_value;
	__raw_writel(tmp, target_int_clkdiv->rbus.target_reg);

	tmp = __raw_readl(target_int_clkdiv->top.target_reg);
	tmp &= ~target_int_clkdiv->top.reg_mask;
	tmp |= target_int_clkdiv->top.reg_value;
	__raw_writel(tmp, target_int_clkdiv->top.target_reg);

	tmp = __raw_readl(target_int_clkdiv->acp0.target_reg);
	tmp &= ~target_int_clkdiv->acp0.reg_mask;
	tmp |= target_int_clkdiv->acp0.reg_value;
	__raw_writel(tmp, target_int_clkdiv->acp0.target_reg);

	tmp = __raw_readl(target_int_clkdiv->mfc.target_reg);
	tmp &= ~target_int_clkdiv->mfc.reg_mask;
	tmp |= target_int_clkdiv->mfc.reg_value;
	__raw_writel(tmp, target_int_clkdiv->mfc.target_reg);

	/*
	 * Wait for divider change done
	 */
	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->lbus.wait_reg);
		timeout--;

		if (!timeout) {
			pr_err("%s : Leftbus DIV timeout\n", __func__);
			return -ETIME;
		}

	} while (tmp & target_int_clkdiv->lbus.wait_mask);

	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->rbus.wait_reg);
		timeout--;

		if (!timeout) {
			pr_err("%s : Rightbus DIV timeout\n", __func__);
			return -ETIME;
		}

	} while (tmp & target_int_clkdiv->rbus.wait_mask);

	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->top.wait_reg);
		timeout--;

		if (!timeout) {
			pr_err("%s : TOP DIV timeout\n", __func__);
			return -ETIME;
		}

	} while (tmp & target_int_clkdiv->top.wait_mask);

	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->acp0.wait_reg);
		timeout--;

		if (!timeout) {
			pr_err("%s : ACP0 DIV timeout\n", __func__);
			return -ETIME;
		}

	} while (tmp & target_int_clkdiv->acp0.wait_mask);

	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->mfc.wait_reg);
		timeout--;

		if (!timeout) {
			pr_err("%s : MFC DIV timeout\n", __func__);
			return -ETIME;
		}

	} while (tmp & target_int_clkdiv->mfc.wait_mask);

	return 0;
}

static enum int_bus_idx exynos3250_find_int_bus_idx(unsigned long target_freq)
{
	unsigned int i;

	for (i = 0; i < LV_END; i++) {
		if (int_bus_opp_list[i].clk == target_freq)
			return i;
	}

	return LV_END;
}
struct regulator *int_regulator = NULL;
struct mutex int_g3d_lock;
void int_g3d_regulator_init(struct regulator *regulator)
{
	if (int_regulator == NULL) {
		mutex_init(&int_g3d_lock);
		int_regulator = regulator;
	}
	return;
}

static int int_g3d_regulator_set_voltage(int min_uV, int max_uV, enum asv_type_id target_type)
{
	static int int_min_uV =0, int_max_uV =0, g3d_min_uV = 0, g3d_max_uV = 0;

	BUG_ON(!int_regulator);
	mutex_lock(&int_g3d_lock);
	if (target_type == ID_G3D) {
		g3d_min_uV = min_uV;
		g3d_max_uV = max_uV;
	} else if (target_type == ID_INT) {
		int_min_uV = min_uV;
		int_max_uV = max_uV;
	}

	if (g3d_min_uV < int_min_uV)
		regulator_set_voltage(int_regulator, int_min_uV, int_max_uV);
	else
		regulator_set_voltage(int_regulator, g3d_min_uV, g3d_max_uV);

	mutex_unlock(&int_g3d_lock);
	return 0;
}

int g3d_regulator_set_voltage(int target_freq)
{
	int g3d_voltage = 0, i;

	for (i = LV_0; i < LV_END; i++) {
		if (int_bus_opp_list[i].clk == target_freq) {
			g3d_voltage = int_bus_opp_list[i].volt;
			break;
		}
	}
	int_g3d_regulator_set_voltage(g3d_voltage, SAFE_INT_VOLT(g3d_voltage), ID_G3D);
	return g3d_voltage;
}

static int exynos3250_int_devfreq_target(struct device *dev,
					unsigned long *_freq, u32 flags)
{
	int err = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_int *data = platform_get_drvdata(pdev);
	struct opp *opp;
	unsigned long freq;
	unsigned long old_freq;
	unsigned long target_volt;

	/* get available opp information */
	rcu_read_lock();
	opp = devfreq_recommended_opp(dev, _freq, flags);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(dev, "%s: Invalid OPP.\n", __func__);
		return PTR_ERR(opp);
	}

	freq = opp_get_freq(opp);
	target_volt = opp_get_voltage(opp);
	rcu_read_unlock();

	/* get olg opp information */
	rcu_read_lock();
	old_freq = opp_get_freq(data->curr_opp);
	rcu_read_unlock();

	exynos3250_int_update_state(old_freq);

	if (old_freq == freq)
		return 0;

	mutex_lock(&data->lock);

	if (old_freq < freq) {
		int_g3d_regulator_set_voltage(target_volt, SAFE_INT_VOLT(target_volt), ID_INT);
		err = exynos3250_int_set_div(exynos3250_find_int_bus_idx(freq));
	} else {
		err = exynos3250_int_set_div(exynos3250_find_int_bus_idx(freq));
		int_g3d_regulator_set_voltage(target_volt, SAFE_INT_VOLT(target_volt), ID_INT);
	}

	if (freq > int_bus_opp_list[LV_3].clk) {
		if (pm_qos_request_active(&exynos3250_miflock_qos))
			pm_qos_update_request(&exynos3250_miflock_qos, 200000);
	} else {
		if (pm_qos_request_active(&exynos3250_miflock_qos))
			pm_qos_update_request(&exynos3250_miflock_qos, 0);
	}

	data->curr_opp = opp;

	mutex_unlock(&data->lock);

	return err;
}

static int exynos3250_int_bus_get_dev_status(struct device *dev,
					struct devfreq_dev_status *stat)
{
	struct devfreq_data_int *data = dev_get_drvdata(dev);
	unsigned long busy_data;
	unsigned int int_ccnt = 0;
	unsigned long int_pmcnt = 0;

	rcu_read_lock();
	stat->current_frequency = opp_get_freq(data->curr_opp);
	rcu_read_unlock();

	busy_data = exynos3250_ppmu_get_busy(data->ppmu, PPMU_SET_INT,
					&int_ccnt, &int_pmcnt);

	stat->total_time = int_ccnt;
	stat->busy_time = int_pmcnt;

	return 0;
}

#if defined(CONFIG_DEVFREQ_GOV_PM_QOS)
static struct devfreq_pm_qos_data exynos3250_devfreq_int_pm_qos_data = {
	.bytes_per_sec_per_hz = 8,
	.pm_qos_class = PM_QOS_DEVICE_THROUGHPUT,
};
#endif

#if defined(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND)
static struct devfreq_simple_ondemand_data exynos3250_int_governor_data = {
	.pm_qos_class	= PM_QOS_DEVICE_THROUGHPUT,
	.upthreshold	= 15,
#ifdef CONFIG_EXYNOS_LOCK_MAX_INTFREQ
	.cal_qos_max = MAX_INTFREQ,
#else
	.cal_qos_max = 134000,
#endif
};
#endif

static struct devfreq_dev_profile exynos3250_int_devfreq_profile = {
	.initial_freq	= 133000,
	.polling_ms	= 100,
	.target		= exynos3250_int_devfreq_target,
	.get_dev_status	= exynos3250_int_bus_get_dev_status,
};

static int exynos3250_init_int_table(struct devfreq_data_int *data)
{
	unsigned int i;
	unsigned int ret;
	struct int_clkdiv_info *tmp_int_table;

	/* make list for setting value for int DVS */
	for (i = LV_0; i < LV_END; i++) {
		tmp_int_table = kzalloc(sizeof(struct int_clkdiv_info), GFP_KERNEL);

		tmp_int_table->lv_idx = i;

		/* Setting for LEFTBUS */
		tmp_int_table->lbus.target_reg = EXYNOS3_CLKDIV_LEFTBUS;
		tmp_int_table->lbus.reg_value = ((exynos3250_int_lbus_div[i][0] << EXYNOS3_CLKDIV_GDL_SHIFT) |\
						 (exynos3250_int_lbus_div[i][1] << EXYNOS3_CLKDIV_GPL_SHIFT));
		tmp_int_table->lbus.reg_mask = (EXYNOS3_CLKDIV_GDL_MASK |\
						EXYNOS3_CLKDIV_STAT_GPL_MASK);
		tmp_int_table->lbus.wait_reg = EXYNOS3_CLKDIV_STAT_LEFTBUS;
		tmp_int_table->lbus.wait_mask = (EXYNOS3_CLKDIV_STAT_GPL_MASK |\
						 EXYNOS3_CLKDIV_STAT_GDL_MASK);

		/* Setting for RIGHTBUS */
		tmp_int_table->rbus.target_reg = EXYNOS3_CLKDIV_RIGHTBUS;
		tmp_int_table->rbus.reg_value = ((exynos3250_int_rbus_div[i][0] << EXYNOS3_CLKDIV_GDR_SHIFT) |\
						 (exynos3250_int_rbus_div[i][1] << EXYNOS3_CLKDIV_GPR_SHIFT));
		tmp_int_table->rbus.reg_mask = (EXYNOS3_CLKDIV_GDR_MASK |\
						EXYNOS3_CLKDIV_STAT_GPR_MASK);
		tmp_int_table->rbus.wait_reg = EXYNOS3_CLKDIV_STAT_RIGHTBUS;
		tmp_int_table->rbus.wait_mask = (EXYNOS3_CLKDIV_STAT_GPR_MASK |\
						 EXYNOS3_CLKDIV_STAT_GDR_MASK);

		/* Setting for TOP */
		tmp_int_table->top.target_reg = EXYNOS3_CLKDIV_TOP;
		tmp_int_table->top.reg_value = ((exynos3250_int_top_div[i][0] << EXYNOS3_CLKDIV_ACLK_266_SHIFT) |
						 (exynos3250_int_top_div[i][1] << EXYNOS3_CLKDIV_ACLK_160_SHIFT) |
						 (exynos3250_int_top_div[i][2] << EXYNOS3_CLKDIV_ACLK_200_SHIFT) |
						 (exynos3250_int_top_div[i][3] << EXYNOS3_CLKDIV_ACLK_100_SHIFT) |
						 (exynos3250_int_top_div[i][4] << EXYNOS3_CLKDIV_ACLK_400_SHIFT));
		tmp_int_table->top.reg_mask = (EXYNOS3_CLKDIV_ACLK_266_MASK |
						EXYNOS3_CLKDIV_ACLK_160_MASK |
						EXYNOS3_CLKDIV_ACLK_200_MASK |
						EXYNOS3_CLKDIV_ACLK_100_MASK |
						EXYNOS3_CLKDIV_ACLK_400_MASK);
		tmp_int_table->top.wait_reg = EXYNOS3_CLKDIV_STAT_TOP;
		tmp_int_table->top.wait_mask = (EXYNOS3_CLKDIV_STAT_ACLK_266_MASK |
						 EXYNOS3_CLKDIV_STAT_ACLK_160_MASK |
						 EXYNOS3_CLKDIV_STAT_ACLK_200_MASK |
						 EXYNOS3_CLKDIV_STAT_ACLK_100_MASK |
						 EXYNOS3_CLKDIV_STAT_ACLK_400_MASK);

		/* Setting for ACP0 */
		tmp_int_table->acp0.target_reg = EXYNOS3_CLKDIV_ACP0;

		tmp_int_table->acp0.reg_value = ((exynos3250_int_acp0_div[i][0] << EXYNOS3_CLKDIV_ACP_SHIFT) |
						 (exynos3250_int_acp0_div[i][1] << EXYNOS3_CLKDIV_ACP_PCLK_SHIFT) |
						 (exynos3250_int_acp0_div[i][2] << EXYNOS3_CLKDIV_ACP_DMC_SHIFT) |
						 (exynos3250_int_acp0_div[i][3] << EXYNOS3_CLKDIV_ACP_DMCD_SHIFT) |
						 (exynos3250_int_acp0_div[i][4] << EXYNOS3_CLKDIV_ACP_DMCP_SHIFT));
		tmp_int_table->acp0.reg_mask = (EXYNOS3_CLKDIV_ACP_MASK |
						EXYNOS3_CLKDIV_ACP_PCLK_MASK |
						EXYNOS3_CLKDIV_ACP_DMC_MASK |
						EXYNOS3_CLKDIV_ACP_DMCD_MASK |
						EXYNOS3_CLKDIV_ACP_DMCP_MASK);
		tmp_int_table->acp0.wait_reg = EXYNOS3_CLKDIV_STAT_ACP0;
		tmp_int_table->acp0.wait_mask = (EXYNOS3_CLKDIV_STAT_ACP_MASK |
						 EXYNOS3_CLKDIV_STAT_ACP_PCLK_MASK |
						 EXYNOS3_CLKDIV_STAT_ACP_DMC_SHIFT |
						 EXYNOS3_CLKDIV_STAT_ACP_DMCD_SHIFT |
						 EXYNOS3_CLKDIV_STAT_ACP_DMCP_SHIFT);

		/* Setting for MFC */
		tmp_int_table->mfc.target_reg = EXYNOS3_CLKDIV_MFC;
		tmp_int_table->mfc.reg_value = (exynos3250_int_mfc_div[i] << EXYNOS3_CLKDIV_MFC_SHIFT);
		tmp_int_table->mfc.reg_mask = EXYNOS3_CLKDIV_MFC_MASK;
		tmp_int_table->mfc.wait_reg = EXYNOS3_CLKDIV_STAT_MFC;
		tmp_int_table->mfc.wait_mask = EXYNOS3_CLKDIV_STAT_MFC_MASK;

		list_add(&tmp_int_table->list, &int_dvfs_list);
	}

	/* will add code for ASV information setting function in here */
	for (i = 0; i < ARRAY_SIZE(int_bus_opp_list); i++) {
#ifdef CONFIG_EXYNOS_LOCK_MAX_INTFREQ
		if (MAX_INTFREQ < int_bus_opp_list[i].clk)
			continue;
#endif
		int_bus_opp_list[i].volt = get_match_volt(ID_INT, int_bus_opp_list[i].clk);
		if (int_bus_opp_list[i].volt == 0) {
			dev_err(data->dev, "Invalid value\n");
			return -EINVAL;
		}

		pr_info("INT %luKhz ASV is %luuV\n", int_bus_opp_list[i].clk,
							int_bus_opp_list[i].volt);

		ret = opp_add(data->dev, int_bus_opp_list[i].clk, int_bus_opp_list[i].volt);

		if (ret) {
			dev_err(data->dev, "Fail to add opp entries.\n");
			return ret;
		}
	}

	return 0;
}

static ssize_t show_freq_table(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i, count = 0;
	struct opp *opp;

	if (!unlikely(int_dev)) {
		pr_err("%s: device is not probed\n", __func__);
		return -ENODEV;
	}

	rcu_read_lock();
	for (i = 0; i < ARRAY_SIZE(int_bus_opp_list); i++) {
#ifdef CONFIG_EXYNOS_LOCK_MAX_INTFREQ
		if (MAX_INTFREQ < int_bus_opp_list[i].clk)
			continue;
#endif
		opp = opp_find_freq_exact(int_dev, int_bus_opp_list[i].clk, true);
		if (!IS_ERR_OR_NULL(opp))
			count += snprintf(&buf[count], PAGE_SIZE-count, "%lu ", opp_get_freq(opp));
	}
	rcu_read_unlock();

	count += snprintf(&buf[count], PAGE_SIZE-count, "\n");
	return count;
}

static DEVICE_ATTR(freq_table, S_IRUGO, show_freq_table, NULL);

static ssize_t int_show_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int i;
	ssize_t len = 0;
	ssize_t write_cnt = (ssize_t)((PAGE_SIZE / LV_END) - 2);

	for (i = LV_0; i < LV_END; i++) {
#ifdef CONFIG_EXYNOS_LOCK_MAX_INTFREQ
		if (MAX_INTFREQ < int_bus_opp_list[i].clk)
			continue;
#endif
		len += snprintf(buf + len, write_cnt, "%ld %llu\n", int_bus_opp_list[i].clk,
				(unsigned long long)int_bus_opp_list[i].time_in_state);
	}

	return len;
}

static DEVICE_ATTR(int_time_in_state, 0644, int_show_state, NULL);
static struct attribute *devfreq_int_entries[] = {
	&dev_attr_int_time_in_state.attr,
	NULL,
};
static struct attribute_group devfreq_int_attr_group = {
	.name	= "time_in_state",
	.attrs	= devfreq_int_entries,
};

static struct exynos_devfreq_platdata default_qos_int_pd = {
	.default_qos = 50000,
};

static int exynos3250_int_reboot_notifier_call(struct notifier_block *this,
				unsigned long code, void *_cmd)
{
	pm_qos_update_request(&exynos3250_int_qos, int_bus_opp_list[LV_2].clk);

	return NOTIFY_DONE;
}

static struct notifier_block exynos3250_int_reboot_notifier = {
	.notifier_call = exynos3250_int_reboot_notifier_call,
};

static int exynos3250_devfreq_int_probe(struct platform_device *pdev)
{
	struct devfreq_data_int *data;
	struct opp *opp;
	struct device *dev = &pdev->dev;
	struct exynos_devfreq_platdata *pdata;
	int err = 0;

	data = kzalloc(sizeof(struct devfreq_data_int), GFP_KERNEL);

	if (data == NULL) {
		dev_err(dev, "Cannot allocate memory for INT.\n");
		return -ENOMEM;
	}

	data->dev = dev;
	mutex_init(&data->lock);

	/* Setting table for int */
	exynos3250_init_int_table(data);

	rcu_read_lock();
	opp = opp_find_freq_floor(dev, &exynos3250_int_devfreq_profile.initial_freq);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(dev, "Invalid initial frequency %lu kHz.\n",
				exynos3250_int_devfreq_profile.initial_freq);
		err = PTR_ERR(opp);
		goto err_opp_add;
	}

	data->curr_opp = opp;
	rcu_read_unlock();

	int_pre_time = get_jiffies_64();

	platform_set_drvdata(pdev, data);

	data->vdd_int = regulator_get(dev, "vdd_int");
	if (IS_ERR(data->vdd_int)) {
		dev_err(dev, "Cannot get the regulator \"vdd_int\"\n");
		err = PTR_ERR(data->vdd_int);
		goto err_regulator;
	}
	int_g3d_regulator_init(data->vdd_int);

	/* Init PPMU for INT devfreq */
	data->ppmu = exynos3250_ppmu_get(PPMU_SET_INT);
	if (!data->ppmu)
		goto err_ppmu_get;

#if defined(CONFIG_DEVFREQ_GOV_USERSPACE)
	data->devfreq = devfreq_add_device(dev, &exynos3250_int_devfreq_profile,
			"userspace", NULL);
#endif
#if defined(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND)
	data->devfreq = devfreq_add_device(dev, &exynos3250_int_devfreq_profile,
			"simple_ondemand", &exynos3250_int_governor_data);
	data->devfreq->max_freq = exynos3250_int_governor_data.cal_qos_max;
#endif
	if (IS_ERR(data->devfreq)) {
		err = PTR_ERR(data->devfreq);
		goto err_devfreq_add;
	}

	devfreq_register_opp_notifier(dev, data->devfreq);

	int_dev = data->dev;

	/* Create file for time_in_state */
	err = sysfs_create_group(&data->devfreq->dev.kobj, &devfreq_int_attr_group);

	/* Add sysfs for freq_table */
	err = device_create_file(&data->devfreq->dev, &dev_attr_freq_table);
	if (err)
		pr_err("%s: Fail to create sysfs file\n", __func__);

	pdata = pdev->dev.platform_data;
	if (!pdata)
		pdata = &default_qos_int_pd;

	/* Register Notify */
	pm_qos_add_request(&exynos3250_int_qos, PM_QOS_DEVICE_THROUGHPUT, pdata->default_qos);
	pm_qos_add_request(&exynos3250_miflock_qos, PM_QOS_BUS_THROUGHPUT, 0);
	/* int max freq for fast booting */
	pm_qos_add_request(&exynos3250_boot_int_qos, PM_QOS_DEVICE_THROUGHPUT, \
				exynos3250_int_devfreq_profile.initial_freq);
	pm_qos_update_request_timeout(&exynos3250_boot_int_qos, \
				exynos3250_int_devfreq_profile.initial_freq, \
				40000 * 1000);

	register_reboot_notifier(&exynos3250_int_reboot_notifier);

	return 0;

err_devfreq_add:
	devfreq_remove_device(data->devfreq);
err_ppmu_get:
	if (data->vdd_int)
		regulator_put(data->vdd_int);
err_regulator:
	platform_set_drvdata(pdev, NULL);
err_opp_add:
	kfree(data);

	return err;
}

static int exynos3250_devfreq_int_remove(struct platform_device *pdev)
{
	struct devfreq_data_int *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);
	if (data->vdd_int)
		regulator_put(data->vdd_int);
	kfree(data);

	return 0;
}

#define INT_COLD_OFFSET	50000

static int exynos3250_devfreq_int_suspend(struct device *dev)
{
	unsigned int temp_volt;

	if (pm_qos_request_active(&exynos3250_int_qos))
		pm_qos_update_request(&exynos3250_int_qos, exynos3250_int_devfreq_profile.initial_freq);

	temp_volt = get_match_volt(ID_INT, int_bus_opp_list[0].clk);
	int_g3d_regulator_set_voltage((temp_volt + INT_COLD_OFFSET),
				SAFE_INT_VOLT(temp_volt + INT_COLD_OFFSET), ID_INT);

	return 0;
}

static int exynos3250_devfreq_int_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (!pdata)
		pdata = &default_qos_int_pd;

	if (pm_qos_request_active(&exynos3250_int_qos))
		pm_qos_update_request(&exynos3250_int_qos, pdata->default_qos);

	return 0;
}

static const struct dev_pm_ops exynos3250_devfreq_int_pm = {
	.suspend	= exynos3250_devfreq_int_suspend,
	.resume		= exynos3250_devfreq_int_resume,
};

static struct platform_driver exynos3250_devfreq_int_driver = {
	.probe	= exynos3250_devfreq_int_probe,
	.remove	= exynos3250_devfreq_int_remove,
	.driver = {
		.name	= "exynos3250-devfreq-int",
		.owner	= THIS_MODULE,
		.pm	= &exynos3250_devfreq_int_pm,
	},
};

static struct platform_device exynos3250_devfreq_int_device = {
	.name	= "exynos3250-devfreq-int",
	.id	= -1,
};

static int __init exynos3250_devfreq_int_init(void)
{
	int ret;

	ret = platform_device_register(&exynos3250_devfreq_int_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos3250_devfreq_int_driver);
}
late_initcall(exynos3250_devfreq_int_init);

static void __exit exynos3250_devfreq_int_exit(void)
{
	platform_driver_unregister(&exynos3250_devfreq_int_driver);
}
module_exit(exynos3250_devfreq_int_exit);
