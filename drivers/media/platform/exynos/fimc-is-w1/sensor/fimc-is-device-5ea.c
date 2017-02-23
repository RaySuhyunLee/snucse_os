/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <mach/exynos-fimc-is-sensor.h>

#include "../fimc-is-core.h"
#include "../fimc-is-device-sensor.h"
#include "../fimc-is-resourcemgr.h"
#include "fimc-is-device-5ea.h"
#include "fimc-is-device-5ea_reg.h"

static const struct s5k5ea_regs regs_set = {
	.init_reg_1 = S5K5EA_REGSET_TABLE(init_reg1),
	.init_reg_2 = S5K5EA_REGSET_TABLE(init_reg2),
	.frame_time = {
		[FM_LOW] = S5K5EA_REGSET_TABLE(frame_time_low),
		[FM_NORMAL] = S5K5EA_REGSET_TABLE(frame_time_normal),
		[FM_HIGH] = S5K5EA_REGSET_TABLE(frame_time_high),
	},
	.scene_mode = {
		[V4L2_SCENE_MODE_NONE] =
			S5K5EA_REGSET_TABLE(scene_mode_none),
		[V4L2_SCENE_MODE_BACKLIGHT] =
			S5K5EA_REGSET_TABLE(scene_mode_none),
		[V4L2_SCENE_MODE_BEACH_SNOW] =
			S5K5EA_REGSET_TABLE(scene_mode_beach_snow),
		[V4L2_SCENE_MODE_CANDLE_LIGHT] =
			S5K5EA_REGSET_TABLE(scene_mode_candle_night),
		[V4L2_SCENE_MODE_DAWN_DUSK] =
			S5K5EA_REGSET_TABLE(scene_mode_none),
		[V4L2_SCENE_MODE_FALL_COLORS] =
			S5K5EA_REGSET_TABLE(scene_mode_none),
		[V4L2_SCENE_MODE_FIREWORKS] =
			S5K5EA_REGSET_TABLE(scene_mode_none),
		[V4L2_SCENE_MODE_LANDSCAPE] =
			S5K5EA_REGSET_TABLE(scene_mode_landscape),
		[V4L2_SCENE_MODE_NIGHT] =
			S5K5EA_REGSET_TABLE(scene_mode_night),
		[V4L2_SCENE_MODE_PARTY_INDOOR] =
			S5K5EA_REGSET_TABLE(scene_mode_none),
		[V4L2_SCENE_MODE_PORTRAIT] =
			S5K5EA_REGSET_TABLE(scene_mode_portrait),
		[V4L2_SCENE_MODE_SPORTS] =
			S5K5EA_REGSET_TABLE(scene_mode_sports),
		[V4L2_SCENE_MODE_SUNSET] =
			S5K5EA_REGSET_TABLE(scene_mode_sunset),
		[V4L2_SCENE_MODE_TEXT] =
			S5K5EA_REGSET_TABLE(scene_mode_none),
	},
	.wb = {
		[WHITE_BALANCE_BASE] =
			S5K5EA_REGSET_TABLE(wb_auto),
		[WHITE_BALANCE_AUTO] =
			S5K5EA_REGSET_TABLE(wb_auto),
		[WHITE_BALANCE_SUNNY] =
			S5K5EA_REGSET_TABLE(wb_sunny),
		[WHITE_BALANCE_CLOUDY] =
			S5K5EA_REGSET_TABLE(wb_cloudy),
		[WHITE_BALANCE_TUNGSTEN] =
			S5K5EA_REGSET_TABLE(wb_tungsten),
		[WHITE_BALANCE_FLUORESCENT] =
			S5K5EA_REGSET_TABLE(wb_fluorescent),
	},
	.preview_input = {
		[S5K5EA_PREVIEW_INPUT_2560_1920] =
			S5K5EA_REGSET_TABLE(preview_input_2560_1920),
		[S5K5EA_PREVIEW_INPUT_1920_1080] =
			S5K5EA_REGSET_TABLE(preview_input_1920_1080),
		[S5K5EA_PREVIEW_INPUT_2560_1440] =
			S5K5EA_REGSET_TABLE(preview_input_2560_1440),
	},
};

static const struct s5k5ea_framesize preview_size_list[] = {
	{ S5K5EA_PREVIEW_1280_960,	1280,	960 },
	{ S5K5EA_PREVIEW_1920_1080,	1920,	1080 },
	{ S5K5EA_PREVIEW_1280_720,	1280,	720 },
	{ S5K5EA_PREVIEW_720_480,	720,	480 },
	{ S5K5EA_PREVIEW_640_480,	640,	480 },
};

static const struct s5k5ea_framesize capture_size_list[] = {
	{ S5K5EA_CAPTURE_2560_1920,	2560,	1920 },
	{ S5K5EA_CAPTURE_2048_1536,	2048,	1536 },
	{ S5K5EA_CAPTURE_1600_1200,	1600,	1200 },
	{ S5K5EA_CAPTURE_1024_768,	1024,	768 },
	{ S5K5EA_CAPTURE_640_480,	640,	480 },
};

static int s5k5ea_get_index(struct v4l2_subdev *sd);

static inline struct s5k5ea_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct s5k5ea_state, sd);
}

static inline struct i2c_client *to_client(struct v4l2_subdev *sd)
{
	struct fimc_is_module_enum *module;
	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(sd);
	return (struct i2c_client *)module->client;
}

static void s5k5ea_delay(struct v4l2_subdev *sd)
{
	struct s5k5ea_state *state = to_state(sd);

	if (state->userset.frame_mode == FM_LOW)
		mdelay(LOW_MAXFRTIME / 10);
	else if (state->userset.frame_mode == FM_NORMAL)
		mdelay(NOR_MAXFRTIME / 10);
	else
		mdelay(HIG_MAXFRTIME / 10);
}

static int s5k5ea_i2c_read_twobyte(struct i2c_client *client,
		u16 subaddr, u16 *data)
{
	int err;
	unsigned char buf[2];
	struct i2c_msg msg[2];

	cpu_to_be16s(&subaddr);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = (u8 *)&subaddr;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = buf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (unlikely(err != 2)) {
		dev_err(&client->dev,
				"%s: register read fail\n", __func__);
		return -EIO;
	}

	*data = ((buf[0] << 8) | buf[1]);

	return 0;
}

static int s5k5ea_i2c_write_twobyte(struct i2c_client *client,
		u16 addr, u16 w_data)
{
	int retry_count = 5;
	unsigned char buf[4];
	struct i2c_msg msg = {client->addr, 0, 4, buf};
	int ret = 0;

	buf[0] = addr >> 8;
	buf[1] = addr;
	buf[2] = w_data >> 8;
	buf[3] = w_data & 0xff;

	do {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (likely(ret == 1))
			break;
		mdelay(10);
	} while (retry_count-- > 0);

	if (ret != 1) {
		dev_err(&client->dev, "%s: I2C is not working.\n", __func__);
		return -EIO;
	}

	return 0;
}

