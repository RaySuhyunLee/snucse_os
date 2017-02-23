/*
 * MIPI-DSI based eh400wv TFT LCD 3.97 inch panel driver.
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * Chanho Park <chanho61.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <linux/backlight.h>

#define MIPI_DCS_CUSTOM_HBM_MODE		0x53
#define MIPI_DCS_CUSTOM_HBM_MODE_NONE		0x28
#define MIPI_DCS_CUSTOM_HBM_MODE_MEDIUM		0x68
#define MIPI_DCS_CUSTOM_HBM_MODE_MAX		0xe8

#define MIPI_DCS_CUSTOM_DIMMING_SET		0x51

struct eh400wv {
	struct device *dev;
	struct drm_panel panel;

	struct regulator_bulk_data supplies[2];
	int reset_gpio;
	int psr_te_gpio;
	u32 power_on_delay;
	u32 reset_delay;
	u32 init_delay;
	bool flip_horizontal;
	bool flip_vertical;
	struct videomode vm;
	u32 width_mm;
	u32 height_mm;
	bool is_power_on;

	u8 id[3];
	/* This field is tested by functions directly accessing DSI bus before
	 * transfer, transfer is skipped if it is set. In case of transfer
	 * failure or unexpected response the field is set to error value.
	 * Such construct allows to eliminate many checks in higher level
	 * functions.
	 */
	int error;
};

static inline struct eh400wv *panel_to_eh400wv(struct drm_panel *panel)
{
	return container_of(panel, struct eh400wv, panel);
}

static int eh400wv_clear_error(struct eh400wv *ctx)
{
	int ret = ctx->error;

	ctx->error = 0;
	return ret;
}

static void eh400wv_dcs_write(struct eh400wv *ctx, const void *data,
		size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return;

	ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing dcs seq: %*ph\n", ret,
			(int)len, data);
		ctx->error = ret;
	}
}

#define eh400wv_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	eh400wv_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define eh400wv_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	eh400wv_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define POWER_SEQ_COUNT		16
#define POWER_SEQ_SLEEP_NUM	7
#define GAMMA_TABLE_COUNT	6
#define DISPLAY_PARAM_COUNT	12

static const u8 power_sequence_tables[][10] = {
	{ 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01},
	{ 0xB0, 0x05, 0x05, 0x05 },
	{ 0xB1, 0x05, 0x05, 0x05 },
	{ 0xB2, 0x03, 0x03, 0x03 },
	{ 0xB3, 0x0a, 0x0a, 0x0a },
	{ 0xB4, 0x2d, 0x2d, 0x2d },
	{ 0xB5, 0x08, 0x08, 0x08 },
	{ 0xB6, 0x34, 0x34, 0x34 },
	{ 0xB7, 0x24, 0x24, 0x24 },
	{ 0xB8, 0x24, 0x24, 0x24 },
	{ 0xB9, 0x34, 0x34, 0x34 },
	{ 0xBA, 0x14, 0x14, 0x14 },
	{ 0xBC, 0x00, 0x68, 0x00 },
	{ 0xBD, 0x00, 0x68, 0x00 },
	{ 0xBE, 0x00, 0x0C },
	{ 0xBF, 0x01},
};

