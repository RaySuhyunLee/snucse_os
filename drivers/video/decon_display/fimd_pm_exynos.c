/* linux/drivers/video/decon_display/fimd_pm_exynos.c
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

#include <linux/platform_device.h>
#include "regs-fimd.h"
#include "decon_display_driver.h"
#include "fimd_fb.h"
#include "decon_mipi_dsi.h"
#include "decon_dt.h"
#include <mach/map.h>
#include <plat/cpu.h>

#include <../drivers/clk/samsung/clk.h>

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
#define FIMD_VIDEO_PSR

static struct clk *g_mout_fimd1;
static struct clk *g_mout_mpll_ctrl;
static struct clk *g_dout_fimd1;
static struct clk *g_mout_aclk_300_disp1_user;
static struct clk *g_mout_aclk_300_disp1_sw;
static struct clk *g_dout_aclk_300_disp1;
static struct clk *g_mout_aclk_300_disp1;
/*static struct clk *g_mout_aclk_200_disp1_user;
static struct clk *g_mout_aclk_200_disp1_sw;
static struct clk *g_dout_aclk_200_disp1;
static struct clk *g_mout_aclk_200_disp1;*/
static struct clk *g_mout_dpll_ctrl;
static struct clk *g_mout_aclk_400_disp1_user;
static struct clk *g_mout_aclk_400_disp1_sw;
static struct clk *g_dout_aclk_400_disp1;
static struct clk *g_mout_aclk_400_disp1;

#else

static struct clk *g_mout_fimd0;
static struct clk *g_dout_mpll_pre;
static struct clk *g_dout_fimd0;
static struct clk *g_mout_aclk_160;
static struct clk *g_dout_aclk_160;
static struct clk *g_mout_mipi0;
static struct clk *g_dout_mipi0;
static struct clk *g_dout_mipi0_pre;

#endif /* CONFIG_SOC_EXYNOS5422_REV_0 */