static int s5k5ea_write_burst(struct i2c_client *client,
		const struct s5k5ea_reg_item reg[], int lines)
{
	struct s5k5ea_regset *regset_table;
	struct s5k5ea_regset *regset;
	struct s5k5ea_regset *end_regset;
	u8 *regset_data;
	u8 *dst_ptr;
	const struct s5k5ea_reg_item *end_src_ptr;
	bool flag_copied;
	const struct s5k5ea_reg_item *src_ptr = reg;
	struct s5k5ea_reg_item src_value;
	int err;

	cam_info("%s start\n", __func__);

	regset_data = vmalloc(sizeof(struct s5k5ea_reg_item) * lines);
	if (regset_data == NULL)
		return -ENOMEM;

	regset_table = vmalloc(sizeof(struct s5k5ea_regset) * lines);
	if (regset_table == NULL) {
		kfree(regset_data);
		return -ENOMEM;
	}

	dst_ptr = regset_data;
	regset = regset_table;
	end_src_ptr = &reg[lines];

	src_value = *src_ptr++;
	while (src_ptr <= end_src_ptr) {
		/* initial value for a regset */
		regset->data = dst_ptr;
		flag_copied = false;
		*dst_ptr++ = src_value.addr >> 8;
		*dst_ptr++ = src_value.addr;
		*dst_ptr++ = src_value.data >> 8;
		*dst_ptr++ = src_value.data;

		/* check subsequent values for a data flag (starts with
		   0x0F12) or something else */
		do {
			src_value = *src_ptr++;
			if ((src_value.addr) != 0x0F12) {
				/* src_value is start of next regset */
				regset->size = dst_ptr - regset->data;
				regset++;
				break;
			}
			/* copy the 0x0F12 flag if not done already */
			if (!flag_copied) {
				*dst_ptr++ = src_value.addr >> 8;
				*dst_ptr++ = src_value.addr;
				flag_copied = true;
			}
			/* copy the data part */
			*dst_ptr++ = src_value.data >> 8;
			*dst_ptr++ = src_value.data;
		} while (src_ptr <= end_src_ptr);
	}

	end_regset = regset;
	regset = regset_table;

	do {
		if (regset->size > 4) {
			/* write the address packet */
			err = fimc_is_sensor_write_burst(client, regset->data, 4);
			if (err)
				break;
			/* write the data in a burst */
			err = fimc_is_sensor_write_burst(client, regset->data+4,
					regset->size-4);

		} else
			err = fimc_is_sensor_write_burst(client, regset->data,
					regset->size);
		if (err)
			break;
		regset++;
	} while (regset < end_regset);

	vfree(regset_data);
	vfree(regset_table);

	cam_info("%s end\n", __func__);

	return err;
}

static int s5k5ea_apply_set(struct v4l2_subdev *sd,
	const struct s5k5ea_regset_table *regset)
{
	int ret = 0;
	u16 addr, val;
	u32 i;
	struct i2c_client *client = to_client(sd);
	struct s5k5ea_state *state = to_state(sd);

	BUG_ON(!client);
	BUG_ON(!regset);
	BUG_ON(!state);

	mutex_lock(&state->i2c_lock);

	cam_info("%s : E, setting_name : %s\n", __func__, regset->setting_name);

	for (i = 0; i < regset->array_size; i++) {
		addr = regset->reg[i].addr;
		val = regset->reg[i].data;
		if (addr == 0xFFFF) {
			cam_info("%s : use delay (%d ms)\n", __func__, val);
			msleep(val);
		} else {
			ret = fimc_is_sensor_write16(client, addr, val);
			if (unlikely(ret < 0)) {
				cam_err("regs set is fail(%d)", ret);
				goto p_err;
			}
		}
	}

p_err:
	mutex_unlock(&state->i2c_lock);
	return (ret > 0) ? 0 : ret;
}


static void s5k5ea_set_brightness(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *client = g_s5k5ea_i2c_client;
	struct s5k5ea_state *state = to_state(sd);
	u16 write_value = 0;

	cam_info("%s (%d)\n", __func__, value);

	if (value < EV_MIN_VALUE)
		value = EV_MIN_VALUE;
	else if (value > EV_MAX_VALUE)
		value = EV_MAX_VALUE;

	write_value = value * 0x20;

	s5k5ea_i2c_write_twobyte(client, 0x0028, 0x2000);
	s5k5ea_i2c_write_twobyte(client, 0x002a, 0x026A);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, write_value);

	mutex_lock(&state->lock);
	state->userset.brightness = value;
	mutex_unlock(&state->lock);
}

static void s5k5ea_set_contrast(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *client = g_s5k5ea_i2c_client;
	struct s5k5ea_state *state = to_state(sd);
	u16 write_value = 0;

	cam_info("%s (%d)\n", __func__, value);

	switch (value) {
	case V4L2_CONTRAST_MINUS_4:
		cam_info("V4L2_CONTRAST_MINUS_4\n");
		write_value = 0xFF81;
		break;
	case V4L2_CONTRAST_MINUS_3:
		cam_info("V4L2_CONTRAST_MINUS_3\n");
		write_value = 0xFFA0;
		break;
	case V4L2_CONTRAST_MINUS_2:
		cam_info("V4L2_CONTRAST_MINUS_2\n");
		write_value = 0xFFC0;
		break;
	case V4L2_CONTRAST_MINUS_1:
		cam_info("V4L2_CONTRAST_MINUS_1\n");
		write_value = 0xFFE0;
		break;
	case V4L2_CONTRAST_AUTO:
	case V4L2_CONTRAST_DEFAULT:
	default:
		cam_info("V4L2_CONTRAST_DEFAULT\n");
		write_value = 0x0000;
		break;
	case V4L2_CONTRAST_PLUS_1:
		cam_info("V4L2_CONTRAST_PLUS_1\n");
		write_value = 0x0020;
		break;
	case V4L2_CONTRAST_PLUS_2:
		cam_info("V4L2_CONTRAST_PLUS_2\n");
		write_value = 0x0040;
		break;
	case V4L2_CONTRAST_PLUS_3:
		cam_info("V4L2_CONTRAST_PLUS_3\n");
		write_value = 0x0060;
		break;
	case V4L2_CONTRAST_PLUS_4:
		cam_info("V4L2_CONTRAST_PLUS_4\n");
		write_value = 0x007F;
		break;
	}

	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x026C);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, write_value);

	mutex_lock(&state->lock);
	state->userset.contrast = value;
	mutex_unlock(&state->lock);
}

