/*
* Maxim ModelGauge ICs fuel gauge driver
*
* Author: Vladimir Barinov <sou...@cogentembedded.com>
* Copyright (C) 2013 Cogent Embedded, Inc.
*
* This program is free software; you can redistribute  it and/or modify it
* under  the terms of  the GNU General  Public License as published by the
* Free Software Foundation;  either version 2 of the  License, or (at your
* option) any later version.
*/

#include <linux/delay.h> 
#include <linux/err.h> 
#include <linux/init.h> 
#include <linux/interrupt.h> 
#include <linux/i2c.h> 
#include <linux/module.h> 
#include <linux/mutex.h> 
#include <linux/power_supply.h> 
#include <linux/platform_device.h> 
#include <linux/platform_data/battery-modelgauge.h>
#include <linux/regmap.h> 
#include <linux/slab.h> 

#include <linux/io.h>
#include <linux/platform_device.h>
#include <mach/map.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#endif

#define DRV_NAME "modelgauge" 

/* Register offsets for ModelGauge ICs */
#define MODELGAUGE_VCELL_REG			0x02 
#define MODELGAUGE_SOC_REG				0x04 
#define MODELGAUGE_MODE_REG				0x06 
#define MODELGAUGE_VERSION_REG			0x08 
#define MODELGAUGE_HIBRT_REG			0x0A 
#define MODELGAUGE_CONFIG_REG			0x0C 
#define MODELGAUGE_OCV_REG				0x0E 
#define MODELGAUGE_VALRT_REG			0x14 
#define MODELGAUGE_CRATE_REG			0x16 
#define MODELGAUGE_VRESETID_REG			0x18 
#define MODELGAUGE_STATUS_REG			0x1A 
#define MODELGAUGE_UNLOCK_REG			0x3E 
#define MODELGAUGE_TABLE_REG			0x40 
#define MODELGAUGE_RCOMPSEG_REG			0x80 
#define MODELGAUGE_CMD_REG				0xFE 

/* MODE register bits */
#define MODELGAUGE_MODE_QUICKSTART		(1 << 14) 
#define MODELGAUGE_MODE_ENSLEEP			(1 << 13) 
#define MODELGAUGE_MODE_HIBSTAT			(1 << 12) 

/* CONFIG register bits */
#define MODELGAUGE_CONFIG_SLEEP			(1 << 7) 
#define MODELGAUGE_CONFIG_ALSC			(1 << 6) 
#define MODELGAUGE_CONFIG_ALRT			(1 << 5) 
#define MODELGAUGE_CONFIG_ATHD_MASK		0x1f 

/* STATUS register bits */
#define MODELGAUGE_STATUS_ENVR			(1 << 14) 
#define MODELGAUGE_STATUS_SC			(1 << 13) 
#define MODELGAUGE_STATUS_HD			(1 << 12) 
#define MODELGAUGE_STATUS_VR			(1 << 11) 
#define MODELGAUGE_STATUS_VL			(1 << 10) 
#define MODELGAUGE_STATUS_VH			(1 << 9) 
#define MODELGAUGE_STATUS_RI			(1 << 8) 

/* VRESETID register bits */
#define MODELGAUGE_VRESETID_DIS			(1 << 8) 

#define MODELGAUGE_UNLOCK_VALUE			0x4a57 
#define MODELGAUGE_RESET_VALUE			0x5400 

#define MODELGAUGE_RCOMP_UPDATE_DELAY	60000 

/* Capacity threshold where an interrupt is generated on the ALRT pin */
#define MODELGAUGE_EMPTY_ATHD			15 
/* Enable alert for 1% soc change */
#define MODELGAUGE_SOC_CHANGE_ALERT		1 
/* Hibernate threshold (crate), where IC enters hibernate mode */
#define MODELGAUGE_HIBRT_THD			20 
/* Active threshold (mV), where IC exits hibernate mode */
#define MODELGAUGE_ACTIVE_THD			60 
/* Voltage (mV), when IC alerts if battery voltage less then undervoltage */
#define MODELGAUGE_UV					0 
/* Voltage (mV), when IC alerts if battery voltage greater then overvoltage */
#define MODELGAUGE_OV					5120 
/*
* Voltage threshold (mV) below which the IC resets itself.
* Used to detect battery removal and reinsertion
*/
#define MODELGAUGE_RV					0 
//#define MODELGAUGE_RV					2800		// nermy

