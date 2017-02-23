/*
 * SAMSUNG EXYNOS3 Flattened Device Tree enabled machine
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/memblock.h>
#include <linux/io.h>
#include <linux/clocksource.h>
#include <linux/exynos_ion.h>
#include <linux/reboot.h>
#include <linux/bq24160_charger.h>

#include <asm/mach/arch.h>
#include <mach/regs-pmu.h>

#include <plat/cpu.h>
#include <plat/regs-watchdog.h>

#include "common.h"

#if defined(CONFIG_SOC_EXYNOS3250)
extern void exynos3250_wdt_reset(void);

void espresso3250_power_off(void)
{
	unsigned int reg = 0;
	unsigned int chg_status = 0;
	printk("power off the device....\n");

#ifdef CONFIG_CHARGER_BQ24160
//	chg_status = bq24160_get_ext_charging_status();
#endif
	if( EXT_CHG_STAT_VALID_CHARGER_CONNECTED == chg_status){
		printk("A charger is detected..  Try WDT reset..");

#ifdef CONFIG_RESET_USES_WATCHDOG
		exynos3250_wdt_reset();
#else
		__raw_writel(0x1, EXYNOS_SWRESET);
#endif
	}
	else{
		printk("No charger is detected..  Try power down..");

		reg = readl(EXYNOS3_PS_HOLD_CONTROL);
		reg &= ~(0x1<<8);
		writel(reg, EXYNOS3_PS_HOLD_CONTROL);
	}
}

static void espresso3250_power_off_prepare(void)
{
	printk("power off prepare the deivce....\n");
}
#endif

static void __init exynos3_dt_map_io(void)
{
	exynos_init_io(NULL, 0);
}

#if defined(CONFIG_SOC_EXYNOS3250)
static void __init espresso3250_power_off_init(void)
{
	//register power off callback
	pm_power_off = espresso3250_power_off;
	pm_power_off_prepare = espresso3250_power_off_prepare;
}
#endif

static void __init exynos3_dt_machine_init(void)
{
#if defined(CONFIG_SOC_EXYNOS3250)
	espresso3250_power_off_init();
#endif
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
}

static char const *exynos3_dt_compat[] __initdata = {
	"samsung,exynos3250",
	NULL
};

static void __init exynos3_reserve(void)
{
	init_exynos_ion_contig_heap();
}

DT_MACHINE_START(EXYNOS3_DT, "Exynos3")
	.init_irq	= exynos3_init_irq,
	.smp		= smp_ops(exynos_smp_ops),
	.map_io		= exynos3_dt_map_io,
	.init_machine	= exynos3_dt_machine_init,
	.init_late	= exynos_init_late,
	.init_time	= exynos_init_time,
	.dt_compat	= exynos3_dt_compat,
	.restart        = exynos3_restart,
	.reserve	= exynos3_reserve,
MACHINE_END