static void s5k5ea_set_sharpness(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *client = g_s5k5ea_i2c_client;
	struct s5k5ea_state *state = to_state(sd);
	u16 write_value = 0;

	cam_info("%s (%d)\n", __func__, value);

	switch (value) {
	case V4L2_SHARPNESS_MINUS_2:
		cam_info("V4L2_SHARPNESS_MINUS_2\n");
		write_value = 0xFFC0;
		break;
	case V4L2_SHARPNESS_MINUS_1:
		cam_info("V4L2_SHARPNESS_MINUS_1\n");
		write_value = 0xFFE0;
		break;
	case V4L2_SHARPNESS_DEFAULT:
	default:
		cam_info("V4L2_SHARPNESS_DEFAULT\n");
		write_value = 0x0000;
		break;
	case V4L2_SHARPNESS_PLUS_1:
		cam_info("V4L2_SHARPNESS_PLUS_1\n");
		write_value = 0x0020;
		break;
	case V4L2_SHARPNESS_PLUS_2:
		cam_info("V4L2_SHARPNESS_PLUS_2\n");
		write_value = 0x0040;
		break;
	}

	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x0270);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, write_value);

	mutex_lock(&state->lock);
	state->userset.sharpness = value;
	mutex_unlock(&state->lock);
}

static void s5k5ea_set_saturation(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *client = g_s5k5ea_i2c_client;
	struct s5k5ea_state *state = to_state(sd);
	u16 write_value = 0;

	cam_info("%s\n", __func__);

	switch (value) {
	case V4L2_SATURATION_MINUS_3:
		cam_info("V4L2_SATURATION_MINUS_3\n");
		/* FALL THROUGH */
	case V4L2_SATURATION_MINUS_2:
		cam_info("V4L2_SATURATION_MINUS_2\n");
		write_value = 0xFF81;
		break;
	case V4L2_SATURATION_MINUS_1:
		cam_info("V4L2_SATURATION_MINUS_1\n");
		write_value = 0xFFC0;
		break;
	case V4L2_SATURATION_DEFAULT:
	default:
		cam_info("V4L2_SATURATION_DEFAULT\n");
		write_value = 0x0000;
		break;
	case V4L2_SATURATION_PLUS_1:
		cam_info("V4L2_SATURATION_PLUS_1\n");
		write_value = 0x0040;
		break;
	case V4L2_SATURATION_PLUS_3:
		cam_info("V4L2_SATURATION_PLUS_3\n");
		/* FALL THROUGH */
	case V4L2_SATURATION_PLUS_2:
		cam_info("V4L2_SATURATION_PLUS_2\n");
		write_value = 0x0070;
		break;
	}

	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x026E);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, write_value);

	mutex_lock(&state->lock);
	state->userset.saturation = value;
	mutex_unlock(&state->lock);
}

static void s5k5ea_set_iso(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *client = g_s5k5ea_i2c_client;
	struct s5k5ea_state *state = to_state(sd);
	u16 write_value1 = 0, write_value2 = 0;

	cam_info("%s (%d)\n", __func__, value);

	switch (value) {
	case V4L2_ISO_AUTO:
	default:
		cam_info("V4L2_ISO_AUTO\n");
		write_value1 = 0;
		write_value2 = 0;
		break;
	case V4L2_ISO_50:
		cam_info("V4L2_ISO_50\n");
		write_value1 = 0x0001;
		write_value2 = 0x0100;
		break;
	case V4L2_ISO_100:
		cam_info("V4L2_ISO_100\n");
		write_value1 = 0x0001;
		write_value2 = 0x01A0;
		break;
	case V4L2_ISO_200:
		cam_info("V4L2_ISO_200\n");
		write_value1 = 0x0001;
		write_value2 = 0x0340;
		break;
	case V4L2_ISO_400:
		cam_info("V4L2_ISO_400\n");
		write_value1 = 0x0001;
		write_value2 = 0x0710;
		break;
	case V4L2_ISO_800:
		cam_info("V4L2_ISO_800\n");
		write_value1 = 0x0001;
		write_value2 = 0x0A00;
		break;
	}

	s5k5ea_i2c_write_twobyte(client, 0x0028, 0x2000);
	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x050C);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, write_value1);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, write_value2);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0001);
	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x098E);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0200);

	s5k5ea_delay(sd);
	mutex_lock(&state->lock);
	state->userset.iso = value;
	mutex_unlock(&state->lock);
}

static void s5k5ea_init_setting(struct v4l2_subdev *sd);
static int s5k5ea_start_preview(struct v4l2_subdev *sd);
static int s5k5ea_stop_preview(struct v4l2_subdev *sd);
static void s5k5ea_set_scenemode(struct v4l2_subdev *sd, int value)
{
	struct s5k5ea_state *state = to_state(sd);

	cam_info("%s\n", __func__);

	s5k5ea_stop_preview(sd);
	s5k5ea_delay(sd);

	mutex_lock(&state->lock);

	if (value == V4L2_SCENE_MODE_NIGHT)
		state->userset.frame_mode = FM_LOW;
	else if (value == V4L2_SCENE_MODE_SPORTS)
		state->userset.frame_mode = FM_HIGH;
	else
		state->userset.frame_mode = FM_NORMAL;

	mutex_unlock(&state->lock);

	s5k5ea_apply_set(sd, &regs_set.frame_time[state->userset.frame_mode]);

	if (value > V4L2_SCENE_MODE_TEXT)
		value = V4L2_SCENE_MODE_NONE;

	s5k5ea_apply_set(sd, &regs_set.scene_mode[value]);

	if (value == V4L2_SCENE_MODE_NONE)
		s5k5ea_init_setting(sd);


	s5k5ea_start_preview(sd);
	s5k5ea_delay(sd);
	mutex_lock(&state->lock);
	state->userset.scenemode = value;
	mutex_unlock(&state->lock);
}

static void s5k5ea_set_white_balance(struct v4l2_subdev *sd, int value)
{
	struct s5k5ea_state *state = to_state(sd);

	cam_info("%s\n", __func__);

	if (value >= WHITE_BALANCE_MAX)
		value = WHITE_BALANCE_AUTO;

	s5k5ea_apply_set(sd, &regs_set.wb[value]);

	mutex_lock(&state->lock);
	state->userset.white_balance = value;
	mutex_unlock(&state->lock);
}


static void s5k5ea_set_effect(struct v4l2_subdev *sd, int value)
{
	cam_info("%s\n", __func__);

}

static inline int LMAX(u16 a, u16 b)
{
	return a > b ? a : b;
}
static inline int LMIN(u16 a, u16 b)
{
	return a < b ? a : b;
}