#ifndef DEBUG
#define DEBUG	(1)
#endif
#if DEBUG
	#define gprintk(fmt, x... ) printk( "%s:%d: " fmt, __FUNCTION__ , __LINE__, ## x)
#else
	#define gprintk(x...) do { } while (0)
#endif

enum chip_id {
	ID_MAX17040,
	ID_MAX17041,
	ID_MAX17043,
	ID_MAX17044,
	ID_MAX17048,
	ID_MAX17049,
	ID_MAX17058,
	ID_MAX17059,
};

struct modelgauge_priv {
	struct device					*dev;
	struct regmap					*regmap;
	struct power_supply				battery;
	struct modelgauge_platform_data	*pdata;
	enum chip_id					chip;
	struct work_struct				load_work;
	struct delayed_work				rcomp_work;
	int								soc_shift;
	/* jia */
	void __iomem *base_syscon;
};


int nermy_regmap_read(struct regmap *map, unsigned int reg, unsigned int *val)
{
	int ret;

	ret = regmap_read(map, reg, val);
	//gprintk(" 0x%x --> 0x%x  \n", reg, val[0]);
	return ret;
}

void nermy_regmap_write(struct regmap *regmap, unsigned int addr, unsigned int reg)
{
	regmap_write(regmap, addr, reg);
	//gprintk("0x%x <-- 0x%04x \n", addr, reg);
}


static void modelgauge_write_block(struct regmap *regmap, u8 adr, u8 size,
	u16 *data)
{
	int k;

	/* RAM has different endianness then registers */
	for (k = 0; k < size; k += 2, adr += 2, data++)
		nermy_regmap_write(regmap, adr, cpu_to_be16(*data));
}

static int modelgauge_lsb_to_uvolts(struct modelgauge_priv *priv, int lsb)
{
	switch (priv->chip) {
		case ID_MAX17040:
		case ID_MAX17043:
			return (lsb >> 4) * 1250; /* 1.25mV per bit */
		case ID_MAX17041:
		case ID_MAX17044:
			return (lsb >> 4) * 2500; /* 2.5mV per bit */
		case ID_MAX17048:
		case ID_MAX17058:
			return lsb * 625 / 8; /* 78.125uV per bit */
		case ID_MAX17049:
		case ID_MAX17059:
			return lsb * 625 / 4; /* 156.25uV per bit */
		default:
			return -EINVAL;
	}
}

static enum power_supply_property modelgauge_battery_props[] = {
//	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
};

static int modelgauge_get_property(struct power_supply *psy,
							enum power_supply_property psp,
							union power_supply_propval *val)
{
	struct modelgauge_priv *priv = container_of(psy,
												struct modelgauge_priv,
												battery);
	struct regmap *regmap = priv->regmap;
	struct modelgauge_platform_data *pdata = priv->pdata;
	int reg=0;
	int ret;

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			if (pdata && pdata->get_charging_status)
				val->intval = pdata->get_charging_status();
			else
				val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
			if (pdata && pdata->full_adjustment)
				val->intval = pdata->full_adjustment;
			else
				val->intval = 100;
			break;
		case POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN:
			if (pdata && pdata->empty_adjustment)
				val->intval = pdata->empty_adjustment;
			else
				val->intval = 0;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			ret = nermy_regmap_read(regmap, MODELGAUGE_VCELL_REG, &reg);
			if (ret < 0)
				return ret;

			val->intval = modelgauge_lsb_to_uvolts(priv, reg);
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_OCV:
			/* Unlock model access */
			nermy_regmap_write(regmap, MODELGAUGE_UNLOCK_REG,
				MODELGAUGE_UNLOCK_VALUE);
			ret = nermy_regmap_read(regmap, MODELGAUGE_OCV_REG, &reg);
			/* Lock model access */
			nermy_regmap_write(regmap, MODELGAUGE_UNLOCK_REG, 0);
			if (ret < 0)
				return ret;

			val->intval = modelgauge_lsb_to_uvolts(priv, reg);
			break;

