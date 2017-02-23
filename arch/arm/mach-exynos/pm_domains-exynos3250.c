/*
 * Exynos3250 Generic power domain support.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <mach/pm_domains.h>

struct exynos3_pd_reg_state {
	void __iomem *reg;
	unsigned long val;
	unsigned long set_val;
};

static void exynos3_pd_save_reg(struct exynos3_pd_reg_state *ptr, int count)
{
	for (; count > 0; count--, ptr++) {
		DEBUG_PRINT_INFO("%s : read %p (store %08x)\n", __func__,
			ptr->reg, __raw_readl(ptr->reg));
		ptr->val = __raw_readl(ptr->reg);
	}
}

static void exynos3_pd_restore_reg(struct exynos3_pd_reg_state *ptr, int count)
{
	for (; count > 0; count--, ptr++) {
		DEBUG_PRINT_INFO("%s : restore %p (restore %08lx, was %08x)\n",
			__func__, ptr->reg, ptr->val, __raw_readl(ptr->reg));
		__raw_writel(ptr->val, ptr->reg);
	}
}

static void exynos3_disable_bits(struct exynos3_pd_reg_state *ptr, int count)
{
	for (; count > 0; count--, ptr++) {
		DEBUG_PRINT_INFO("%s : set %p (change %08lx, was %08x)\n",
			__func__, ptr->reg, __raw_readl(ptr->reg) &
			~(ptr->set_val), __raw_readl(ptr->reg));
		__raw_writel(__raw_readl(ptr->reg) & ~(ptr->set_val), ptr->reg);
	}
}

static void exynos3_enable_bits(struct exynos3_pd_reg_state *ptr, int count)
{
	for (; count > 0; count--, ptr++) {
		DEBUG_PRINT_INFO("%s : set %p (change %08lx, was %08x)\n",
			__func__, ptr->reg, __raw_readl(ptr->reg) |
			ptr->set_val, __raw_readl(ptr->reg));
		__raw_writel(__raw_readl(ptr->reg) | ptr->set_val, ptr->reg);
	}
}

static void exynos3_pwr_reg_set(struct exynos3_pd_reg_state *ptr, int count)
{
	for (; count > 0; count--, ptr++) {
		DEBUG_PRINT_INFO("%s : set %p (change %08lx, was %08x)\n",
			__func__, ptr->reg, ptr->set_val,__raw_readl(ptr->reg));
		__raw_writel(ptr->set_val, ptr->reg);
	}
}

static struct exynos3_pd_reg_state exynos3_g3d_clk_reg[] = {
	{ .reg = EXYNOS3_CLKGATE_BUS_G3D,
		.set_val = ((0x1 << 18)|(0x1 << 16)|(0x7 << 3))},
	{ .reg = EXYNOS3_CLKGATE_IP_G3D,	.set_val = 0xf},
};

static struct exynos3_pd_reg_state exynos3_g3d_pwr_reg[] = {
	{ .reg = EXYNOS3_CMU_CLKSTOP_G3D_SYS_PWR_REG,	.set_val = 0},
	{ .reg = EXYNOS3_CMU_RESET_G3D_SYS_PWR_REG,	.set_val = 0},
};

static int exynos3_pd_g3d_on_pre(struct exynos_pm_domain *domain)
{
	DEBUG_PRINT_INFO("%s pre power on/off\n", "G3D");

	exynos3_pd_save_reg(exynos3_g3d_clk_reg,
				ARRAY_SIZE(exynos3_g3d_clk_reg));
	exynos3_enable_bits(exynos3_g3d_clk_reg,
				ARRAY_SIZE(exynos3_g3d_clk_reg));
	exynos3_pwr_reg_set(exynos3_g3d_pwr_reg,
				ARRAY_SIZE(exynos3_g3d_pwr_reg));

	return 0;
}

static int exynos3_pd_g3d_on_post(struct exynos_pm_domain *domain)
{
	DEBUG_PRINT_INFO("%s post power on/off\n", "G3D");

	exynos3_pd_restore_reg(exynos3_g3d_clk_reg,
				ARRAY_SIZE(exynos3_g3d_clk_reg));

	return 0;
}

static struct exynos3_pd_reg_state exynos3_mfc_clk_reg[] = {
	{ .reg = EXYNOS3_CLKGATE_BUS_MFC,
	.set_val = ((0x1 << 19)|(0x7 << 15)|(0x1 << 6)|(0x1 << 4)|(0x3 << 1))},
	{ .reg = EXYNOS3_CLKGATE_IP_MFC,
	.set_val = ((0x1 << 5)|(0x1 << 3)|(0x3))},
};

static struct exynos3_pd_reg_state exynos3_mfc_pwr_reg[] = {
	{ .reg = EXYNOS3_CMU_CLKSTOP_MFC_SYS_PWR_REG,	.set_val = 0},
	{ .reg = EXYNOS3_CMU_RESET_MFC_SYS_PWR_REG,	.set_val = 0},
};

static int exynos3_pd_mfc_on_pre(struct exynos_pm_domain *domain)
{
	DEBUG_PRINT_INFO("%s pre power on/off\n", "MFC");

	exynos3_pd_save_reg(exynos3_mfc_clk_reg,
				ARRAY_SIZE(exynos3_mfc_clk_reg));
	exynos3_enable_bits(exynos3_mfc_clk_reg,
				ARRAY_SIZE(exynos3_mfc_clk_reg));
	exynos3_pwr_reg_set(exynos3_mfc_pwr_reg,
				ARRAY_SIZE(exynos3_mfc_pwr_reg));

	return 0;
}

static int exynos3_pd_mfc_on_post(struct exynos_pm_domain *domain)
{
	DEBUG_PRINT_INFO("%s post power on/off\n", "MFC");

	exynos3_pd_restore_reg(exynos3_mfc_clk_reg,
				ARRAY_SIZE(exynos3_mfc_clk_reg));

	return 0;
}

static struct exynos3_pd_reg_state exynos3_lcd0_clk_reg[] = {
	{ .reg = EXYNOS3_CLKSRC_MASK_LCD,
		.set_val = ((0x1 << 12)|(0x1))},
	{ .reg = EXYNOS3_CLKGATE_BUS_LCD,
		.set_val = ((0x7F << 14)|(0x3 << 9)|(0x1F << 3)|(0x1))},
	{ .reg = EXYNOS3_CLKGATE_SCLK_LCD,
		.set_val = ((0x3 << 3)|(0x3))},
	{ .reg = EXYNOS3_CLKGATE_IP_LCD,
		.set_val = ((0x3F << 2)|(0x1))},
};

static struct exynos3_pd_reg_state exynos3_lcd0_lpi_pwr_reg1[] = {
	{ .reg = EXYNOS3_LPI_MASK1,	                .set_val = 0x1 << 20},
};

static struct exynos3_pd_reg_state exynos3_lcd0_lpi_pwr_reg2[] = {
	{ .reg = EXYNOS3_LPI_DENIAL_MASK1,	        .set_val = 0x1 << 20},
};

static struct exynos3_pd_reg_state exynos3_lcd0_pwr_reg[] = {
	{ .reg = EXYNOS3_CMU_CLKSTOP_LCD0_SYS_PWR_REG,	.set_val = 0},
	{ .reg = EXYNOS3_CMU_RESET_LCD0_SYS_PWR_REG,	.set_val = 0},
};

static int exynos3_pd_lcd0_on_pre(struct exynos_pm_domain *domain)
{
	DEBUG_PRINT_INFO("%s pre power on/off\n", "LCD0");

	exynos3_pd_save_reg(exynos3_lcd0_clk_reg,
				ARRAY_SIZE(exynos3_lcd0_clk_reg));
	exynos3_pd_save_reg(exynos3_lcd0_lpi_pwr_reg1,
				ARRAY_SIZE(exynos3_lcd0_lpi_pwr_reg1));
	exynos3_pd_save_reg(exynos3_lcd0_lpi_pwr_reg2,
				ARRAY_SIZE(exynos3_lcd0_lpi_pwr_reg2));
	exynos3_enable_bits(exynos3_lcd0_lpi_pwr_reg1,
				ARRAY_SIZE(exynos3_lcd0_lpi_pwr_reg1));
	exynos3_disable_bits(exynos3_lcd0_lpi_pwr_reg2,
				ARRAY_SIZE(exynos3_lcd0_lpi_pwr_reg2));
	exynos3_enable_bits(exynos3_lcd0_clk_reg,
				ARRAY_SIZE(exynos3_lcd0_clk_reg));
	exynos3_pwr_reg_set(exynos3_lcd0_pwr_reg,
				ARRAY_SIZE(exynos3_lcd0_pwr_reg));

	return 0;
}

static int exynos3_pd_lcd0_on_post(struct exynos_pm_domain *domain)
{
	DEBUG_PRINT_INFO("%s post power on/off\n", "LCD0");

	exynos3_pd_restore_reg(exynos3_lcd0_clk_reg,
				ARRAY_SIZE(exynos3_lcd0_clk_reg));
	exynos3_pd_restore_reg(exynos3_lcd0_lpi_pwr_reg1,
				ARRAY_SIZE(exynos3_lcd0_lpi_pwr_reg1));
	exynos3_pd_restore_reg(exynos3_lcd0_lpi_pwr_reg2,
				ARRAY_SIZE(exynos3_lcd0_lpi_pwr_reg2));

	return 0;
}

static struct exynos3_pd_reg_state exynos3_cam_clk_reg[] = {
	{ .reg = EXYNOS3_CLKSRC_MASK_CAM,
		.set_val = ((0x1 << 20)|(0x1))},
	{ .reg = EXYNOS3_CLKGATE_BUS_CAM0,
		.set_val = ((0x3 << 29)|(0x1 << 27)|(0xF << 22)|(0x7 << 18)|
			(0x3 << 14)|(0x7 << 10)|(0xF << 5)|(0x7))},
	{ .reg = EXYNOS3_CLKGATE_BUS_CAM1,
		.set_val = ((0xF << 10)|(0x7))},
	{ .reg = EXYNOS3_CLKGATE_SCLK_CAM,
		.set_val = (0x1 << 8)},
	{ .reg = EXYNOS3_CLKGATE_IP_CAM,
		.set_val = ((0xF << 16)|(0xF << 11)|(0xF << 6)|(0x7))},
};

static struct exynos3_pd_reg_state exynos3_cam_pwr_reg[] = {
	{ .reg = EXYNOS3_CMU_CLKSTOP_CAM_SYS_PWR_REG,	.set_val = 0},
	{ .reg = EXYNOS3_CMU_RESET_CAM_SYS_PWR_REG,	.set_val = 0},
};

static int exynos3_pd_cam_on_pre(struct exynos_pm_domain *domain)
{
	DEBUG_PRINT_INFO("%s pre power on/off\n", "CAM");

	exynos3_pd_save_reg(exynos3_cam_clk_reg,
				ARRAY_SIZE(exynos3_cam_clk_reg));
	exynos3_enable_bits(exynos3_cam_clk_reg,
				ARRAY_SIZE(exynos3_cam_clk_reg));
	exynos3_pwr_reg_set(exynos3_cam_pwr_reg,
				ARRAY_SIZE(exynos3_cam_pwr_reg));

	return 0;
}

static int exynos3_pd_cam_on_post(struct exynos_pm_domain *domain)
{
	DEBUG_PRINT_INFO("%s post power on/off\n", "CAM");

	exynos3_pd_restore_reg(exynos3_cam_clk_reg,
				ARRAY_SIZE(exynos3_cam_clk_reg));

	return 0;
}

static struct exynos3_pd_reg_state exynos3_isp_clk_reg[] = {
	{.reg = EXYNOS3_CLKSRC_MASK_ISP,
		.set_val = ((0x1 << 12)|(0x1 << 8)|(0x1 << 4))},
	{.reg = EXYNOS3_CLKGATE_SCLK_TOP_ISP,
		.set_val = (0xF << 1)},
};

static struct exynos3_pd_reg_state exynos3_isp_sclks_reg[] = {
	{.reg = EXYNOS3_CLKGATE_BUS_ISP0,
		.set_val = ((0x7 << 27)|(0xF << 21)|(0xFFF << 8)|(0x7F))},
	{.reg = EXYNOS3_CLKGATE_BUS_ISP1,
		.set_val = ((0x7F << 15)|(0x1 << 4))},
	{.reg = EXYNOS3_CLKGATE_BUS_ISP2,
		.set_val = ((0x3 << 30)|(0x1 << 28)|(0xF << 23)|(0x3 << 20)|
			(0x1F << 14)|(0xFFF << 1))},
	{.reg = EXYNOS3_CLKGATE_BUS_ISP3,
		.set_val = ((0x7F << 15)|(0x3 << 12)|(0x3 << 3)|(0x1))},
	{.reg = EXYNOS3_CLKGATE_IP_ISP0,
		.set_val = ((0x3 << 30)|(0x1 << 28)|(0xF << 23)|(0x3 << 20)|
			(0x7FFFF))},
	{.reg = EXYNOS3_CLKGATE_IP_ISP1,
		.set_val = ((0x7F << 15)|(0x3 << 12)|(0x1 << 4)|(0x1))},
	{.reg = EXYNOS3_CLKGATE_SCLK_ISP,
		.set_val = (0x1)},
};

static struct exynos3_pd_reg_state exynos3_isp_pwr_reg[] = {
	{.reg = EXYNOS3_ISP_OPTION,			.set_val = 2},
	{.reg = EXYNOS3_CMU_CLKSTOP_ISP_SYS_PWR_REG,	.set_val = 0},
	{.reg = EXYNOS3_CMU_SYSCLK_ISP_SYS_PWR_REG,	.set_val = 0},
	{.reg = EXYNOS3_CMU_RESET_ISP_SYS_PWR_REG,	.set_val = 0},
	{.reg = EXYNOS3_ISP_ARM_SYS_PWR_REG,		.set_val = 0},
};

static void exynos3_pd_isp_force_poweroff(void)
{
#define SMMU_FIMC_ISP 0x12260000
#define SMMU_FIMC_DRC 0x12270000
#define SMMU_FIMC_SCC 0x12280000
#define SMMU_FIMC_FD  0x122A0000
#define SMMU_FIMC_CPU 0x122B0000
	unsigned long timeout;
	unsigned int i;
	void __iomem *reg;
	unsigned int isp_sysmmu_reg_force_off[] = {
		SMMU_FIMC_ISP,
		SMMU_FIMC_DRC,
		SMMU_FIMC_SCC,
		SMMU_FIMC_FD,
		SMMU_FIMC_CPU,
	};
	unsigned int nelem = ARRAY_SIZE(isp_sysmmu_reg_force_off);

	/* First enable the SysMMUs with blocking mode */
	for (i = 0; i < nelem; i++) {
		reg = ioremap(isp_sysmmu_reg_force_off[i], 0x20);
		if (reg == NULL)
			continue;
		__raw_writel(0x3, reg);
		timeout = 50;
		while(((__raw_readl(reg + 0x8) & 0x1) != 0x1) && timeout) {
			timeout--;
			usleep_range(80, 100);
		}
		iounmap(reg);
	}

	__raw_writel(0x4fc8, EXYNOS3_LPI_MASK0);
	__raw_writel(0, EXYNOS3_ISP_ARM_CONFIGURATION);

	timeout = 500;
	do {
		if (timeout == 0) {
			pr_err("PM DOMAIN : %s can't turn off forcefully!, timeout\n", "ISP_ARM");
			break;
		}
		--timeout;
		usleep_range(80, 100);
	} while (__raw_readl(EXYNOS3_ISP_ARM_STATUS) & 0x1);

	__raw_writel(0, EXYNOS3_ISP_CONFIGURATION);

	timeout = 750;
	do {
		if (timeout == 0) {
			pr_err("PM DOMAIN : %s can't turn off forcefully!, timeout\n", "ISP_DOMAIN");
			break;
		}
		--timeout;
		usleep_range(80, 100);
	} while (__raw_readl(EXYNOS3_ISP_STATUS) & EXYNOS_INT_LOCAL_PWR_EN);

}