static void s5k5ea_set_window(struct v4l2_subdev *sd)
{
	struct s5k5ea_state *state = to_state(sd);
	struct i2c_client *client = g_s5k5ea_i2c_client;
	struct afpoint point;

	cam_info("%s\n", __func__);
	point.x = state->point.x;
	point.y = state->point.y;

	if ((point.x + point.y) == 0) {
		cam_info("%s : default AF\n", __func__);
		s5k5ea_i2c_write_twobyte(client, 0x002A, 0x02D0);
		/* REG_TC_AF_FstWinStartX */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0100);
		/* REG_TC_AF_FstWinStartY */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x00E3);
		/* REG_TC_AF_FstWinSizeX */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0200);
		/* REG_TC_AF_FstWinSizeY */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0238);
		/* REG_TC_AF_ScndWinStartX */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x01C6);
		/* REG_TC_AF_ScndWinStartY */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0166);
		/*REG_TC_AF_ScndWinSizeX */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0074);
		/*REG_TC_AF_ScndWinSizeY */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0132);
		/* REG_TC_AF_WinSizesUpdated */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0001);
	} else {
		point.x = LMIN(LMAX(point.x, 50), 1280-50) - 50;
		point.y = LMIN(LMAX(point.y, 50), 960-50) - 50;

		cam_info("%s : AF (%d, %d)\n", __func__, point.x, point.y);
		cam_info("%s : AF 1st Window(%d : %d , %d : %d)\n", __func__,
			point.x, point.x+100, point.y, point.y+100);
		cam_info("%s : AF 2nd Window(%d : %d , %d : %d)\n", __func__,
			point.x+25, point.x+75, point.y+25, point.y+75);

		s5k5ea_i2c_write_twobyte(client, 0x002A, 0x02D0);
		/* REG_TC_AF_FstWinStartX */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, point.x);
		/* REG_TC_AF_FstWinStartY */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, point.y);
		/* REG_TC_AF_FstWinSizeX */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, 100);
		/* REG_TC_AF_FstWinSizeY */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, 100);
		/* REG_TC_AF_ScndWinStartX */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, point.x + 25);
		/* REG_TC_AF_ScndWinStartY */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, point.y + 25);
		/*REG_TC_AF_ScndWinSizeX */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, 50);
		/*REG_TC_AF_ScndWinSizeY */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, 50);
		/* REG_TC_AF_WinSizesUpdated */
		s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0001);
	}

	s5k5ea_delay(sd);
}

static void s5k5ea_set_af(struct v4l2_subdev *sd)
{
	struct i2c_client *client = g_s5k5ea_i2c_client;

	cam_info("%s\n", __func__);
	s5k5ea_set_window(sd);

	s5k5ea_i2c_write_twobyte(client, 0x0028, 0x2000);
	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x02C8);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0005);
}

static void s5k5ea_cancel_af(struct v4l2_subdev *sd)
{
	struct i2c_client *client = g_s5k5ea_i2c_client;

	cam_info("%s\n", __func__);

	s5k5ea_i2c_write_twobyte(client, 0xFCFC, 0xD000);
	s5k5ea_i2c_write_twobyte(client, 0x0028, 0x2000);
	s5k5ea_i2c_write_twobyte(client, 0X002A, 0X02C8);
	s5k5ea_i2c_write_twobyte(client, 0X0F12, 0X0001);
}



static int s5k5ea_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct s5k5ea_state *state = to_state(sd);
	int value = ctrl->value;

	cam_info("%s\n", __func__);

	switch (ctrl->id) {
	case V4L2_CID_SCENEMODE:
		if (state->userset.scenemode != value)
			s5k5ea_set_scenemode(sd, value);
		break;
	case V4L2_CID_CAM_BRIGHTNESS:
		if (state->userset.brightness != value)
			s5k5ea_set_brightness(sd, value);
		break;
	case V4L2_CID_WHITE_BALANCE_PRESET:
		if (state->userset.white_balance != value)
			s5k5ea_set_white_balance(sd, value);
		break;
	case V4L2_CID_CAMERA_EFFECT:
		s5k5ea_set_effect(sd, value);
		break;
	case V4L2_CID_CAM_CONTRAST:
		if (state->userset.contrast != value)
			s5k5ea_set_contrast(sd, value);
		break;
	case V4L2_CID_CAM_SATURATION:
		if (state->userset.saturation != value)
			s5k5ea_set_saturation(sd, value);
		break;
	case V4L2_CID_CAM_SHARPNESS:
		if (state->userset.sharpness != value)
			s5k5ea_set_sharpness(sd, value);
		break;
	case V4L2_CID_CAM_ISO:
		if (state->userset.iso != value)
			s5k5ea_set_iso(sd, value);
		break;
	case V4L2_CID_CAPTURE:
		mutex_lock(&state->lock);
		if (value)
			state->userset.camera_mode = S5K5EA_OPRMODE_IMAGE;
		else
			state->userset.camera_mode = S5K5EA_OPRMODE_VIDEO;
		mutex_unlock(&state->lock);
		break;
	case V4L2_CID_CAM_OBJECT_POSITION_X:
		cam_info("V4L2_CID_CAMERA_OBJECT_POSITION_X : %d\n", value);
		mutex_lock(&state->lock);
		state->point.x = value;
		mutex_unlock(&state->lock);
		break;
	case V4L2_CID_CAM_OBJECT_POSITION_Y:
		cam_info("V4L2_CID_CAMERA_OBJECT_POSITION_Y : %d\n", value);
		mutex_lock(&state->lock);
		state->point.y = value;
		mutex_unlock(&state->lock);
		break;
	case V4L2_CID_CAM_SINGLE_AUTO_FOCUS:
	case V4L2_CID_CAM_SET_AUTO_FOCUS:
		cam_info("V4L2_CID_CAM_SINGLE_AUTO_FOCUS (%d)\n", value);
		mutex_lock(&state->lock);
		if (value == 0)
			s5k5ea_set_af(sd);
		else
			s5k5ea_cancel_af(sd);
		state->point.x = state->point.y = 0;
		mutex_unlock(&state->lock);
		break;
	case V4L2_CID_FOCUS_MODE:
	case V4L2_CID_JPEG_QUALITY:
	case V4L2_CID_CAM_FLASH_MODE:
		break;
	default:
		cam_err("[%s] Unidentified ID (%X)\n", __func__, ctrl->id);
		break;
	}

	return 0;
}

static int s5k5ea_get_af(struct v4l2_subdev *sd)
{
	struct i2c_client *client = g_s5k5ea_i2c_client;
	u16 read_value = -1;

	cam_info("%s\n", __func__);

	s5k5ea_i2c_write_twobyte(client, 0xFCFC, 0xd000);
	s5k5ea_i2c_write_twobyte(client, 0x002C, 0x2000);
	s5k5ea_i2c_write_twobyte(client, 0x002E, 0x1F92);
	s5k5ea_i2c_read_twobyte(client, 0x0F12, &read_value);

	cam_info("AF Result 1st(%d)\n", read_value);

	return read_value;
}

static int s5k5ea_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct s5k5ea_state *state = to_state(sd);
	int err = 0;

	cam_info("%s\n", __func__);

	switch (ctrl->id) {
	case V4L2_CID_CAMERA_WHITE_BALANCE:
		ctrl->value = state->userset.white_balance;
		break;
	case V4L2_CID_CAMERA_CONTRAST:
		ctrl->value = state->userset.contrast;
		break;
	case V4L2_CID_CAMERA_SATURATION:
		ctrl->value = state->userset.saturation;
		break;
	case V4L2_CID_CAMERA_SHARPNESS:
		ctrl->value = state->userset.sharpness;
		break;
	case V4L2_CID_CAM_AUTO_FOCUS_RESULT:
		ctrl->value = s5k5ea_get_af(sd);
		break;
	case V4L2_CID_CAM_DATE_INFO_YEAR:
	case V4L2_CID_CAM_DATE_INFO_MONTH:
	case V4L2_CID_CAM_DATE_INFO_DATE:
	case V4L2_CID_CAM_SENSOR_VER:
	default:
		break;
	}

	return err;
}

