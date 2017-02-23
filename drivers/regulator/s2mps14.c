/*
 * s2mps14.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/regulator/of_regulator.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/s2mps14.h>

struct s2mps14_info {
	struct regulator_dev *rdev[S2MPS14_REGULATOR_MAX];
	unsigned int opmode[S2MPS14_REGULATOR_MAX];
};

/* Some LDOs supports [LPM/Normal]ON mode during suspend state */
static int s2m_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct s2mps14_info *s2mps14 = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret, id = rdev_get_id(rdev);

	switch (mode) {
	case SEC_OPMODE_SUSPEND:			/* ON in Standby Mode */
		val = 0x1 << S2MPS14_ENABLE_SHIFT;
		break;
	case SEC_OPMODE_ON:			/* ON in Normal Mode */
		val = 0x3 << S2MPS14_ENABLE_SHIFT;
		break;
	default:
		pr_warn("%s: regulator_suspend_mode : 0x%x not supported\n",
			rdev->desc->name, mode);
		return -EINVAL;
	}

	ret = regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
				  rdev->desc->enable_mask, val);
	if (ret)
		return ret;

	s2mps14->opmode[id] = val;
	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mps14_info *s2mps14 = rdev_get_drvdata(rdev);

	return regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
				  rdev->desc->enable_mask,
				  s2mps14->opmode[rdev_get_id(rdev)]);
}

static int get_ramp_delay(int ramp_delay)
{
	unsigned char cnt = 0;

	ramp_delay /= 6;

	while (true) {
		ramp_delay = ramp_delay >> 1;
		if (ramp_delay == 0)
			break;
		cnt++;
	}
	return cnt;
}

static int s2m_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	int ramp_reg, ramp_shift, reg_id = rdev_get_id(rdev);
	int ramp_mask = 0x03;
	unsigned int ramp_value = 0;

	ramp_value = get_ramp_delay(ramp_delay / 1000);
	if (ramp_value > 4) {
		pr_warn("%s: ramp_delay: %d not supported\n",
			rdev->desc->name, ramp_delay);
	}

	switch (reg_id) {
	case S2MPS14_BUCK1 ... S2MPS14_BUCK5:
		ramp_reg = S2MPS14_REG_ETC_OTP;
		ramp_shift = 0;
		break;
	default:
		return -EINVAL;
	}

	return regmap_update_bits(rdev->regmap, ramp_reg,
				  ramp_mask << ramp_shift, ramp_value << ramp_shift);
}

static int s2m_set_voltage_time_sel(struct regulator_dev *rdev,
				   unsigned int old_selector,
				   unsigned int new_selector)
{
	unsigned int ramp_delay = 0;
	int old_volt, new_volt;

	if (rdev->constraints->ramp_delay)
		ramp_delay = rdev->constraints->ramp_delay;
	else if (rdev->desc->ramp_delay)
		ramp_delay = rdev->desc->ramp_delay;

	if (ramp_delay == 0) {
		pr_warn("%s: ramp_delay not set\n", rdev->desc->name);
		return 0;
	}

	/* sanity check */
	if (!rdev->desc->ops->list_voltage)
		return -EINVAL;

	old_volt = rdev->desc->ops->list_voltage(rdev, old_selector);
	new_volt = rdev->desc->ops->list_voltage(rdev, new_selector);

	if (old_selector < new_selector)
		return DIV_ROUND_UP(new_volt - old_volt, ramp_delay);

	return 0;
}


static struct regulator_ops s2mps14_ldo_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
	.set_mode		= s2m_set_mode,
};

static struct regulator_ops s2mps14_buck_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
	.set_mode		= s2m_set_mode,
	.set_ramp_delay		= s2m_set_ramp_delay,
};

#define _BUCK(macro)	S2MPS14_BUCK##macro
#define _buck_ops(num)	s2mps14_buck_ops##num

#define _LDO(macro)	S2MPS14_LDO##macro
#define _REG(ctrl)	S2MPS14_REG##ctrl
#define _ldo_ops(num)	s2mps14_ldo_ops##num

#define BUCK_DESC(_name, _id, _ops, m, s, v, e)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPS14_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS14_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS14_ENABLE_MASK			\
}

#define LDO_DESC(_name, _id, _ops, m, s, v, e)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPS14_LDO_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS14_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS14_ENABLE_MASK			\
}

enum regulator_desc_type {
	S2MPS14_DESC_TYPE0 = 0,
};

