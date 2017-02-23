/* linux/arch/arm/mach-exynos/cpuidle-exynos3250.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/suspend.h>
#include <linux/smp.h>

#include <asm/suspend.h>

#include <mach/regs-pmu.h>
#include <mach/regs-clock.h>
#include <mach/exynos-pm.h>
#include <mach/pmu.h>
#include <mach/smc.h>

#include <plat/pm.h>
#include <plat/cpu.h>

#ifdef CONFIG_ARM_TRUSTZONE
#define REG_DIRECTGO_ADDR	(S5P_VA_SYSRAM_NS + 0x24)
#define REG_DIRECTGO_FLAG	(S5P_VA_SYSRAM_NS + 0x20)
#else
#define REG_DIRECTGO_ADDR	EXYNOS_INFORM0
#define REG_DIRECTGO_FLAG	EXYNOS_INFORM1
#endif

#define EXYNOS_CHECK_DIRECTGO	0xFCBA0D10

#if defined(CONFIG_MMC_DW)
extern int dw_mci_exynos_request_status(void);
#endif

#ifdef CONFIG_SAMSUNG_USBPHY
extern int samsung_usbphy_check_op(void);
extern void samsung_usb_lpa_resume(void);
#endif

static int exynos_enter_idle(struct cpuidle_device *dev,
			struct cpuidle_driver *drv,
			      int index);
static int exynos_enter_lowpower(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index);

static struct cpuidle_state exynos_cpuidle_set[] __initdata = {
	[0] = {
		.enter			= exynos_enter_idle,
		.exit_latency		= 1,
		.target_residency	= 1000,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C1",
		.desc			= "ARM clock gating(WFI)",
	},
	[1] = {
		.enter                  = exynos_enter_lowpower,
		.exit_latency           = 300,
		.target_residency       = 5000,
		.flags                  = CPUIDLE_FLAG_TIME_VALID,
		.name                   = "C3",
		.desc                   = "ARM power down",
	},
};

static DEFINE_PER_CPU(struct cpuidle_device, exynos_cpuidle_device);

static struct cpuidle_driver exynos_idle_driver = {
	.name		= "exynos_idle",
	.owner		= THIS_MODULE,
};

struct check_reg_lpa {
	void __iomem	*check_reg;
	unsigned int	check_bit;
};

/*
 * List of check power domain list for LPA mode
 * These register are have to power off to enter LPA mode
 */
static struct check_reg_lpa exynos_power_domain[] = {
	{.check_reg = EXYNOS3_LCD0_STATUS,	.check_bit = 0x7},
	{.check_reg = EXYNOS3_CAM_STATUS,       .check_bit = 0x7},
	{.check_reg = EXYNOS3_G3D_STATUS,	.check_bit = 0x7},
	{.check_reg = EXYNOS3_MFC_STATUS,	.check_bit = 0x7},
	/* need to add ISP check */
};

/*
 * List of check clock gating list for LPA mode
 * If clock of list is not gated, system can not enter LPA mode.
 */
static struct check_reg_lpa exynos_clock_gating[] = {
	{.check_reg = EXYNOS3_CLKGATE_IP_MFC,	.check_bit = 0x00000001},
/* TOP can be powered on during lpa mode.*/
#if 0
	{.check_reg = EXYNOS3_CLKGATE_IP_FSYS,	.check_bit = 0x00000001},
	{.check_reg = EXYNOS3_CLKGATE_IP_PERIL,	.check_bit = 0x00033FC0},
#endif
};

static struct check_reg_lpa audio_clock_gating[] = {
	{.check_reg = EXYNOS3_CLKGATE_IP_PERIL,   .check_bit = 0x00200000},
};
#if defined(CONFIG_MTK_COMBO_MT66XX)
int exynos_bt_op_f = 0;

EXPORT_SYMBOL(exynos_bt_op_f);

static int exynos_check_bt_operation(void)
{
	if (exynos_bt_op_f == 0) {
		return 0;
	}
	return 1;
}
#endif