static int exynos3_pd_isp_power(struct exynos_pm_domain *pd, int power_flags)
{
	unsigned long timeout;
	int ret = 0;

	if (unlikely(!pd))
		return -EINVAL;

	mutex_lock(&pd->access_lock);

	exynos3_pd_save_reg(exynos3_isp_clk_reg,
				ARRAY_SIZE(exynos3_isp_clk_reg));

	if (!power_flags)
		exynos3_enable_bits(exynos3_isp_sclks_reg,
				ARRAY_SIZE(exynos3_isp_sclks_reg));

	exynos3_enable_bits(exynos3_isp_clk_reg,
				ARRAY_SIZE(exynos3_isp_clk_reg));

	exynos3_pwr_reg_set(exynos3_isp_pwr_reg,
				ARRAY_SIZE(exynos3_isp_pwr_reg));

	/* on/off value to CONFIGURATION register */
	__raw_writel(power_flags, EXYNOS3_ISP_CONFIGURATION);

	/* Wait max 75ms */
	timeout = 750;
		/* check STATUS register */
		while ((__raw_readl(EXYNOS3_ISP_STATUS) &
				EXYNOS_INT_LOCAL_PWR_EN) != power_flags) {
			if (timeout == 0) {
				pr_err("%s@%p: %08x, %08x, %08x\n",
					pd->genpd.name,
					pd->base,
					__raw_readl(pd->base),
					__raw_readl(pd->base+4),
					__raw_readl(pd->base+8));
				pr_err(PM_DOMAIN_PREFIX "%s can't control power, timeout\n", pd->name);
				break;
			}
			--timeout;
			cpu_relax();
			usleep_range(80, 100);
		}

	if (timeout == 0 && power_flags == 0) {
		if ((__raw_readl(EXYNOS3_ISP_ARM_STATUS) & 0x1) != 0)
			exynos3_pd_isp_force_poweroff();
	}

	if ((__raw_readl(EXYNOS3_ISP_STATUS) & EXYNOS_INT_LOCAL_PWR_EN) != power_flags)
		ret = -ETIMEDOUT;
	else
		pd->status = power_flags;

	exynos3_pd_restore_reg(exynos3_isp_clk_reg,
				ARRAY_SIZE(exynos3_isp_clk_reg));

	mutex_unlock(&pd->access_lock);

	DEBUG_PRINT_INFO("%s@%p: %08x, %08x, %08x\n",
	pd->genpd.name, pd->base,
	__raw_readl(pd->base),
	__raw_readl(pd->base+4),
	__raw_readl(pd->base+8));

	return ret;
}