#define DISPLAY_CLOCK_SET_PARENT(child, parent) do {\
	g_##child = clk_get(dev, #child); \
	g_##parent = clk_get(dev, #parent); \
	if (IS_ERR(g_##child)) { \
		pr_err("Failed to clk_get " #child "\n"); \
		return PTR_ERR(g_##child); \
	} \
	if (IS_ERR(g_##parent)) { \
		pr_err("Failed to clk_get - " #parent "\n"); \
		return PTR_ERR(g_##parent); \
	} \
	ret = clk_set_parent(g_##child, g_##parent); \
	if (ret < 0) { \
		pr_err("failed to set parent " #parent " of " #child "\n"); \
	} \
	} while (0)

#define DISPLAY_CLOCK_INLINE_SET_PARENT(child, parent) do {\
	ret = clk_set_parent(g_##child, g_##parent); \
	if (ret < 0) { \
		pr_err("failed to set parent " #parent " of " #child "\n"); \
	} \
	} while (0)

#define DISPLAY_CHECK_REGS(addr, mask, val) do {\
	regs = ioremap(addr, 0x4); \
	data = readl(regs); \
	pr_err("[ERROR] CHECK_SFR 0x%08X\n", data); \
	iounmap(regs); \
	} while (0)

#define TEMPORARY_RECOVER_CMU(addr, mask, value) do {\
	regs = ioremap(addr, 0x4); \
	data = readl(regs); \
	data &= (mask); \
	data |= value; \
	writel(data, regs); \
	iounmap(regs); \
	} while (0)

#define DISPLAY_INLINE_SET_RATE(node, target) \
	clk_set_rate(g_##node, target);

#define DISPLAY_SET_RATE(node, target) do {\
	g_##node = clk_get(dev, #node); \
	if (IS_ERR(g_##node)) { \
		pr_err("Failed to clk_get - " #node "\n"); \
		return PTR_ERR(g_##node); \
	} \
	clk_set_rate(g_##node, target); \
	} while (0)

int init_display_decon_clocks(struct device *dev)
{
	int ret = 0;

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	DISPLAY_CLOCK_SET_PARENT(mout_fimd1, mout_mpll_ctrl);

#if defined(CONFIG_DECON_LCD_S6E8AA0)
	DISPLAY_SET_RATE(dout_fimd1, 67 * MHZ);
#elif defined(CONFIG_DECON_LCD_S6E3FA0)
	DISPLAY_SET_RATE(dout_fimd1, 133 * MHZ);
#else
	DISPLAY_SET_RATE(dout_fimd1, 266 * MHZ);
#endif

	DISPLAY_CLOCK_SET_PARENT(mout_aclk_300_disp1_user, mout_aclk_300_disp1_sw);
	DISPLAY_CLOCK_SET_PARENT(mout_aclk_300_disp1_sw, dout_aclk_300_disp1);
	DISPLAY_CLOCK_SET_PARENT(mout_aclk_300_disp1, mout_dpll_ctrl);
	DISPLAY_SET_RATE(dout_aclk_300_disp1, 300 * MHZ);
	DISPLAY_CLOCK_SET_PARENT(mout_aclk_400_disp1_user, mout_aclk_400_disp1_sw);
	DISPLAY_CLOCK_SET_PARENT(mout_aclk_400_disp1_sw, dout_aclk_400_disp1);
	DISPLAY_CLOCK_SET_PARENT(mout_aclk_400_disp1, mout_dpll_ctrl);

	pr_info("%s: pixel_clk_rate: %d\n", __func__, exynos_get_rate("sclk_fimd1"));
	pr_info("%s: aclk200_clk_rate: %d\n", __func__, exynos_get_rate("mout_aclk_200_disp1_user"));
	pr_info("%s: aclk300_clk_rate: %d\n", __func__, exynos_get_rate("mout_aclk_300_disp1_user"));
	pr_info("%s: aclk400_clk_rate: %d\n", __func__, exynos_get_rate("mout_aclk_400_disp1_user"));
#else /* CONFIG_SOC_EXYNOS5422_REV_0 */
	/* Set parent clock and rate */
	/* 1. Set [LCD0_BLK:sclk_fimd0]: display special pixel clock */
	DISPLAY_CLOCK_SET_PARENT(dout_fimd0, mout_fimd0);
	DISPLAY_CLOCK_SET_PARENT(mout_fimd0, dout_mpll_pre);
	DISPLAY_SET_RATE(dout_fimd0, 25 * MHZ);

	/* 2. Set [CMU_TOP:aclk_160] : display top clock(dedicated for LCD_BLK) */
	DISPLAY_CLOCK_SET_PARENT(dout_aclk_160, mout_aclk_160);
	DISPLAY_CLOCK_SET_PARENT(mout_aclk_160, dout_mpll_pre);
	DISPLAY_SET_RATE(dout_aclk_160, 100 * MHZ);
#endif /* CONFIG_SOC_EXYNOS5422_REV_0 */
	return ret;
}

int init_display_driver_clocks(struct device *dev)
{
	int ret = 0;
#ifndef CONFIG_SOC_EXYNOS5422_REV_0
	/* Set parent clock and rate */
	/* 1. Set [LCD0_BLK:sclk_mipi0]: display special mipi-dsim */
	DISPLAY_CLOCK_SET_PARENT(dout_mipi0_pre, dout_mipi0);
	DISPLAY_CLOCK_SET_PARENT(dout_mipi0, mout_mipi0);
	DISPLAY_CLOCK_SET_PARENT(mout_mipi0, dout_mpll_pre);
	DISPLAY_SET_RATE(dout_mipi0_pre, 100 * MHZ);
#endif /* CONFIG_SOC_EXYNOS5422_REV_0 */
	return ret;
}

void init_display_gpio_exynos(void)
{
	unsigned int reg = 0;

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
#if defined(CONFIG_S5P_DP)
	unsigned gpio_dp_hotplug = 0;

	gpio_dp_hotplug = get_display_dp_hotplug_gpio_exynos();
	/* Set Hotplug detect for DP */
	gpio_request(gpio_dp_hotplug, "dp_hotplug");
	/* TO DO */
	s3c_gpio_cfgpin(gpio_dp_hotplug, S3C_GPIO_SFN(3));
#endif

	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 *
	 * FIMD of DISP1_BLK Bypass selection : DISP1BLK_CFG[15]
	 * ---------------------
	 *  1 | FIMD : selected
	 */
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg &= ~(1 << 15);	/* To save other reset values */
	reg |= (1 << 15);
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
#if defined(CONFIG_S5P_DP)
	/* Reference clcok selection for DPTX_PHY: PAD_OSC_IN */
	reg = __raw_readl(S3C_VA_SYS + 0x04d4);
	reg &= ~(1 << 0);
	__raw_writel(reg, S3C_VA_SYS + 0x04d4);

	/* DPTX_PHY: XXTI */
	reg = __raw_readl(S3C_VA_SYS + 0x04d8);
	reg &= ~(1 << 3);
	__raw_writel(reg, S3C_VA_SYS + 0x04d8);
#endif
	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 *
	 * MIC of DISP1_BLK Bypass selection: DISP1BLK_CFG[11]
	 * --------------------
	 *  0 | MIC
	 *  1 | Bypass : selected
	 */
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg &= ~(1 << 11);
#ifndef CONFIG_DECON_MIC
	reg |= (1 << 11);
#endif
	__raw_writel(reg, S3C_VA_SYS + 0x0214);

#if  defined (CONFIG_FB_I80_COMMAND_MODE) && !defined (FIMD_VIDEO_PSR)
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg |= (1 << 24);
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
#endif

#if defined (CONFIG_FB_I80_COMMAND_MODE) 
	/* related to convertor between FIMD & MIPI */
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg |= (1 << 12);
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
#else
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg &= ~(1 << 12);
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
#endif

#else /* CONFIG_SOC_EXYNOS5422_REV_0 */
	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 * ---------------------
	 *  0 | MIE/MDNIE
	 *  1 | FIMD : selected
	 */
	reg = __raw_readl(S3C_VA_SYS + 0x0210);
	reg &= ~(1 << 1);
#if !defined(CONFIG_FB_SMIES)
	reg |= (1 << 1);
#endif
	__raw_writel(reg, S3C_VA_SYS + 0x0210);
	reg = __raw_readl(S3C_VA_SYS + 0x0210);
	reg &= ~(3 << 10);
	reg |= (1 << 10);
	__raw_writel(reg, S3C_VA_SYS + 0x0210);
#endif /* CONFIG_SOC_EXYNOS5422_REV_0 */
}


int enable_display_driver_power(struct device *dev)
{
	struct display_gpio *gpio;
	struct display_driver *dispdrv;
	int ret = 0;

	dispdrv = get_display_driver();

	gpio = dispdrv->dt_ops.get_display_dsi_reset_gpio();

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	gpio_request_one(gpio->id[0], GPIOF_OUT_INIT_HIGH, "lcd_power");
	usleep_range(5000, 6000);
	gpio_free(gpio->id[0]);

	gpio_request_one(gpio->id[1], GPIOF_OUT_INIT_HIGH, "lcd_reset");
	usleep_range(5000, 6000);
	gpio_set_value(gpio->id[1], 0);
	usleep_range(5000, 6000);
	gpio_set_value(gpio->id[1], 1);
	usleep_range(5000, 6000);
	gpio_free(gpio->id[1]);
#else /* CONFIG_SOC_EXYNOS5422_REV_0 */
	gpio_request_one(gpio->id[0], GPIOF_OUT_INIT_HIGH, "lcd_reset");
	usleep_range(20000, 21000);
	gpio_free(gpio->id[0]);
#endif /* CONFIG_SOC_EXYNOS5422_REV_0 */
	
	return ret;
}

int disable_display_driver_power(struct device *dev)
{
	struct display_gpio *gpio;
	struct display_driver *dispdrv;
	int ret = 0;

	dispdrv = get_display_driver();

	gpio = dispdrv->dt_ops.get_display_dsi_reset_gpio();
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	gpio_request_one(gpio->id[1], GPIOF_OUT_INIT_LOW, "lcd_reset");
	usleep_range(5000, 6000);
	gpio_free(gpio->id[1]);
	/* Power */
	gpio_request_one(gpio->id[0], GPIOF_OUT_INIT_LOW, "lcd_power");
	usleep_range(5000, 6000);
#else /* CONFIG_SOC_EXYNOS5422_REV_0 */
	/* Reset */
	gpio_request_one(gpio->id[0], GPIOF_OUT_INIT_LOW, "lcd_reset");
	usleep_range(20000, 21000);
#endif /* CONFIG_SOC_EXYNOS5422_REV_0 */
	gpio_free(gpio->id[0]);

	return ret;
}

#ifndef CONFIG_BACKLIGHT_PWM
void backlight_en(int en)
{
	struct display_gpio *gpio;
	struct display_driver *dispdrv;

	dispdrv = get_display_driver();

	gpio = dispdrv->dt_ops.get_display_dsi_reset_gpio();
	gpio_request_one(gpio->id[1], GPIOF_OUT_INIT_LOW, "bl_en");
	msleep(50);
	if(en)
	{
		gpio_set_value(gpio->id[1], 1);
		usleep_range(20000, 21000);
	}
	gpio_free(gpio->id[1]);
}
#endif

void fimd_clock_on(struct display_driver *dispdrv)
{
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	clk_prepare_enable(sfb->bus_clk);

	/*TODO: Check FIMD H/W version */
	clk_prepare_enable(sfb->axi_disp1);

	if (!sfb->variant.has_clksel)
		clk_prepare_enable(sfb->lcd_clk);
}

void fimd_clock_off(struct display_driver *dispdrv)
{
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	if (!sfb->variant.has_clksel)
		clk_disable_unprepare(sfb->lcd_clk);

	/*TODO: Check FIMD H/W version */
	clk_disable_unprepare(sfb->axi_disp1);

	clk_disable_unprepare(sfb->bus_clk);
}

void dsi_clock_on(struct display_driver *dispdrv)
{
	if (!dispdrv->dsi_driver.clk) {
		dispdrv->dsi_driver.clk = __clk_lookup("gate_clk_mipi0");
		if (IS_ERR(dispdrv->dsi_driver.clk)) {
			pr_err("Failed to clk_get - clk_dsim1\n");
			return;
		}
	}
	clk_prepare_enable(dispdrv->dsi_driver.clk);
}

void dsi_clock_off(struct display_driver *dispdrv)
{
	clk_disable_unprepare(dispdrv->dsi_driver.clk);
}

void mic_clock_on(struct display_driver *dispdrv) { }
void mic_clock_off(struct display_driver *dispdrv) { }

int enable_display_decon_clocks(struct device *dev)
{
	int ret = 0;
	struct display_driver *dispdrv;
	struct s3c_fb *sfb;

	dispdrv = get_display_driver();
	sfb = dispdrv->decon_driver.sfb;

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
DISPLAY_CLOCK_INLINE_SET_PARENT(mout_fimd1, mout_mpll_ctrl);

#if defined(CONFIG_DECON_LCD_S6E8AA0)
	DISPLAY_INLINE_SET_RATE(dout_fimd1, 67 * MHZ);
#elif defined(CONFIG_DECON_LCD_S6E3FA0)
	DISPLAY_INLINE_SET_RATE(dout_fimd1, 133 * MHZ);
#else
	DISPLAY_INLINE_SET_RATE(dout_fimd1, 266 * MHZ);
#endif

	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_aclk_300_disp1_user, mout_aclk_300_disp1_sw);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_aclk_300_disp1_sw, dout_aclk_300_disp1);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_aclk_300_disp1, mout_dpll_ctrl);
	DISPLAY_INLINE_SET_RATE(dout_aclk_300_disp1, 300 * MHZ);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_aclk_400_disp1_user, mout_aclk_400_disp1_sw);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_aclk_400_disp1_sw, dout_aclk_400_disp1);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_aclk_400_disp1, mout_dpll_ctrl);

#else /* CONFIG_SOC_EXYNOS5422_REV_0 */

	/* Set INLINE parent clock and rate */
	/* 1. Set [LCD0_BLK:sclk_fimd0]: display special pixel clock */
	DISPLAY_CLOCK_INLINE_SET_PARENT(dout_fimd0, mout_fimd0);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_fimd0, dout_mpll_pre);
	DISPLAY_INLINE_SET_RATE(dout_fimd0, 25 * MHZ);

	/* 2. Set [CMU_TOP:aclk_160] : display top clock(dedicated for LCD_BLK) */
	DISPLAY_CLOCK_INLINE_SET_PARENT(dout_aclk_160, mout_aclk_160);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_aclk_160, dout_mpll_pre);
	DISPLAY_INLINE_SET_RATE(dout_aclk_160, 100 * MHZ);

#endif /* CONFIG_SOC_EXYNOS5422_REV_0 */

#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	dispdrv->pm_status.ops->clk_on(dispdrv);
#else
	if (!dispdrv->dsi_driver.clk) {
		dispdrv->dsi_driver.clk = __clk_lookup("clk_dsim1");
		if (IS_ERR(dispdrv->dsi_driver.clk)) {
			pr_err("Failed to clk_get - clk_dsim1\n");
			return ret;
		}
	}

	dsi_clock_on(dispdrv);
	fimd_clock_on(dispdrv);
#endif
	return ret;
}

int enable_display_driver_clocks(struct device *dev)
{
	return 0;
}

int disable_display_decon_clocks(struct device *dev)
{
	struct display_driver *dispdrv;
	struct s3c_fb *sfb;

	dispdrv = get_display_driver();
	sfb = dispdrv->decon_driver.sfb;

#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	dispdrv->pm_status.ops->clk_off(dispdrv);
#else
	fimd_clock_off(dispdrv);
	dsi_clock_off(dispdrv);
#endif

	return 0;
}

int enable_display_decon_runtimepm(struct device *dev)
{
	return 0;
}

int disable_display_decon_runtimepm(struct device *dev)
{
	return 0;
}

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
bool check_camera_is_running(void)
{
	/* CAM1 STATUS */
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	if (readl(S5P_VA_PMU + 0x5104) & 0x1)
#else /* CONFIG_SOC_EXYNOS5422_REV_0 */
	if (readl(S5P_VA_PMU + 0x3C04) & 0x1)
#endif /* CONFIG_SOC_EXYNOS5422_REV_0 */
		return true;
	else
		return false;
}

bool get_display_power_status(void)
{
	/* DISP_STATUS */
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	if (readl(S5P_VA_PMU + 0x40C4) & 0x1)
#else /* CONFIG_SOC_EXYNOS5422_REV_0 */
	if (readl(S5P_VA_PMU + 0x3C84) & 0x1)
#endif /* CONFIG_SOC_EXYNOS5422_REV_0 */
		return true;
	else
		return false;
}

int get_display_line_count(struct display_driver *dispdrv)
{
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;
	return (readl(sfb->regs + VIDCON1) >> VIDCON1_LINECNT_SHIFT);
#endif /* CONFIG_SOC_EXYNOS5422_REV_0 */
	return 0;
}

void set_default_hibernation_mode(struct display_driver *dispdrv)
{
	bool clock_gating = false;
	bool power_gating = false;
	bool hotplug_gating = false;

#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	clock_gating = true;
#endif
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_POWER_GATING
	power_gating = true;
#endif
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_POWER_GATING_DEEPSTOP
	hotplug_gating = true;
#endif
	dispdrv->pm_status.clock_gating_on = clock_gating;
	dispdrv->pm_status.power_gating_on = power_gating;
	dispdrv->pm_status.hotplug_gating_on = hotplug_gating;
}

struct pm_ops decon_pm_ops = {
	.clk_on		= fimd_clock_on,
	.clk_off 	= fimd_clock_off,
};
#ifdef CONFIG_DECON_MIC
struct pm_ops mic_pm_ops = {
	.clk_on		= mic_clock_on,
	.clk_off 	= mic_clock_off,
};
#endif
struct pm_ops dsi_pm_ops = {
	.clk_on		= dsi_clock_on,
	.clk_off 	= dsi_clock_off,
};

#else
int disp_pm_runtime_get_sync(struct display_driver *dispdrv)
{
	pm_runtime_get_sync(dispdrv->display_driver);
	return 0;
}

int disp_pm_runtime_put_sync(struct display_driver *dispdrv)
{
	pm_runtime_put_sync(dispdrv->display_driver);
	return 0;
}
#endif