		case POWER_SUPPLY_PROP_CAPACITY:
			ret = nermy_regmap_read(regmap, MODELGAUGE_SOC_REG, &reg);
			if (ret < 0)
				return ret;

			val->intval = reg / (1 << priv->soc_shift);
			if ( val->intval > 100)
				val->intval = 100;
			break;

		case POWER_SUPPLY_PROP_TEMP:
			if (pdata && pdata->get_temperature)
				val->intval = pdata->get_temperature();
			else
				val->intval = 25;
			break;

		default:
			return -EINVAL;
	}
	return 0;
}

static void modelgauge_update_rcomp(struct modelgauge_priv *priv)
{
	struct regmap *regmap = priv->regmap;
	struct modelgauge_platform_data *pdata = priv->pdata;
	u16 rcomp;
	int temp;

	if (pdata->get_temperature)
		temp = pdata->get_temperature();
	else
		temp = 25;

	if (!pdata->temp_co_up)
		pdata->temp_co_up = -500;
	if (!pdata->temp_co_down)
		pdata->temp_co_down = -5000;

	rcomp = pdata->rcomp0;
	if (temp > 20)
		rcomp += (temp - 20) * pdata->temp_co_up / 1000;
	else
		rcomp += (temp - 20) * pdata->temp_co_down / 1000;

	/* Update RCOMP */
	regmap_update_bits(regmap, MODELGAUGE_CONFIG_REG, 0xff, rcomp << 8);
}

static void modelgauge_update_rcomp_work(struct work_struct *work)
{
	struct modelgauge_priv *priv = container_of(work,
	struct modelgauge_priv,
		rcomp_work.work);

	modelgauge_update_rcomp(priv);
	schedule_delayed_work(&priv->rcomp_work,
		msecs_to_jiffies(MODELGAUGE_RCOMP_UPDATE_DELAY));
}