static struct exynos_pd_callback pd_callback_list[] = {
	{
			.name = "pd-g3d",
			.on_pre = exynos3_pd_g3d_on_pre,
			.on_post = exynos3_pd_g3d_on_post,
			.off_pre = exynos3_pd_g3d_on_pre,
			.off_post = exynos3_pd_g3d_on_post,
	} , {
			.name = "pd-mfc",
			.on_pre = exynos3_pd_mfc_on_pre,
			.on_post = exynos3_pd_mfc_on_post,
			.off_pre = exynos3_pd_mfc_on_pre,
			.off_post = exynos3_pd_mfc_on_post,
	} , {
			.name = "pd-lcd0",
			.on_pre = exynos3_pd_lcd0_on_pre,
			.on_post = exynos3_pd_lcd0_on_post,
			.off_pre = exynos3_pd_lcd0_on_pre,
			.off_post = exynos3_pd_lcd0_on_post,
	} , {
			.name = "pd-cam",
			.on_pre = exynos3_pd_cam_on_pre,
			.on_post = exynos3_pd_cam_on_post,
			.off_pre = exynos3_pd_cam_on_pre,
			.off_post = exynos3_pd_cam_on_post,
	} , {
			.name = "pd-isp",
			.on = exynos3_pd_isp_power,
			.off = exynos3_pd_isp_power,
	}
};

struct exynos_pd_callback * exynos_pd_find_callback(struct exynos_pm_domain *pd)
{
	struct exynos_pd_callback *cb = NULL;
	int i;

	/* find callback function for power domain */
	for (i=0; i < ARRAY_SIZE(pd_callback_list); i++) {
		if (strcmp(pd->name, pd_callback_list[i].name))
			continue;

		cb = &pd_callback_list[i];
		DEBUG_PRINT_INFO("%s: found callback function\n", pd->name);
		break;
	}

	pd->cb = cb;
	return cb;
}