static const u8 gamma_tables[GAMMA_TABLE_COUNT][53] = {
	{
		0xD1, 0x00, 0x00, 0x00, 0x0D, 0x00,
		0x25, 0x00, 0x3b, 0x00, 0x48,
		0x00, 0x68, 0x00, 0x80, 0x00,
		0xB0, 0x00, 0xDC, 0x01, 0x25,
		0x01, 0x58, 0x01, 0xB0, 0x01,
		0xF8, 0x01, 0xF9, 0x02, 0x3C,
		0x02, 0x88, 0x02, 0xB6, 0x02,
		0xEA, 0x03, 0x0c, 0x03, 0x34,
		0x03, 0x50, 0x03, 0x71, 0x03,
		0x85, 0x03, 0xa1, 0x03, 0xbc,
		0x03, 0x83
	}, {
		0xD2, 0x00, 0x00, 0x00, 0x0D, 0x00,
		0x25, 0x00, 0x3b, 0x00, 0x48,
		0x00, 0x68, 0x00, 0x80, 0x00,
		0xB0, 0x00, 0xDC, 0x01, 0x25,
		0x01, 0x58, 0x01, 0xB0, 0x01,
		0xF8, 0x01, 0xF9, 0x02, 0x3C,
		0x02, 0x88, 0x02, 0xB6, 0x02,
		0xEA, 0x03, 0x0c, 0x03, 0x34,
		0x03, 0x50, 0x03, 0x71, 0x03,
		0x85, 0x03, 0xa1, 0x03, 0xbc,
		0x03, 0x83,
	}, {
		0xD3, 0x00, 0x00, 0x00, 0x0D, 0x00,
		0x25, 0x00, 0x3b, 0x00, 0x48,
		0x00, 0x68, 0x00, 0x80, 0x00,
		0xB0, 0x00, 0xDC, 0x01, 0x25,
		0x01, 0x58, 0x01, 0xB0, 0x01,
		0xF8, 0x01, 0xF9, 0x02, 0x3C,
		0x02, 0x88, 0x02, 0xB6, 0x02,
		0xEA, 0x03, 0x0c, 0x03, 0x34,
		0x03, 0x50, 0x03, 0x71, 0x03,
		0x85, 0x03, 0xa1, 0x03, 0xbc,
		0x03, 0x83,
	}, {
		0xD4, 0x00, 0x00, 0x00, 0x0D, 0x00,
		0x25, 0x00, 0x3b, 0x00, 0x48,
		0x00, 0x68, 0x00, 0x80, 0x00,
		0xB0, 0x00, 0xDC, 0x01, 0x25,
		0x01, 0x58, 0x01, 0xB0, 0x01,
		0xF8, 0x01, 0xF9, 0x02, 0x3C,
		0x02, 0x88, 0x02, 0xB6, 0x02,
		0xEA, 0x03, 0x0c, 0x03, 0x34,
		0x03, 0x50, 0x03, 0x71, 0x03,
		0x85, 0x03, 0xa1, 0x03, 0xbc,
		0x03, 0x83,
	}, {
		0xD5, 0x00, 0x00, 0x00, 0x0D, 0x00,
		0x25, 0x00, 0x3b, 0x00, 0x48,
		0x00, 0x68, 0x00, 0x80, 0x00,
		0xB0, 0x00, 0xDC, 0x01, 0x25,
		0x01, 0x58, 0x01, 0xB0, 0x01,
		0xF8, 0x01, 0xF9, 0x02, 0x3C,
		0x02, 0x88, 0x02, 0xB6, 0x02,
		0xEA, 0x03, 0x0c, 0x03, 0x34,
		0x03, 0x50, 0x03, 0x71, 0x03,
		0x85, 0x03, 0xa1, 0x03, 0xbc,
		0x03, 0x83,
	}, {
		0xD6, 0x00, 0x00, 0x00, 0x0D, 0x00,
		0x25, 0x00, 0x3b, 0x00, 0x48,
		0x00, 0x68, 0x00, 0x80, 0x00,
		0xB0, 0x00, 0xDC, 0x01, 0x25,
		0x01, 0x58, 0x01, 0xB0, 0x01,
		0xF8, 0x01, 0xF9, 0x02, 0x3C,
		0x02, 0x88, 0x02, 0xB6, 0x02,
		0xEA, 0x03, 0x0c, 0x03, 0x34,
		0x03, 0x50, 0x03, 0x71, 0x03,
		0x85, 0x03, 0xa1, 0x03, 0xbc,
		0x03, 0x83
	},
};

static void eh400wv_apply_power_cond(struct eh400wv *ctx)
{
	int i;

	static const u8 power_b0_b6[][4] = {
		{ 0xB0, 0x05, 0x05, 0x05 },
		{ 0xB1, 0x05, 0x05, 0x05 },
		{ 0xB2, 0x03, 0x03, 0x03 },
		{ 0xB3, 0x0a, 0x0a, 0x0a },
		{ 0xB4, 0x2d, 0x2d, 0x2d },
		{ 0xB5, 0x08, 0x08, 0x08 },
		{ 0xB6, 0x34, 0x34, 0x34 },
	};

	static const u8 power_b7_bd[][4] = {
		{ 0xB7, 0x24, 0x24, 0x24 },
		{ 0xB8, 0x24, 0x24, 0x24 },
		{ 0xB9, 0x34, 0x34, 0x34 },
		{ 0xBA, 0x14, 0x14, 0x14 },
		{ 0xBC, 0x00, 0x68, 0x00 },
		{ 0xBD, 0x00, 0x68, 0x00 },
	};

	eh400wv_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
	for (i = 0; i < ARRAY_SIZE(power_b0_b6); i++)
		eh400wv_dcs_write(ctx, power_b0_b6[i],
				ARRAY_SIZE(power_b0_b6[i]));

	msleep(20);

	for (i = 0; i < ARRAY_SIZE(power_b7_bd); i++)
		eh400wv_dcs_write(ctx, power_b7_bd[i],
				ARRAY_SIZE(power_b7_bd[i]));

	eh400wv_dcs_write_seq_static(ctx, 0xBE, 0x00, 0x0C);
	eh400wv_dcs_write_seq_static(ctx, 0xBF, 0x01);
}

static void eh400wv_gamma_setting(struct eh400wv *ctx)
{
	int i;

	for (i = 0; i < GAMMA_TABLE_COUNT; i++) {
		eh400wv_dcs_write(ctx, gamma_tables[i],
				ARRAY_SIZE(gamma_tables[i]));
	}
}