static int exynos_check_reg_status(struct check_reg_lpa *reg_list,
				    unsigned int list_cnt)
{
	unsigned int i;
	unsigned int tmp;

	for (i = 0; i < list_cnt; i++) {
		tmp = __raw_readl(reg_list[i].check_reg);
		if (tmp & reg_list[i].check_bit)
			return -EBUSY;
	}

	return 0;
}

static int __maybe_unused exynos_check_enter_mode(void)
{
	/* Check power domain */
	if (exynos_check_reg_status(exynos_power_domain,
			    ARRAY_SIZE(exynos_power_domain)))
		return EXYNOS_CHECK_DIDLE;

	/* Check clock gating */
	if (exynos_check_reg_status(exynos_clock_gating,
			    ARRAY_SIZE(exynos_clock_gating)))
		return EXYNOS_CHECK_DIDLE;

#if defined(CONFIG_MMC_DW)
	if (dw_mci_exynos_request_status())
		return EXYNOS_CHECK_DIDLE;
#endif

#ifdef CONFIG_SAMSUNG_USBPHY
	if (samsung_usbphy_check_op())
		return EXYNOS_CHECK_DIDLE;
#endif

#if defined(CONFIG_MTK_COMBO_MT66XX)
     if (exynos_check_bt_operation())
		return EXYNOS_CHECK_DIDLE;
#endif

	return EXYNOS_CHECK_LPA;
}

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
void exynos3250_enable_idle_clock_down(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS3_PWR_CTRL);
	tmp &= ~((0x7 << 28) | (0x7 << 16) | (1 << 9) | (1 << 8));
	tmp |= (0x7 << 28) | (0x7 << 16) | 0x3ff;
	__raw_writel(tmp, EXYNOS3_PWR_CTRL);

	tmp = __raw_readl(EXYNOS3_PWR_CTRL2);
	tmp &= ~((0x3 << 24) | (0xffff << 8) | (0x77));
	tmp |= (100 << 16) | (10 << 8) | (1 << 4) | (0 << 0);
	tmp |= (1 << 25) | (1 << 24);
	__raw_writel(tmp, EXYNOS3_PWR_CTRL2);
}

void exynos3250_disable_idle_clock_down(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS3_PWR_CTRL);
	tmp &= ~(0x3 << 8);
	__raw_writel(tmp, EXYNOS3_PWR_CTRL);

	tmp = __raw_readl(EXYNOS3_PWR_CTRL2);
	tmp &= ~(0x3 << 24);
	__raw_writel(tmp, EXYNOS3_PWR_CTRL2);
}
#else
void exynos3250_enable_idle_clock_down(void) { }
void exynos3250_disable_idle_clock_down(void) { }
#endif

/* Ext-GIC nIRQ/nFIQ is the only wakeup source in AFTR */
static void exynos_set_wakeupmask(void)
{
	unsigned int origin = __raw_readl(EXYNOS_WAKEUP_MASK);

	origin = (origin & ~((0x1<<14)|(0x3<<9)|(0x1<<5)|(0x3<<1))) | (0x1 << 30);
	__raw_writel(origin, EXYNOS_WAKEUP_MASK);
	__raw_writel(0x0, EXYNOS_WAKEUP_MASK2);
}

#if !defined(CONFIG_ARM_TRUSTZONE)
static unsigned int g_pwr_ctrl, g_diag_reg;

static void save_cpu_arch_register(void)
{
	/*read power control register*/
	asm("mrc p15, 0, %0, c15, c0, 0" : "=r"(g_pwr_ctrl) : : "cc");
	/*read diagnostic register*/
	asm("mrc p15, 0, %0, c15, c0, 1" : "=r"(g_diag_reg) : : "cc");
	return;
}

static void restore_cpu_arch_register(void)
{
	/*write power control register*/
	asm("mcr p15, 0, %0, c15, c0, 0" : : "r"(g_pwr_ctrl) : "cc");
	/*write diagnostic register*/
	asm("mcr p15, 0, %0, c15, c0, 1" : : "r"(g_diag_reg) : "cc");
	return;
}
#else
static void save_cpu_arch_register(void) { }
static void restore_cpu_arch_register(void) { }
#endif