static int modelgauge_load_model(struct modelgauge_priv *priv)
{
	struct regmap *regmap = priv->regmap;
	struct modelgauge_platform_data *pdata = priv->pdata;
	int ret = -EINVAL;
	int timeout, k;
	int ocv, config, soc;

	/* Save CONFIG */
	nermy_regmap_read(regmap, MODELGAUGE_CONFIG_REG, &config);

	for (timeout = 0; timeout < 100; timeout++) {
		/* Unlock model access */
		nermy_regmap_write(regmap, MODELGAUGE_UNLOCK_REG,
			MODELGAUGE_UNLOCK_VALUE);

		/* Read OCV */
		nermy_regmap_read(regmap, MODELGAUGE_OCV_REG, &ocv);
		if (ocv != 0xffff)
			break;
	}

	if (timeout >= 100) {
		dev_err(priv->dev, "timeout to unlock model access\n");
		ret = -EIO;
		goto exit;
	}

	switch (priv->chip) {
		case ID_MAX17058:
		case ID_MAX17059:
			/* Reset chip transaction does not provide ACK */
			nermy_regmap_write(regmap, MODELGAUGE_CMD_REG,
				MODELGAUGE_RESET_VALUE);
			msleep(150);

			for (timeout = 0; timeout < 100; timeout++) {
				int reg;

				/* Unlock Model Access */
				nermy_regmap_write(regmap, MODELGAUGE_UNLOCK_REG,
					MODELGAUGE_UNLOCK_VALUE);

				/* Read OCV */
				nermy_regmap_read(regmap, MODELGAUGE_OCV_REG, &reg);
				if (reg != 0xffff)
					break;
			}

			if (timeout >= 100) {
				dev_err(priv->dev, "timeout to unlock model access\n");
				ret = -EIO;
				goto exit;
			}
			break;
		default:
			break;
	}

	switch (priv->chip) {
		case ID_MAX17040:
		case ID_MAX17041:
		case ID_MAX17043:
		case ID_MAX17044:
			/* Write OCV */
			nermy_regmap_write(regmap, MODELGAUGE_OCV_REG, pdata->ocvtest);
			/* Write RCOMP to its maximum value */
			nermy_regmap_write(regmap, MODELGAUGE_CONFIG_REG, 0xff00);
			break;
		default:
			break;
	}

	/* Write the model */
	modelgauge_write_block(regmap, MODELGAUGE_TABLE_REG,
		MODELGAUGE_TABLE_SIZE,
		(u16 *)pdata->model_data);

	switch (priv->chip) {
		case ID_MAX17048:
        case ID_MAX17049: { 
			u16 buf[16];

			if (!pdata->rcomp_seg)
				pdata->rcomp_seg = 0x80;

			for (k = 0; k < 16; k++)
				*buf = pdata->rcomp_seg;

			/* Write RCOMPSeg */
			modelgauge_write_block(regmap, MODELGAUGE_RCOMPSEG_REG,
				32, buf);
			}
			break;
		default:
			break;
	}

	switch (priv->chip) {
		case ID_MAX17040:
		case ID_MAX17041:
		case ID_MAX17043:
		case ID_MAX17044:
			/* Delay at least 150ms */
			msleep(150);
			break;
		default:
			break;
	}

	/* Write OCV */
	nermy_regmap_write(regmap, MODELGAUGE_OCV_REG, pdata->ocvtest);

	switch (priv->chip) {
		case ID_MAX17048:
		case ID_MAX17049:
			/* Disable Hibernate */
			nermy_regmap_write(regmap, MODELGAUGE_HIBRT_REG, 0);
			/* fall-through */
		case ID_MAX17058:
		case ID_MAX17059:
			/* Lock Model Access */
			nermy_regmap_write(regmap, MODELGAUGE_UNLOCK_REG, 0);
			break;
		default:
			break;
	}

	/* Delay between 150ms and 600ms */
	msleep(200);

	/* Read SOC Register and compare to expected result */
	nermy_regmap_read(regmap, MODELGAUGE_SOC_REG, &soc);
	soc >>= 8;
	if (soc >= pdata->soc_check_a && soc <= pdata->soc_check_b)
		ret = 0;

	switch (priv->chip) {
		case ID_MAX17048:
		case ID_MAX17049:
		case ID_MAX17058:
		case ID_MAX17059:
			/* Unlock model access */
			nermy_regmap_write(regmap, MODELGAUGE_UNLOCK_REG,
				MODELGAUGE_UNLOCK_VALUE);
			break;
		default:
			break;
	}

	/* Restore CONFIG and OCV */
	nermy_regmap_write(regmap, MODELGAUGE_CONFIG_REG, config);
	nermy_regmap_write(regmap, MODELGAUGE_OCV_REG, ocv);

	switch (priv->chip) {
		case ID_MAX17048:
		case ID_MAX17049:
			/* Restore Hibernate */
			nermy_regmap_write(regmap, MODELGAUGE_HIBRT_REG,
				(MODELGAUGE_HIBRT_THD << 8) |
				MODELGAUGE_ACTIVE_THD);
			break;
		default:
			break;
	}

exit:
	/* Lock model access */
	nermy_regmap_write(regmap, MODELGAUGE_UNLOCK_REG, 0);

	/* Wait 150ms minimum */
	msleep(150);

	return ret;
}

static void modelgauge_load_model_work(struct work_struct *work)
{
	struct modelgauge_priv *priv = container_of(work,
									struct modelgauge_priv,
									load_work);
	struct regmap *regmap = priv->regmap;
	int ret;
	int timeout;

	for (timeout = 0; timeout < 10; timeout++) {
		/* Load custom model data */
		ret = modelgauge_load_model(priv);
		if (!ret)
			break;
	}

	if (timeout >= 10) {
		dev_info(priv->dev, "failed to load custom model\n");
		return;
	}

	switch (priv->chip) {
		case ID_MAX17048:
		case ID_MAX17049:
		case ID_MAX17058:
		case ID_MAX17059:
			/* Clear reset indicator bit */
			regmap_update_bits(regmap, MODELGAUGE_STATUS_REG,
				MODELGAUGE_STATUS_RI, 0);
			break;
		default:
			break;
	}
}

static irqreturn_t modelgauge_irq_handler(int id, void *dev)
{
	struct modelgauge_priv *priv = dev;

	gprintk(":irq \n");
	/* clear alert status bit */
	regmap_update_bits(priv->regmap, MODELGAUGE_CONFIG_REG,
		MODELGAUGE_CONFIG_ALRT, 0);

	power_supply_changed(&priv->battery);
	return IRQ_HANDLED;
}