static void eh400wv_apply_display_parameter(struct eh400wv *ctx)
{
	u8 rgb_settings[6] = { 0xB0, 0x00, ctx->vm.vback_porch,
		ctx->vm.vfront_porch, ctx->vm.hback_porch,
		ctx->vm.hfront_porch };

	eh400wv_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	/* Porch setting from videomode */
	eh400wv_dcs_write(ctx, rgb_settings, ARRAY_SIZE(rgb_settings));
	eh400wv_dcs_write_seq_static(ctx, 0xB1, 0xFC, 0x00);
	eh400wv_dcs_write_seq_static(ctx, 0xB6, 0x08);
	eh400wv_dcs_write_seq_static(ctx, 0xB8, 0x00, 0x00, 0x00, 0x00);
	eh400wv_dcs_write_seq_static(ctx, 0xBC, 0x00, 0x00, 0x00);
	eh400wv_dcs_write_seq_static(ctx, 0xCC, 0x03);

	eh400wv_dcs_write_seq_static(ctx, MIPI_DCS_SET_ADDRESS_MODE);
	eh400wv_dcs_write_seq_static(ctx, MIPI_DCS_SET_PIXEL_FORMAT, 0x77);

	eh400wv_dcs_write_seq_static(ctx, MIPI_DCS_EXIT_SLEEP_MODE);

	msleep(120);
}

static void eh400wv_set_maximum_return_packet_size(struct eh400wv *ctx,
						   u16 size)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (ctx->error < 0)
		return;

	ret = mipi_dsi_set_maximum_return_packet_size(dsi, size);
	if (ret < 0) {
		dev_err(ctx->dev,
			"error %d setting maximum return packet size to %d\n",
			ret, size);
		ctx->error = ret;
	}
}

static int eh400wv_dcs_read(struct eh400wv *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (ctx->error < 0)
		return ctx->error;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void eh400wv_read_mtp_id(struct eh400wv *ctx)
{
	int ret;
	int id_len = ARRAY_SIZE(ctx->id);

	ret = eh400wv_dcs_read(ctx, 0x04, ctx->id, id_len);
	if (ret < id_len || ctx->error < 0) {
		dev_err(ctx->dev, "read id failed\n");
		ctx->error = -EIO;
		return;
	}
}

static void eh400wv_panel_init(struct eh400wv *ctx)
{
	eh400wv_apply_power_cond(ctx);
	eh400wv_set_maximum_return_packet_size(ctx, 3);
	eh400wv_read_mtp_id(ctx);
	if (ctx->error != 0)
		return;
	eh400wv_gamma_setting(ctx);
	eh400wv_apply_display_parameter(ctx);
}

static int eh400wv_power_on(struct eh400wv *ctx)
{
	int ret;

	if (ctx->is_power_on)
		return 0;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0)
		return ret;

	msleep(ctx->power_on_delay);

	gpio_direction_output(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);
	gpio_set_value(ctx->reset_gpio, 1);

	msleep(ctx->reset_delay);

	gpio_direction_output(ctx->psr_te_gpio, 0);
	usleep_range(20000, 21000);
	gpio_set_value(ctx->psr_te_gpio, 1);

	ctx->is_power_on = true;

	return 0;
}

static int eh400wv_power_off(struct eh400wv *ctx)
{
	if (!ctx->is_power_on)
		return 0;

	gpio_set_value(ctx->psr_te_gpio, 0);

	gpio_set_value(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);

	regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	ctx->is_power_on = false;

	return 0;
}

static int eh400wv_disable(struct drm_panel *panel)
{
	struct eh400wv *ctx = panel_to_eh400wv(panel);

	eh400wv_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	if (ctx->error != 0)
		return ctx->error;

	eh400wv_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	if (ctx->error != 0)
		return ctx->error;

	msleep(40);

	return 0;
}

static int eh400wv_unprepare(struct drm_panel *panel)
{
	struct eh400wv *ctx = panel_to_eh400wv(panel);
	int ret;

	ret = eh400wv_power_off(ctx);
	if (ret)
		return ret;

	eh400wv_clear_error(ctx);

	return 0;
}

static int eh400wv_prepare(struct drm_panel *panel)
{
	struct eh400wv *ctx = panel_to_eh400wv(panel);
	int ret;

	ret = eh400wv_power_on(ctx);
	if (ret < 0)
		return ret;

	eh400wv_panel_init(ctx);
	ret = ctx->error;

	if (ret < 0)
		eh400wv_unprepare(panel);

	return ret;
}