static struct regulator_desc regulators[][S2MPS14_REGULATOR_MAX] = {
	[S2MPS14_DESC_TYPE0] = {
			/* name, id, ops, min_uv, uV_step, vsel_reg, enable_reg */
		LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP1), _REG(_L1CTRL), _REG(_L1CTRL)),
		LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP1), _REG(_L2CTRL), _REG(_L2CTRL)),
		LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2), _REG(_L3CTRL), _REG(_L3CTRL)),
		LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2), _REG(_L4CTRL), _REG(_L4CTRL)),
		LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP1), _REG(_L5CTRL), _REG(_L5CTRL)),
		LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP1), _REG(_L6CTRL), _REG(_L6CTRL)),
		LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2), _REG(_L7CTRL), _REG(_L7CTRL)),
		LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2), _REG(_L8CTRL), _REG(_L8CTRL)),
		LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP1), _REG(_L9CTRL), _REG(_L9CTRL)),
		LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP1), _REG(_L10CTRL), _REG(_L10CTRL)),
		LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2), _REG(_L11CTRL), _REG(_L11CTRL)),
		LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2), _REG(_L12CTRL), _REG(_L12CTRL)),
		LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2), _REG(_L13CTRL), _REG(_L13CTRL)),
		LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2), _REG(_L14CTRL), _REG(_L14CTRL)),
		LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2), _REG(_L15CTRL), _REG(_L15CTRL)),
		LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2), _REG(_L16CTRL), _REG(_L16CTRL)),
		LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2), _REG(_L17CTRL), _REG(_L17CTRL)),
		LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2), _REG(_L18CTRL), _REG(_L18CTRL)),
		LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2), _REG(_L19CTRL), _REG(_L19CTRL)),
		LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2), _REG(_L20CTRL), _REG(_L20CTRL)),
		LDO_DESC("LDO21", _LDO(21), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2), _REG(_L21CTRL), _REG(_L21CTRL)),
		LDO_DESC("LDO22", _LDO(22), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP1), _REG(_L22CTRL), _REG(_L22CTRL)),
		LDO_DESC("LDO23", _LDO(23), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2), _REG(_L23CTRL), _REG(_L23CTRL)),
		LDO_DESC("LDO24", _LDO(24), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2), _REG(_L24CTRL), _REG(_L24CTRL)),
		LDO_DESC("LDO25", _LDO(25), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2), _REG(_L25CTRL), _REG(_L25CTRL)),
		BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1), _REG(_B1CTRL2), _REG(_B1CTRL1)),
		BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1), _REG(_B2CTRL2), _REG(_B2CTRL1)),
		BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1), _REG(_B3CTRL2), _REG(_B3CTRL1)),
		BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), _BUCK(_MIN2), _BUCK(_STEP2), _REG(_B4CTRL2), _REG(_B4CTRL1)),
		BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1), _REG(_B5CTRL2), _REG(_B5CTRL1)),
	},
};

#ifdef CONFIG_OF
static int s2mps14_pmic_dt_parse_pdata(struct sec_pmic_dev *iodev,
					struct sec_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct sec_regulator_data *rdata;
	unsigned int i;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}

	regulators_np = of_find_node_by_name(pmic_np, "regulators");
	if (!regulators_np) {
		dev_err(iodev->dev, "could not find regulators sub-node\n");
		return -EINVAL;
	}

	/* count the number of regulators to be supported in pmic */
	pdata->num_regulators = 0;
	for_each_child_of_node(regulators_np, reg_np) {
		pdata->num_regulators++;
	}

	rdata = devm_kzalloc(iodev->dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		dev_err(iodev->dev,
			"could not allocate memory for regulator data\n");
		return -ENOMEM;
	}

	pdata->regulators = rdata;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(regulators[S2MPS14_DESC_TYPE0]); i++)
			if (!of_node_cmp(reg_np->name,
					regulators[S2MPS14_DESC_TYPE0][i].name))
				break;

		if (i == ARRAY_SIZE(regulators[S2MPS14_DESC_TYPE0])) {
			dev_warn(iodev->dev,
			"don't know how to configure regulator %s\n",
			reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(
						iodev->dev, reg_np);
		rdata->reg_node = reg_np;
		rdata++;
	}

	return 0;
}
#else
static int s2mps14_pmic_dt_parse_pdata(struct sec_pmic_dev *iodev,
					struct sec_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#ifdef CONFIG_DEBUG_FS
static ssize_t data_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	ssize_t size = 0;
	struct regulator_dev *rdev = file->private_data;
	struct s2mps14_info *info = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);

	pr_debug("%s: count=%zu, ppos=%lld\n", __func__, count, *ppos);

	switch (info->opmode[reg_id] >> S2MPS14_ENABLE_SHIFT) {
		case SEC_OPMODE_OFF:
			size = simple_read_from_buffer(data, count, ppos, "OFF\n", sizeof("OFF\n"));
			break;
		case SEC_OPMODE_SUSPEND:
			size = simple_read_from_buffer(data, count, ppos, "SUSPEND\n", sizeof("SUSPEND\n"));
			break;
		case SEC_OPMODE_LOWPOWER:
			size = simple_read_from_buffer(data, count, ppos, "LOWPOWER\n", sizeof("LOWPOWER\n"));
			break;
		case SEC_OPMODE_ON:
			size = simple_read_from_buffer(data, count, ppos, "ON\n", sizeof("ON\n"));
			break;
		default:
			pr_err("opmode_data of %s is invalid:%d\n", rdev->desc->name, info->opmode[reg_id]);
			break;
	}

	pr_debug("%s: size=%zd, *ppos=%lld\n", __func__, size, *ppos);
	return size;
}