static int modelgauge_init(struct modelgauge_priv *priv)
{
	struct regmap *regmap = priv->regmap;
	struct modelgauge_platform_data *pdata = priv->pdata;
	int ret;
	int reg;


	ret = nermy_regmap_read(regmap, MODELGAUGE_VERSION_REG, &reg);
	if (ret < 0)
		return -ENODEV;

	dev_info(priv->dev, "IC production version 0x%04x\n", reg);
	gprintk("IC production version 0x%04x\n", reg);

	/* SOC=0 means unrecoverable IC fault, reset is a workaround */
	nermy_regmap_read(regmap, MODELGAUGE_SOC_REG, &reg);
	if (!reg) {
		dev_info(priv->dev, "Reset chip, SOC measurement stall\n");
		/* Reset chip transaction does not provide ACK */
		nermy_regmap_write(regmap, MODELGAUGE_CMD_REG,
			MODELGAUGE_RESET_VALUE);
		msleep(150);
	}

	/* Default model uses 8 bits per percent */
	priv->soc_shift = 8;

	if (!priv->pdata) {
		dev_info(priv->dev, "no platform data provided\n");
		return 0;
	}

	switch (pdata->bits) {
		case 19:
			priv->soc_shift = 9;
			break;
		case 18:
		default:
			priv->soc_shift = 8;
			break;
	}

	/* Set RCOMP */
	modelgauge_update_rcomp(priv);
	if (pdata->get_temperature) {			// nermy->check
		/* Schedule update RCOMP */
		schedule_delayed_work(&priv->rcomp_work,
			msecs_to_jiffies(MODELGAUGE_RCOMP_UPDATE_DELAY));
	}

	/* Clear alert status bit, wake-up, set alert threshold */
	reg = 0;
	switch (priv->chip) {
		case ID_MAX17048:
		case ID_MAX17049:
			reg |= MODELGAUGE_SOC_CHANGE_ALERT ? MODELGAUGE_CONFIG_ALSC : 0;
			/* fall-through */
		case ID_MAX17043:
		case ID_MAX17044:
		case ID_MAX17058:
		case ID_MAX17059:
			reg |= 32 - (MODELGAUGE_EMPTY_ATHD << (priv->soc_shift - 8));
			break;
		default:
			break;
	}
	regmap_update_bits(regmap, MODELGAUGE_CONFIG_REG,
		MODELGAUGE_CONFIG_ALRT | MODELGAUGE_CONFIG_SLEEP |
		MODELGAUGE_CONFIG_ALSC | MODELGAUGE_CONFIG_ATHD_MASK,
		reg);

	switch (priv->chip) {
		case ID_MAX17048:
		case ID_MAX17049:
			/* Set Hibernate thresholds */
			reg = (MODELGAUGE_HIBRT_THD * 125 / 26) & 0xff;
			reg <<= 8;
			reg |= (MODELGAUGE_ACTIVE_THD * 4 / 5) & 0xff;
			nermy_regmap_write(regmap, MODELGAUGE_HIBRT_REG, reg);

			/* Set undervoltage/overvoltage alerts */
			reg = (MODELGAUGE_UV / 20) & 0xff;
			reg <<= 8;
			reg |= (MODELGAUGE_OV / 20) & 0xff;
			nermy_regmap_write(regmap, MODELGAUGE_VALRT_REG, reg);
			/* fall-through */
		case ID_MAX17058:
		case ID_MAX17059:
			/* Disable sleep mode and quick start */
			nermy_regmap_write(regmap, MODELGAUGE_MODE_REG, 0);

			/* Setup reset voltage threshold */
			if (MODELGAUGE_RV)
				reg = ((MODELGAUGE_RV / 40) & 0x7f) << 9;
			else
				reg = MODELGAUGE_VRESETID_DIS;

			nermy_regmap_write(regmap, MODELGAUGE_VRESETID_REG, reg);

			/* Skip load model if reset indicator cleared */
			nermy_regmap_read(regmap, MODELGAUGE_STATUS_REG, &reg);
			/* Skip load custom model */
			if (!(reg & MODELGAUGE_STATUS_RI))
				return 0;
			break;
		default:
			break;
	}

	/* Schedule load custom model work */
	if (pdata->model_data)
		schedule_work(&priv->load_work);

	return 0;
}