static void s5k5ea_init_setting(struct v4l2_subdev *sd)
{
	struct s5k5ea_state *state = to_state(sd);
	int value;

	cam_info("%s\n", __func__);

	value = state->userset.brightness;
	if (value != EV_DEFAULT)
		s5k5ea_set_brightness(sd, state->userset.brightness);

	value = state->userset.white_balance;
	if (value != WHITE_BALANCE_AUTO)
		s5k5ea_set_white_balance(sd, value);

	value = state->userset.contrast;
	if (value != V4L2_CONTRAST_AUTO)
		s5k5ea_set_contrast(sd, value);

	value = state->userset.saturation;
	if (value != V4L2_SATURATION_DEFAULT)
		s5k5ea_set_saturation(sd, value);

	value = state->userset.sharpness;
	if (value != V4L2_SHARPNESS_DEFAULT)
		s5k5ea_set_sharpness(sd, value);

	value = state->userset.iso;
	if (value != V4L2_ISO_AUTO)
		s5k5ea_set_iso(sd, value);
}

static int s5k5ea_init(struct v4l2_subdev *sd, u32 val)
{
	struct s5k5ea_state *state = to_state(sd);
	struct i2c_client *client = g_s5k5ea_i2c_client;
	u16 read_value = 0;
	int ret = 0;

	cam_info("%s\n", __func__);

	state->size.width = state->size.height = state->size.code = 0;
	state->point.x = state->point.y = 0;
	state->userset.camera_mode   = S5K5EA_OPRMODE_VIDEO;
	state->userset.frame_mode    = FM_NORMAL;
	state->userset.scenemode    = V4L2_SCENE_MODE_NONE;
	state->userset.brightness   = EV_DEFAULT;
	state->userset.white_balance = WHITE_BALANCE_AUTO;
	state->userset.contrast     = V4L2_CONTRAST_AUTO;
	state->userset.saturation   = V4L2_SATURATION_DEFAULT;
	state->userset.sharpness    = V4L2_SHARPNESS_DEFAULT;
	state->userset.iso          = V4L2_ISO_AUTO;
	state->userset.framerate	= 30;
	state->preset_index			= 0;

	/* read Chip ID */
	s5k5ea_i2c_write_twobyte(client, 0xFCFC, 0xD000);
	s5k5ea_i2c_write_twobyte(client, 0x002C, 0x2000);
	s5k5ea_i2c_write_twobyte(client, 0x002E, 0x0000);
	s5k5ea_i2c_read_twobyte(client, 0x0F12, &read_value);

	cam_info("%s chip id = %04x\n", __func__, read_value);

	if (read_value != 0x5EA1) {
		cam_info("%s : Wrong chip ID %04X, Not 5EA1\n",
			__func__, read_value);
		return -1;
	}

	s5k5ea_i2c_write_twobyte(client, 0xFCFC, 0xD000);
	s5k5ea_i2c_write_twobyte(client, 0x002C, 0x2000);
	s5k5ea_i2c_write_twobyte(client, 0x002E, 0x0002);
	s5k5ea_i2c_read_twobyte(client, 0x0F12, &read_value);

	cam_info("%s hw version = %04x\n", __func__, read_value);

	ret = s5k5ea_apply_set(sd, &regs_set.init_reg_1);
	if (ret < 0) {
		err("%s: init_reg_1 failed\n", __func__);
		goto p_err;
	}

	msleep(20);

	s5k5ea_write_burst(client, regs_set.init_reg_2.reg,
		regs_set.init_reg_2.array_size);

	msleep(100);

p_err:
	return ret;
}

static int s5k5ea_s_ext_ctrls(struct v4l2_subdev *sd,
		struct v4l2_ext_controls *ctrls)
{
	cam_info("%s\n", __func__);

	return 0;
}


static int s5k5ea_s_power(struct v4l2_subdev *sd, int on)
{
	int ret = 0;

	cam_info("%s\n", __func__);

	if (on) {
		s5k5ea_init(sd, on);
		cam_info("\n\n ==============> [%s] : POWER - ON\n", __func__);
	} else {
		cam_info("\n\n ==============> [%s] : POWER - OFF\n", __func__);
	}

	return ret;
}

static int s5k5ea_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	cam_info("%s\n", __func__);

	return 0;
}

static int s5k5ea_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	cam_info("%s\n", __func__);

	return 0;
}

static const struct v4l2_subdev_core_ops s5k5ea_core_ops = {
	.g_ctrl         = s5k5ea_g_ctrl,
	.s_ctrl         = s5k5ea_s_ctrl,
	.init           = s5k5ea_init,  /* initializing API */
	.s_ext_ctrls    = s5k5ea_s_ext_ctrls,
	.s_power        = s5k5ea_s_power,
	.queryctrl      = s5k5ea_queryctrl,
	.querymenu      = s5k5ea_querymenu,
};

static int s5k5ea_g_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	cam_info("%s\n", __func__);

	return 0;
}

static int s5k5ea_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	struct s5k5ea_state *state = to_state(sd);
	struct i2c_client *client = g_s5k5ea_i2c_client;
	u16 format = PCFG_FORMAT_YUV;

	state->size.width	= fmt->width;
	state->size.height	= fmt->height;
	state->size.code	= fmt->code;

	if (fmt->code == V4L2_PIX_FMT_YUYV) {
		format = PCFG_FORMAT_YUV;
	} else if (fmt->code == V4L2_PIX_FMT_MJPEG) {
		format = PCFG_FORMAT_MJPEG;
	} else {
		cam_err("%s invalid pixel format %x\n", __func__, fmt->code);
		return -1;
	}

	cam_info("%s: width = %d, height = %d, format = %x(%d)\n",
			__func__, fmt->width, fmt->height, fmt->code, format);

	if (state->size.width == 2560 && state->size.height == 1920)
		state->userset.camera_mode = S5K5EA_OPRMODE_IMAGE;
	else
		state->userset.camera_mode = S5K5EA_OPRMODE_VIDEO;

	cam_info("%s: camera_mode: %s\n", __func__,
		(state->userset.camera_mode == S5K5EA_OPRMODE_IMAGE) ?
		"capture mode" : "video mode");

	state->preset_index = s5k5ea_get_index(sd);

	s5k5ea_i2c_write_twobyte(client, 0x0028, 0x2000);
	/* REG_xTC_PCFG_Format */
	s5k5ea_i2c_write_twobyte(client, 0x002A,
		PCFG_FMT_BASE + PCFG_SIZE * state->preset_index);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, format);

	return 0;
}