static ssize_t data_write(struct file *file, const char __user *data, size_t count, loff_t *ppos)
{
	ssize_t size;
	struct regulator_dev *rdev = file->private_data;
	struct s2mps14_info *info = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	char buffer[9];

	size = simple_write_to_buffer(buffer, ARRAY_SIZE(buffer), ppos, data, count);

	if (size > 0) {
		if (strncasecmp("OFF", buffer, sizeof("OFF") - 1) == 0) {
			info->opmode[reg_id] = SEC_OPMODE_OFF << S2MPS14_ENABLE_SHIFT;
		} else if (strncasecmp("SUSPEND", buffer, sizeof("SUSPEND") - 1) == 0) {
			info->opmode[reg_id] = SEC_OPMODE_SUSPEND << S2MPS14_ENABLE_SHIFT;
		} else if (strncasecmp("LOWPOWER", buffer, sizeof("LOWPOWER") - 1) == 0) {
			info->opmode[reg_id] = SEC_OPMODE_LOWPOWER << S2MPS14_ENABLE_SHIFT;
		} else if (strncasecmp("ON", buffer, sizeof("ON") - 1) == 0) {
			info->opmode[reg_id] = SEC_OPMODE_ON << S2MPS14_ENABLE_SHIFT;
		} else {
			buffer[ARRAY_SIZE(buffer) - 1] = '\0';
			pr_err("%s is not recognizable.\n", buffer);
		}
	}

	s2m_enable(rdev);

	pr_debug("%s: size=%zd\n", __func__, size);
	return size;
}

static ssize_t all_data_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	int i;
	ssize_t size;
	struct s2mps14_info *info = file->private_data;
	char *p, *buffer = (char *)kzalloc(PAGE_SIZE, GFP_KERNEL);

	pr_debug("%s: count=%zu, ppos=%lld\n", __func__, count, *ppos);

	p = buffer;
	size = snprintf(p, PAGE_SIZE - (p - buffer), "NAME\tSW Mode\tRegister value \n==============================\n");
	if (size > 0) {
		p += size;
	} else {
		pr_err("Title cannot be printed.\n");
	}

	for (i = 0; i < ARRAY_SIZE(info->rdev); i++) {
		struct regulator_dev *rdev = info->rdev[i];
		if (!IS_ERR_OR_NULL(rdev)) {
			int id = rdev_get_id(rdev);
			unsigned ret, mode, voltage, value;

			voltage = 0;
			mode = 0;
			ret = regmap_read(rdev->regmap, regulators[S2MPS14_DESC_TYPE0][id].enable_reg, &value);
			if (ret < 0) {
				pr_err("failure regmap_read\n");
			}
			else {
				mode = (value & 0xc0) >> 0x6;
				voltage = (value & 0x3F);
			}

			switch (info->opmode[id] >> S2MPS14_ENABLE_SHIFT) {
				case SEC_OPMODE_OFF:
					size = snprintf(p, PAGE_SIZE - (p - buffer), "%s\t%s\tmode:0x%x vol:0x%x\n", rdev->desc->name, "OFF",mode,voltage);
					break;
				case SEC_OPMODE_SUSPEND:
					size = snprintf(p, PAGE_SIZE - (p - buffer), "%s\t%s\tmode:0x%x vol:0x%x\n", rdev->desc->name, "SUSPEND",mode,voltage);
					break;
				case SEC_OPMODE_LOWPOWER:
					size = snprintf(p, PAGE_SIZE - (p - buffer), "%s\t%s\tmode:0x%x vol:0x%x\n", rdev->desc->name, "LOWPOWER",mode,voltage);
					break;
				case SEC_OPMODE_ON:
					size = snprintf(p, PAGE_SIZE - (p - buffer), "%s\t%s\tmode:0x%x vol:0x%x\n", rdev->desc->name, "ON",mode,voltage);
					break;
				default:
					pr_err("opmode_data of %s is invalid:%d\n", rdev->desc->name, info->opmode[id]);
					continue;
			}
			if (size > 0) {
				p += size;
			} else {
				pr_err("opmode_data of %s cannot be printed:%d\n", rdev->desc->name, info->opmode[id]);
			}
		}
	}

	size = simple_read_from_buffer(data, count, ppos, buffer, p - buffer + 1);

	pr_debug("%s: size=%zd, *ppos=%lld\n", __func__, size, *ppos);
	kfree(buffer);
	return size;
}