static int idle_finisher(unsigned long flags)
{
#if defined(CONFIG_ARM_TRUSTZONE)
	exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);
	exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CLUSTER, SMC_POWERSTATE_IDLE, 0);
#else
	cpu_do_idle();
#endif
	return 1;
}

static int exynos_enter_core0_aftr(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	unsigned long tmp;
	unsigned int ret = 0;
	unsigned int cpuid = smp_processor_id();

	exynos_set_wakeupmask();

	exynos3250_disable_idle_clock_down();

	/* Set value of power down register for aftr mode */
	exynos_sys_powerdown_conf(SYS_AFTR);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	save_cpu_arch_register();

	/* Setting Central Sequence Register for power down mode */
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);

	set_boot_flag(cpuid, C2_STATE);

	cpu_pm_enter();

	ret = cpu_suspend(0, idle_finisher);
	if (ret) {
		tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	}

	clear_boot_flag(cpuid, C2_STATE);

	cpu_pm_exit();

	restore_cpu_arch_register();

	exynos3250_enable_idle_clock_down();

	/* Clear wakeup state register */
	__raw_writel(0x0, EXYNOS_WAKEUP_STAT);

	return index;
}

static int exynos_enter_core0_lpa(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	unsigned long tmp;
	unsigned int ret;
	unsigned int cpuid = smp_processor_id();

	exynos_set_wakeupmask();

	exynos3250_disable_idle_clock_down();

	if (exynos_check_reg_status(audio_clock_gating,
			ARRAY_SIZE(audio_clock_gating)))
		exynos_sys_powerdown_conf(SYS_WAFTR_AUDIO);
	else
		exynos_sys_powerdown_conf(SYS_LPA);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	save_cpu_arch_register();

	/* Setting Central Sequence Register for power down mode */
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);

	set_boot_flag(cpuid, C2_STATE);

	cpu_pm_enter();
	exynos_lpa_enter();

	ret = cpu_suspend(0, idle_finisher);
	if (ret) {
		tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	}

#ifdef CONFIG_SAMSUNG_USBPHY
	samsung_usb_lpa_resume();
#endif
	clear_boot_flag(cpuid, C2_STATE);

	exynos_lpa_exit();
	cpu_pm_exit();

	restore_cpu_arch_register();

	exynos3250_enable_idle_clock_down();

	/* Clear wakeup state register */
	__raw_writel(0x0, EXYNOS_WAKEUP_STAT);

	return index;
}

static int exynos_enter_idle(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	cpu_do_idle();
	return index;
}

static int exynos_enter_lowpower(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	int new_index = index;

	/* This mode only can be entered when other core's are offline */
	if (num_online_cpus() > 1)
		return exynos_enter_idle(dev, drv, 0);

	if (exynos_check_enter_mode() == EXYNOS_CHECK_DIDLE)
		return exynos_enter_core0_aftr(dev, drv, new_index);
	else
		return exynos_enter_core0_lpa(dev, drv, new_index);
}

static int __init exynos_init_cpuidle(void)
{
	int i, max_cpuidle_state, cpu_id;
	struct cpuidle_device *device;
	struct cpuidle_driver *drv = &exynos_idle_driver;
	struct cpuidle_state *idle_set;

	exynos3250_enable_idle_clock_down();

	/* Setup cpuidle driver */
	idle_set = exynos_cpuidle_set;
	drv->state_count = ARRAY_SIZE(exynos_cpuidle_set);

	max_cpuidle_state = drv->state_count;
	for (i = 0; i < max_cpuidle_state; i++) {
		memcpy(&drv->states[i], &idle_set[i],
				sizeof(struct cpuidle_state));
	}
	drv->safe_state_index = 0;
	cpuidle_register_driver(&exynos_idle_driver);

	for_each_cpu(cpu_id, cpu_online_mask) {
		device = &per_cpu(exynos_cpuidle_device, cpu_id);
		device->cpu = cpu_id;

	device->state_count = max_cpuidle_state;

	if (cpuidle_register_device(device)) {
		printk(KERN_ERR "CPUidle register device failed\n,");
		return -EIO;
		}
	}

	return 0;
}
device_initcall(exynos_init_cpuidle);