static struct modelgauge_platform_data *modelgauge_parse_dt(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct device_node *np = dev->of_node;
	struct modelgauge_platform_data *pdata;
	struct property *prop;
	int gpio;
	int ret;

	 pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL); 
	if (!pdata)
		return NULL;

	gpio = of_get_gpio(np, 0);
	if (!gpio_is_valid(gpio)) {
		dev_err(dev, "failed to get interrupt gpio\n");
		return ERR_PTR(-EINVAL);
	}
	client->irq = gpio_to_irq(gpio);

	ret = of_property_read_u8(np, "maxim,empty_adjustment", &pdata->empty_adjustment);
	if (ret)
		pdata->rcomp0 = 100;

	ret = of_property_read_u8(np, "maxim,full_adjustment", &pdata->full_adjustment);
	if (ret)
		pdata->rcomp0 = 0;

	ret = of_property_read_u8(np, "maxim,rcomp0", &pdata->rcomp0);
	if (ret)
		pdata->rcomp0 = 25;

	ret = of_property_read_u32(np, "maxim,temp-co-up", &pdata->temp_co_up);
	if (ret)
		pdata->temp_co_up = -500;

	ret = of_property_read_u32(np, "maxim,temp-co-down",
		&pdata->temp_co_down);
	if (ret)
		pdata->temp_co_down = -5000;

	ret = of_property_read_u16(np, "maxim,ocvtest", &pdata->ocvtest);
	if (ret)
		pdata->ocvtest = 0;

	ret = of_property_read_u8(np, "maxim,soc-check-a", &pdata->soc_check_a);
	if (ret)
		pdata->soc_check_a = 0;

	ret = of_property_read_u8(np, "maxim,soc-check-b", &pdata->soc_check_b);
	if (ret)
		pdata->soc_check_b = 0;

	ret = of_property_read_u8(np, "maxim,bits", &pdata->bits);
	if (ret)
		pdata->bits = 18;

	ret = of_property_read_u16(np, "maxim,rcomp-seg", &pdata->rcomp_seg);
	if (ret)
		pdata->rcomp_seg = 0;

	prop = of_find_property(np, "maxim,model-data", NULL);
	if (prop && prop->length == MODELGAUGE_TABLE_SIZE) {
		pdata->model_data = devm_kzalloc(dev, MODELGAUGE_TABLE_SIZE,
			GFP_KERNEL);
		if (!pdata->model_data)
			goto out;

		ret = of_property_read_u8_array(np, "maxim,model-data",
			pdata->model_data,
			MODELGAUGE_TABLE_SIZE);
		if (ret) {
			dev_warn(dev, "failed to get model_data %d\n", ret);
			devm_kfree(dev, pdata->model_data);
			pdata->model_data = NULL;
		}
	}

out:
	return pdata;

}

static const struct regmap_config modelgauge_regmap = {
	.reg_bits = 8,
	.val_bits = 16,
};

static int modelgauge_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct modelgauge_priv *priv;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -EIO;

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (client->dev.of_node)
		priv->pdata = modelgauge_parse_dt(&client->dev);
	else
		priv->pdata = client->dev.platform_data;


	priv->dev = &client->dev;
	priv->chip = id->driver_data;

	/* jia */
	priv->base_syscon = ioremap(EXYNOS3_PA_SYSCON, SZ_4K);
	//reg = __raw_readl(priv->base_syscon + 0x0228);
	//reg &= ~(0x1<<0);
	__raw_writel(0x0, priv->base_syscon + 0x0228);
	/* jia */

	i2c_set_clientdata(client, priv);

	priv->regmap = devm_regmap_init_i2c(client, &modelgauge_regmap);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	priv->battery.name = "modelgauge_battery";
	priv->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	priv->battery.get_property = modelgauge_get_property;
	priv->battery.properties = modelgauge_battery_props;
	priv->battery.num_properties = ARRAY_SIZE(modelgauge_battery_props);


	INIT_WORK(&priv->load_work, modelgauge_load_model_work);
	INIT_DELAYED_WORK(&priv->rcomp_work, modelgauge_update_rcomp_work);

	ret = modelgauge_init(priv);
	if (ret)
		return ret;

	ret = power_supply_register(&client->dev, &priv->battery);
	if (ret) {
		dev_err(priv->dev, "failed: power supply register\n");
		goto err_supply;
	}

	if (client->irq) {
		switch (priv->chip) {
			case ID_MAX17040:
			case ID_MAX17041:
				dev_err(priv->dev, "alert line is not supported\n");
				ret = -EINVAL;
				goto err_irq;
			default:
				ret = devm_request_threaded_irq(priv->dev, client->irq,
					NULL,
					modelgauge_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					priv->battery.name,
					priv);
				if (ret) {
					dev_err(priv->dev, "failed to request irq %d\n",
						client->irq);
					goto err_irq;
				}
		}
	}


	return 0;