static const struct file_operations data_ops = {
	.open		= simple_open,
	.read		= data_read,
	.write		= data_write,
	.llseek		= no_llseek,
};

static const struct file_operations all_data_ops = {
	.open		= simple_open,
	.read		= all_data_read,
	.llseek		= no_llseek,
};

static void initialize_debug_fs(struct s2mps14_info *info) {
	struct dentry *root, *reg_dir;
	int i;

	pr_debug("%s is called.\n", __func__);

	root = debugfs_create_dir("s2mps14", NULL);
	if (IS_ERR(root)) {
		pr_err("Debug fs for GPIO power down setting is failed. (%ld)\n", PTR_ERR(root));
	}

	debugfs_create_file("mode", S_IRUGO, root, info, &all_data_ops);

	for (i = 0; i < ARRAY_SIZE(info->rdev); i++) {
		struct regulator_dev *rdev = info->rdev[i];
		if (!IS_ERR_OR_NULL(rdev)) {
			pr_debug("%s: %s\n", __func__, rdev->desc->name);
			reg_dir = debugfs_create_dir(rdev->desc->name, root);
			debugfs_create_file("mode", S_IRUGO|S_IWUSR, reg_dir, rdev, &data_ops);
		}
	}
}
#else
static void initialize_debug_fs(struct s2mps14_info *info) {}
#endif

static int s2mps14_pmic_probe(struct platform_device *pdev)
{
	struct sec_pmic_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct sec_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mps14_info *s2mps14;
	int i, ret;

	if (iodev->dev->of_node) {
		ret = s2mps14_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			return ret;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mps14 = devm_kzalloc(&pdev->dev, sizeof(struct s2mps14_info),
				GFP_KERNEL);
	if (!s2mps14)
		return -ENOMEM;

	platform_set_drvdata(pdev, s2mps14);

	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;
		config.dev = &pdev->dev;
		config.regmap = iodev->regmap;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mps14;
		config.of_node = pdata->regulators[i].reg_node;
		s2mps14->opmode[id] = regulators[S2MPS14_DESC_TYPE0][id].enable_mask;

		s2mps14->rdev[i] = regulator_register(
				&regulators[S2MPS14_DESC_TYPE0][id], &config);
		if (IS_ERR(s2mps14->rdev[i])) {
			ret = PTR_ERR(s2mps14->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n",
				i);
			s2mps14->rdev[i] = NULL;
			goto err;
		}
	}

	initialize_debug_fs(s2mps14);

	return 0;
err:
	for (i = 0; i < S2MPS14_REGULATOR_MAX; i++)
		regulator_unregister(s2mps14->rdev[i]);

	return ret;
}

static int s2mps14_pmic_remove(struct platform_device *pdev)
{
	struct s2mps14_info *s2mps14 = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < S2MPS14_REGULATOR_MAX; i++)
		regulator_unregister(s2mps14->rdev[i]);

	return 0;
}

static const struct platform_device_id s2mps14_pmic_id[] = {
	{ "s2mps14-pmic", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mps14_pmic_id);

static struct platform_driver s2mps14_pmic_driver = {
	.driver = {
		.name = "s2mps14-pmic",
		.owner = THIS_MODULE,
	},
	.probe = s2mps14_pmic_probe,
	.remove = s2mps14_pmic_remove,
	.id_table = s2mps14_pmic_id,
};

static int __init s2mps14_pmic_init(void)
{
	return platform_driver_register(&s2mps14_pmic_driver);
}
subsys_initcall(s2mps14_pmic_init);

static void __exit s2mps14_pmic_exit(void)
{
	platform_driver_unregister(&s2mps14_pmic_driver);
}
module_exit(s2mps14_pmic_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MPS14 Regulator Driver");
MODULE_LICENSE("GPL");