static int s5k5ea_get_index(struct v4l2_subdev *sd)
{
	struct s5k5ea_state *state = to_state(sd);
	int width, height, ret = -1;
	u32 i;

	cam_dbg("%s\n", __func__);

	width = state->size.width;
	height = state->size.height;

	mutex_lock(&state->lock);
	if (state->userset.camera_mode == S5K5EA_OPRMODE_VIDEO) {
		for (i = 0; i < ARRAY_SIZE(preview_size_list); ++i) {
			if ((preview_size_list[i].width == width) &&
				(preview_size_list[i].height == height)) {
				cam_info("%s %dx%d index found %d\n",
					__func__, width, height, i);
				ret = i;
				break;
			}
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(capture_size_list); ++i) {
			if ((capture_size_list[i].width == width) &&
				(capture_size_list[i].height == height)) {
				cam_info("%s %dx%d index found %d\n",
					__func__, width, height, i);
				ret = i;
				break;
			}
		}
	}

	if (ret < 0) {
		err("(%d x %d) is not supported", width, height);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	mutex_unlock(&state->lock);
	return ret;
}

static void s5k5ea_preview_setting(struct v4l2_subdev *sd, int value)
{
	struct s5k5ea_state *state = to_state(sd);

	cam_info("%s\n", __func__);

	if (value == S5K5EA_PREVIEW_1280_960 ||
		value == S5K5EA_PREVIEW_640_480) {
		cam_info("value = (%d), Input = (2560x1920)", value);
		s5k5ea_apply_set(sd, &regs_set.preview_input
			[S5K5EA_PREVIEW_INPUT_2560_1920]);
	} else if (value == S5K5EA_PREVIEW_1920_1080) {
		cam_info("value = (%d), Input = (1920x1080)", value);
		s5k5ea_apply_set(sd, &regs_set.preview_input
			[S5K5EA_PREVIEW_INPUT_1920_1080]);
	} else if (value == S5K5EA_PREVIEW_1280_720) {
		cam_info("value = (%d), Input = (2560x1440)", value);
		s5k5ea_apply_set(sd, &regs_set.preview_input
			[S5K5EA_PREVIEW_INPUT_2560_1440]);
	}

	cam_info("Real Size = (%dx%d)\n",
		state->size.width, state->size.height);
}


static int s5k5ea_start_preview(struct v4l2_subdev *sd)
{
	struct s5k5ea_state *state = to_state(sd);
	struct i2c_client *client = g_s5k5ea_i2c_client;
	int value = state->preset_index;

	if (value < 0) {
		err("Invalid index %d", value);
		return -EINVAL;
	}

	cam_info("%s index(%d)\n", __func__, value);

	s5k5ea_preview_setting(sd, value);

	s5k5ea_i2c_write_twobyte(client, 0x0028, 0x2000);
	/* REG_TC_AF_AfCmdParam */
	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x02CA);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0000);

	/* REG_TC_AF_AfCmd */
	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x02C8);
	/* Continuous AF */
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0006);

	/*REG_TC_GP_ActivePrevConfig */
	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x02A0);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, value);

	/* REG_TC_GP_PrevConfigChanged */
	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x02A2);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0001);

	/* REG_TC_GP_PrevOpenAfterChange */
	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x02A4);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0001);

	/* REG_TC_GP_NewConfigSync */
	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x0288);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0001);
	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x0278);

	/* REG_TC_GP_EnablePreview(1 : Enable) */
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0001);
	/* REG_TC_GP_EnablePreviewChanged */
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0001);

	return 0;
}


static int s5k5ea_stop_preview(struct v4l2_subdev *sd)
{
	struct i2c_client *client = g_s5k5ea_i2c_client;

	cam_info("[%s]\n", __func__);

	s5k5ea_i2c_write_twobyte(client, 0x0028, 0x2000);
	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x0278);
	/* REG_TC_GP_EnablePreview(0 : Disable) */
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0000);
	/* REG_TC_GP_EnablePreviewChanged */
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0001);

	return 0;
}


static void s5k5ea_capture_setting(struct v4l2_subdev *sd)
{
	struct i2c_client *client = g_s5k5ea_i2c_client;

	cam_info("%s\n", __func__);

	s5k5ea_i2c_write_twobyte(client, 0x0028, 0x2000);
	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x0292);
	/* REG_TC_GP_CapReqInputWidth */
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0A00);
	/* REG_TC_GP_CapReqInputHeight */
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0780);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0000);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0000);

	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x029E);
	/* REG_TC_GP_bUseReqInputInCap */
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0001);

	s5k5ea_i2c_write_twobyte(client, 0x002A, 0x04D8);
	/* REG_TC_PZOOM_CapZoomReqInputWidth */
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0A00);
	/* REG_TC_PZOOM_CapZoomReqInputHeight */
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0780);
	/* REG_TC_PZOOM_CapZoomReqInputWidthOfs */
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0000);
	/* REG_TC_PZOOM_CapZoomReqInputHeightOfs */
	s5k5ea_i2c_write_twobyte(client, 0x0F12, 0x0000);
}


static int s5k5ea_start_capture(struct v4l2_subdev *sd)
{
	struct s5k5ea_state *state = to_state(sd);
	struct i2c_client *client = g_s5k5ea_i2c_client;
	int value = state->preset_index;

	if (value < 0) {
		err("Invalid index %d", value);
		return -EINVAL;
	}

	cam_info("%s index(%d)\n", __func__, value);

	s5k5ea_capture_setting(sd);

	s5k5ea_i2c_write_twobyte(client, 0x0028, 0x2000);
	s5k5ea_i2c_write_twobyte(client, 0x002a, 0x02a8);
	s5k5ea_i2c_write_twobyte(client, 0x0f12, value);
	s5k5ea_i2c_write_twobyte(client, 0x002a, 0x02aa);
	s5k5ea_i2c_write_twobyte(client, 0x0f12, 0x0001);
	s5k5ea_i2c_write_twobyte(client, 0x002a, 0x0288);
	s5k5ea_i2c_write_twobyte(client, 0x0f12, 0x0001);
	s5k5ea_i2c_write_twobyte(client, 0x002a, 0x027c);
	s5k5ea_i2c_write_twobyte(client, 0x0f12, 0x0001);
	s5k5ea_i2c_write_twobyte(client, 0x002a, 0x027e);
	s5k5ea_i2c_write_twobyte(client, 0x0f12, 0x0001);

	return 0;
}

static int s5k5ea_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct s5k5ea_state *state = to_state(sd);
	int ret;

	cam_info("%s\n", __func__);

	if (enable)
		if (state->userset.camera_mode == S5K5EA_OPRMODE_VIDEO)
			ret = s5k5ea_start_preview(sd);
		else
			ret = s5k5ea_start_capture(sd);
	else
		ret = s5k5ea_stop_preview(sd);

	s5k5ea_delay(sd);

	return ret;
}

static int s5k5ea_g_parm(struct v4l2_subdev *sd,
		struct v4l2_streamparm *param)
{
	cam_info("%s\n", __func__);

	return 0;
}