err_irq:
	power_supply_unregister(&priv->battery);
err_supply:
	cancel_work_sync(&priv->load_work);
	cancel_delayed_work_sync(&priv->rcomp_work);
	return ret;
}

static int modelgauge_remove(struct i2c_client *client)
{
	struct modelgauge_priv *priv = i2c_get_clientdata(client);

	cancel_work_sync(&priv->load_work);
	cancel_delayed_work_sync(&priv->rcomp_work);

	power_supply_unregister(&priv->battery);
	return 0;
}

#ifdef CONFIG_PM_SLEEP 
static int modelgauge_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct modelgauge_priv *priv = i2c_get_clientdata(client);
	struct modelgauge_platform_data *pdata = priv->pdata;

	if (pdata && pdata->get_temperature)
		cancel_delayed_work_sync(&priv->rcomp_work);

	switch (priv->chip) {
	case ID_MAX17040:
	case ID_MAX17041:
		return 0;
	default:
		if (client->irq) {
			disable_irq(client->irq);
			enable_irq_wake(client->irq);
		}
	}

	return 0;
}

static int modelgauge_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct modelgauge_priv *priv = i2c_get_clientdata(client);
	struct modelgauge_platform_data *pdata = priv->pdata;

	__raw_writel(0x0, priv->base_syscon + 0x0228);

	if (pdata && pdata->get_temperature)
		schedule_delayed_work(&priv->rcomp_work,
		msecs_to_jiffies(MODELGAUGE_RCOMP_UPDATE_DELAY));

	switch (priv->chip) {
		case ID_MAX17040:
		case ID_MAX17041:
			return 0;
		default:
			if (client->irq) {
				disable_irq_wake(client->irq);
				enable_irq(client->irq);
			}
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(modelgauge_pm_ops,
	modelgauge_suspend, modelgauge_resume);
#define MODELGAUGE_PM_OPS (&modelgauge_pm_ops) 
#else 
#define MODELGAUGE_PM_OPS NULL 
#endif /* CONFIG_PM_SLEEP */ 

static const struct of_device_id modelgauge_match[] = {
	{ .compatible = "maxim,max17040" },
	{ .compatible = "maxim,max17041" },
	{ .compatible = "maxim,max17043" },
	{ .compatible = "maxim,max17044" },
	{ .compatible = "maxim,max17048" },
	{ .compatible = "maxim,max17049" },
	{ .compatible = "maxim,max17058" },
	{ .compatible = "maxim,max17059" },
	{},
};
MODULE_DEVICE_TABLE(of, modelgauge_match);

static const struct i2c_device_id modelgauge_id[] = {
	{ "max17040", ID_MAX17040 },
	{ "max17041", ID_MAX17041 },
	{ "max17043", ID_MAX17043 },
	{ "max17044", ID_MAX17044 },
	{ "max17048", ID_MAX17048 },
	{ "max17049", ID_MAX17049 },
	{ "max17058", ID_MAX17058 },
	{ "max17059", ID_MAX17059 },
	{}
};
MODULE_DEVICE_TABLE(i2c, modelgauge_id);

static struct i2c_driver modelgauge_i2c_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = of_match_ptr(modelgauge_match),
		.pm = MODELGAUGE_PM_OPS,
	},
	.probe = modelgauge_probe,
	.remove = modelgauge_remove,
	.id_table = modelgauge_id,
};
module_i2c_driver(modelgauge_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladimir Barinov");
MODULE_DESCRIPTION("Maxim ModelGauge fuel gauge");