static int eh400wv_enable(struct drm_panel *panel)
{
	struct eh400wv *ctx = panel_to_eh400wv(panel);

	eh400wv_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_ON);
	if (ctx->error != 0)
		return ctx->error;

	eh400wv_dcs_write_seq_static(ctx, MIPI_DCS_WRITE_MEMORY_START);
	if (ctx->error != 0)
		return ctx->error;

	return 0;
}

static int eh400wv_get_modes(struct drm_panel *panel)
{
	struct drm_connector *connector = panel->connector;
	struct eh400wv *ctx = panel_to_eh400wv(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_create(connector->dev);
	if (!mode) {
		DRM_ERROR("failed to create a new display mode\n");
		return 0;
	}

	drm_display_mode_from_videomode(&ctx->vm, mode);
	mode->width_mm = ctx->width_mm;
	mode->height_mm = ctx->height_mm;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs eh400wv_drm_funcs = {
	.disable = eh400wv_disable,
	.unprepare = eh400wv_unprepare,
	.prepare = eh400wv_prepare,
	.enable = eh400wv_enable,
	.get_modes = eh400wv_get_modes,
};

static int eh400wv_parse_dt(struct eh400wv *ctx)
{
	struct device *dev = ctx->dev;
	struct device_node *np = dev->of_node;
	int ret;

	ret = of_get_videomode(np, &ctx->vm, 0);
	if (ret < 0)
		return ret;

	of_property_read_u32(np, "power-on-delay", &ctx->power_on_delay);
	of_property_read_u32(np, "reset-delay", &ctx->reset_delay);
	of_property_read_u32(np, "init-delay", &ctx->init_delay);
	of_property_read_u32(np, "panel-width-mm", &ctx->width_mm);
	of_property_read_u32(np, "panel-height-mm", &ctx->height_mm);

	ctx->flip_horizontal = of_property_read_bool(np, "flip-horizontal");
	ctx->flip_vertical = of_property_read_bool(np, "flip-vertical");

	return 0;
}

static int eh400wv_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct eh400wv *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(struct eh400wv), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;

	ctx->is_power_on = false;
	dsi->lanes = 2;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST
		| MIPI_DSI_MODE_VIDEO_HFP | MIPI_DSI_MODE_VIDEO_HBP
		| MIPI_DSI_MODE_VIDEO_HSA | MIPI_DSI_MODE_VSYNC_FLUSH
		| MIPI_DSI_MODE_VIDEO_AUTO_VERT;

	ret = eh400wv_parse_dt(ctx);
	if (ret < 0)
		return ret;

	ctx->supplies[0].supply = "vdd3";
	ctx->supplies[1].supply = "vci";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ctx->supplies),
				      ctx->supplies);
	if (ret < 0)
		dev_warn(dev, "failed to get regulators: %d\n", ret);

	ctx->reset_gpio = of_get_named_gpio(dev->of_node, "reset-gpio", 0);
	if (ctx->reset_gpio < 0) {
		dev_err(dev, "cannot get reset-gpios %d\n",
			ctx->reset_gpio);
		return ctx->reset_gpio;
	}

	ret = devm_gpio_request(dev, ctx->reset_gpio, "reset-gpio");
	if (ret) {
		dev_err(dev, "failed to request reset-gpio\n");
		return ret;
	}

	ctx->psr_te_gpio = of_get_named_gpio(dev->of_node, "psr-te-gpio", 0);
	if (ctx->psr_te_gpio < 0) {
		dev_err(dev, "cannot get psr te gpios %d\n",
			ctx->psr_te_gpio);
		return ctx->psr_te_gpio;
	}

	ret = devm_gpio_request(dev, ctx->psr_te_gpio, "psr-te-gpio");
	if (ret) {
		dev_err(dev, "failed to request psr te gpio\n");
		return ret;
	}

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &eh400wv_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

	return ret;
}

static int eh400wv_remove(struct mipi_dsi_device *dsi)
{
	struct eh400wv *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
	eh400wv_power_off(ctx);

	return 0;
}

static void eh400wv_shutdown(struct mipi_dsi_device *dsi)
{
	struct eh400wv *ctx = mipi_dsi_get_drvdata(dsi);

	eh400wv_power_off(ctx);
}

static const struct of_device_id eh400wv_of_match[] = {
	{ .compatible = "sparkling,eh400wv" },
	{ }
};
MODULE_DEVICE_TABLE(of, eh400wv_of_match);

static struct mipi_dsi_driver eh400wv_driver = {
	.probe = eh400wv_probe,
	.remove = eh400wv_remove,
	.shutdown = eh400wv_shutdown,
	.driver = {
		.name = "panel-sparkling-eh400wv",
		.of_match_table = eh400wv_of_match,
	},
};
module_mipi_dsi_driver(eh400wv_driver);

MODULE_AUTHOR("Chanho Park <chanho61.park@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based eh400wv TFT LCD Panel Driver");
MODULE_LICENSE("GPL v2");