static int s5k5ea_s_parm(struct v4l2_subdev *sd,
		struct v4l2_streamparm *param)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct v4l2_captureparm *cp;
	struct v4l2_fract *tpf;
	u64 framerate;

	BUG_ON(!sd);
	BUG_ON(!param);

	cp = &param->parm.capture;
	tpf = &cp->timeperframe;

	if (!tpf->numerator) {
		err("numerator is 0");
		ret = -EINVAL;
		goto p_err;
	}

	framerate = tpf->denominator;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(sd);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = CALL_MOPS(module, s_duration, sd, framerate);
	if (ret) {
		err("s_duration is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int s5k5ea_enum_framesizes(struct v4l2_subdev *sd,
		struct v4l2_frmsizeenum *fsize)
{
	cam_info("%s\n", __func__);

	return 0;
}

/*
 * Clock configuration
 * Configure expected MCLK from host and return EINVAL if not supported clock
 * frequency is expected
 *  freq : in Hz
 *  flag : not supported for now
 */
static int s5k5ea_s_crystal_freq(struct v4l2_subdev *sd,
	u32  freq, u32 flags)
{
	cam_info("%s\n", __func__);

	return 0;
}

static int s5k5ea_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *fival)
{
	cam_info("%s\n", __func__);

	return 0;
}

static const struct v4l2_subdev_video_ops s5k5ea_video_ops = {
	.g_mbus_fmt         = s5k5ea_g_fmt,
	.s_mbus_fmt         = s5k5ea_s_fmt,
	.g_parm             = s5k5ea_g_parm,
	.s_parm             = s5k5ea_s_parm,
	.s_stream           = s5k5ea_s_stream,
	.enum_framesizes    = s5k5ea_enum_framesizes,
	.s_crystal_freq     = s5k5ea_s_crystal_freq,
	.enum_frameintervals = s5k5ea_enum_frameintervals,
};

/* get format by flite video device command */
static int s5k5ea_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		struct v4l2_subdev_format *fmt)
{
	struct s5k5ea_state *state = to_state(sd);
	struct v4l2_mbus_framefmt format;

	cam_info("%s\n", __func__);

	format.width    = state->size.width;
	format.height   = state->size.height;
	format.code     = state->size.code;

	format.field        = V4L2_FIELD_NONE;
	format.colorspace   = V4L2_COLORSPACE_JPEG;

	fmt->format = format;

	return 0;
}

/* set format by flite video device command */
static int s5k5ea_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		struct v4l2_subdev_format *fmt)
{
	struct s5k5ea_state *state = to_state(sd);
	struct v4l2_mbus_framefmt *format = &fmt->format;

	cam_info("%s\n", __func__);

	state->size.width   = format->width;
	state->size.height  = format->height;
	state->size.code    = format->code;

	return 0;
}

static int s5k5ea_enum_mbus_code(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh,
		struct v4l2_subdev_mbus_code_enum *code)
{
	cam_info("%s\n", __func__);

	if (!code || code->index >= SIZE_DEFAULT_FFMT)
		return -EINVAL;

	code->code = default_fmt[code->index].code;
	return 0;
}

static struct v4l2_subdev_pad_ops s5k5ea_pad_ops = {
	.enum_mbus_code = s5k5ea_enum_mbus_code,
	.get_fmt        = s5k5ea_get_fmt,
	.set_fmt        = s5k5ea_set_fmt,
};

static const struct v4l2_subdev_ops s5k5ea_ops = {
	.core   = &s5k5ea_core_ops,
	.pad    = &s5k5ea_pad_ops,
	.video  = &s5k5ea_video_ops,
};



/*
 * @ brief
 * frame duration time
 * @ unit
 * nano second
 * @ remarks
 */
int sensor_5ea_s_duration(struct v4l2_subdev *subdev, u64 framerate)
{
	int ret = 0;
	u32 frametime;
	struct i2c_client *client = g_s5k5ea_i2c_client;
	struct s5k5ea_state *state = to_state(subdev);

	cam_info("%s\n", __func__);
	BUG_ON(!subdev);

	if (!framerate) {
		err("framerate is 0");
		ret = -EINVAL;
		goto p_err;
	}

	frametime = 10000 / (u32)framerate;
	state->userset.framerate = (int)framerate;

	cam_info("%s framerate=%lld, frametime=%d\n",
		__func__, framerate, frametime);

	s5k5ea_i2c_write_twobyte(client, 0x0028, 0x2000);

	/* XTC_PCFG_usMaxFrTimeMsecMult10 */
	s5k5ea_i2c_write_twobyte(client, 0x002A,
		PCFG_FRTIME_BASE + PCFG_SIZE * state->preset_index);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, frametime);
	s5k5ea_i2c_write_twobyte(client, 0x0F12, frametime);

p_err:
	return ret;
}

int sensor_5ea_g_min_duration(struct v4l2_subdev *subdev)
{
	int ret = 0;

	cam_info("%s\n", __func__);

	return ret;
}

int sensor_5ea_g_max_duration(struct v4l2_subdev *subdev)
{
	int ret = 0;

	cam_info("%s\n", __func__);

	return ret;
}

int sensor_5ea_s_exposure(struct v4l2_subdev *subdev, u64 exposure)
{
	int ret = 0;
	u8 value;
	struct fimc_is_module_enum *sensor;
	struct i2c_client *client;

	cam_info("%s(%d)\n", __func__, (u32)exposure);
	BUG_ON(!subdev);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!sensor)) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = sensor->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	value = exposure & 0xFF;

p_err:
	return ret;
}

int sensor_5ea_g_min_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;

	cam_info("%s\n", __func__);

	return ret;
}

int sensor_5ea_g_max_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;

	cam_info("%s\n", __func__);

	return ret;
}

int sensor_5ea_s_again(struct v4l2_subdev *subdev, u64 sensitivity)
{
	int ret = 0;

	cam_info("%s\n", __func__);

	return ret;
}

int sensor_5ea_g_min_again(struct v4l2_subdev *subdev)
{
	int ret = 0;

	cam_info("%s\n", __func__);

	return ret;
}

int sensor_5ea_g_max_again(struct v4l2_subdev *subdev)
{
	int ret = 0;

	cam_info("%s\n", __func__);

	return ret;
}

int sensor_5ea_s_dgain(struct v4l2_subdev *subdev)
{
	int ret = 0;

	cam_info("%s\n", __func__);

	return ret;
}

int sensor_5ea_g_min_dgain(struct v4l2_subdev *subdev)
{
	int ret = 0;

	cam_info("%s\n", __func__);

	return ret;
}

int sensor_5ea_g_max_dgain(struct v4l2_subdev *subdev)
{
	int ret = 0;

	cam_info("%s\n", __func__);

	return ret;
}

struct fimc_is_sensor_ops module_5ea_ops = {
/*	.stream_on		= sensor_5ea_stream_on,
	.stream_off		= sensor_5ea_stream_off, */
	.s_duration		= sensor_5ea_s_duration,
	.g_min_duration	= sensor_5ea_g_min_duration,
	.g_max_duration	= sensor_5ea_g_max_duration,
	.s_exposure		= sensor_5ea_s_exposure,
	.g_min_exposure	= sensor_5ea_g_min_exposure,
	.g_max_exposure	= sensor_5ea_g_max_exposure,
	.s_again		= sensor_5ea_s_again,
	.g_min_again	= sensor_5ea_g_min_again,
	.g_max_again	= sensor_5ea_g_max_again,
	.s_dgain		= sensor_5ea_s_dgain,
	.g_min_dgain	= sensor_5ea_g_min_dgain,
	.g_max_dgain	= sensor_5ea_g_max_dgain
};

static int s5k5ea_link_setup(struct media_entity *entity,
		const struct media_pad *local,
		const struct media_pad *remote, u32 flags)
{
	cam_info("%s\n", __func__);

	return 0;
}


static const struct media_entity_operations s5k5ea_media_ops = {
	    .link_setup = s5k5ea_link_setup,
};

static int s5k5ea_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct v4l2_subdev_format format;

	cam_info("%s\n", __func__);

	memset(&format, 0, sizeof(format));
	format.pad = 0;
	format.which = fh ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	format.format.code = DEFAULT_SENSOR_CODE;
	format.format.width = DEFAULT_SENSOR_WIDTH;
	format.format.height = DEFAULT_SENSOR_HEIGHT;

	/*  s5k5ea_set_fmt(sd, fh, &format); */

	return 0;
}

static int s5k5ea_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	cam_info("%s\n", __func__);

	return 0;
}

static int s5k5ea_registered(struct v4l2_subdev *sd)
{
	cam_info("%s\n", __func__);

	return 0;
}

static void s5k5ea_unregistered(struct v4l2_subdev *sd)
{
	cam_info("%s\n", __func__);
}


static const struct v4l2_subdev_internal_ops s5k5ea_subdev_internal_ops = {
	.open           = s5k5ea_open,
	.close          = s5k5ea_close,
	.registered     = s5k5ea_registered,
	.unregistered   = s5k5ea_unregistered,
};


int sensor_5ea_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;
	struct sensor_open_extended *ext;
	struct v4l2_subdev *sd;
	struct s5k5ea_state *state;

	cam_info("%s\n", __func__);

	BUG_ON(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	device = &core->sensor[SENSOR_S5K5EA_INSTANCE];

	device->pdata->gpio_cfg(device->pdev, SENSOR_SCENARIO_EXTERNAL, 0);
	ret = s5k5ea_i2c_write_twobyte(client, 0xFCFC, 0xD000);
	if (ret < 0) {
		cam_warn("%s failed to write to sensor(%d)", __func__, ret);
		ret = -ENODEV;
		goto p_err;
	}

	state = kzalloc(sizeof(struct s5k5ea_state), GFP_KERNEL);
	if (state == NULL) {
		dev_err(&client->dev, "S5K5EA probe error : kzalloc\n");
		return -ENOMEM;
	}

	sd = &state->sd;
	g_s5k5ea_i2c_client = client;

	/* S5K5EA */
	module = &device->module_enum[atomic_read
		(&core->resourcemgr.rsccount_module)];
	atomic_inc(&core->resourcemgr.rsccount_module);
	module->id = SENSOR_S5K5EA_NAME;
	module->subdev = sd;
	module->device = SENSOR_S5K5EA_INSTANCE;
	module->ops = &module_5ea_ops;
	module->client = client;
	module->active_width = 1280;
	module->active_height = 960;
	module->pixel_width = module->active_width + 16;
	module->pixel_height = module->active_height + 10;
	module->max_framerate = 30;
	module->position = SENSOR_POSITION_FRONT;
	module->setfile_name = "setfile_6b2.bin";
	module->cfgs = ARRAY_SIZE(config_5ea);
	module->cfg = config_5ea;
	module->private_data = kzalloc(sizeof(struct fimc_is_module_5ea),
		GFP_KERNEL);
	if (!module->private_data) {
		err("private_data is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	ext = &module->ext;
	ext->mipi_lane_num = 2;
	ext->I2CSclk = I2C_L0;
	ext->sensor_con.product_name = SENSOR_NAME_S5K5EA;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = SENSOR_CONTROL_I2C1;
	ext->sensor_con.peri_setting.i2c.slave_address = 0x2D;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;
	ext->companion_con.product_name = COMPANION_NAME_NOTHING;

	v4l2_i2c_subdev_init(sd, client, &s5k5ea_ops);

	/* Registering subdev */
	state->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&sd->entity, 1, &state->pad, 0);
	if (ret) {
		dev_err(&client->dev, "S5K5EA probe error : media entity\n");
		return ret;
	}
	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	sd->entity.ops = &s5k5ea_media_ops;
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->internal_ops = &s5k5ea_subdev_internal_ops;

	/* Initize */
	state->size.width = state->size.height = state->size.code = 0;
	state->point.x = state->point.y = 0;
	mutex_init(&state->lock);
	mutex_init(&state->i2c_lock);
	state->userset.camera_mode   = S5K5EA_OPRMODE_VIDEO;
	state->userset.frame_mode    = FM_NORMAL;
	state->userset.scenemode    = V4L2_SCENE_MODE_NONE;
	state->userset.brightness   = EV_DEFAULT;
	state->userset.white_balance = WHITE_BALANCE_AUTO;
	state->userset.contrast     = V4L2_CONTRAST_AUTO;
	state->userset.saturation   = V4L2_SATURATION_DEFAULT;
	state->userset.sharpness    = V4L2_SHARPNESS_DEFAULT;
	state->userset.iso          = V4L2_ISO_AUTO;
	state->userset.framerate	= 30;
	state->preset_index			= 0;

	v4l2_set_subdevdata(sd, module);
	v4l2_set_subdev_hostdata(sd, device);
	snprintf(sd->name, V4L2_SUBDEV_NAME_SIZE,
		"sensor-subdev.%d", module->id);

p_err:
	device->pdata->gpio_cfg(device->pdev, SENSOR_SCENARIO_EXTERNAL, 1);
	info("%s(%d)\n", __func__, ret);
	return ret;
}

static int sensor_5ea_remove(struct i2c_client *client)
{
	int ret = 0;

	cam_info("%s\n", __func__);

	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_sensor_5ea_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-sensor-5ea",
	},
	{},
};
#endif

static const struct i2c_device_id sensor_5ea_idt[] = {
	{ SENSOR_NAME, 0 },
};

static struct i2c_driver sensor_5ea_driver = {
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = exynos_fimc_is_sensor_5ea_match
#endif
	},
	.probe	= sensor_5ea_probe,
	.remove	= sensor_5ea_remove,
	.id_table = sensor_5ea_idt
};

static int __init sensor_5ea_load(void)
{
	cam_info("%s\n", __func__);
	return i2c_add_driver(&sensor_5ea_driver);
}

static void __exit sensor_5ea_unload(void)
{
	cam_info("%s\n", __func__);
	i2c_del_driver(&sensor_5ea_driver);
}

module_init(sensor_5ea_load);
module_exit(sensor_5ea_unload);

MODULE_AUTHOR("Gilyeon lim");
MODULE_DESCRIPTION("Sensor 5EA driver");
MODULE_LICENSE("GPL v2");
