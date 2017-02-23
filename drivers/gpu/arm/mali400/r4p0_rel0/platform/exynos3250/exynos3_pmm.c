/* drivers/gpu/mali400/mali/platform/exynos3250/exynos3_pmm.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali400 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file exynos4_pmm.c
 * Platform specific Mali driver functions for the exynos 4XXX based platforms
 */
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "exynos3_pmm.h"
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <mach/regs-pmu.h>

#if defined(CONFIG_MALI400_PROFILING)
#include "mali_osk_profiling.h"
#include <linux/types.h>
#include <mach/cpufreq.h>
#include <mach/regs-clock.h>
#include <mach/asv-exynos.h>
#ifdef CONFIG_CPU_FREQ
#define EXYNOS4_ASV_ENABLED
#endif
#endif

/* Some defines changed names in later Odroid-A kernels. Make sure it works for both. */
#ifndef S5P_G3D_CONFIGURATION
#define S5P_G3D_CONFIGURATION EXYNOS4_G3D_CONFIGURATION
#endif
#ifndef S5P_G3D_STATUS
#define S5P_G3D_STATUS (EXYNOS4_G3D_CONFIGURATION + 0x4)
#endif
#ifndef S5P_INT_LOCAL_PWR_EN
#define S5P_INT_LOCAL_PWR_EN EXYNOS_INT_LOCAL_PWR_EN
#endif

#include <asm/io.h>
#include <mach/regs-pmu.h>
#include <linux/workqueue.h>

#ifdef CONFIG_MALI_DVFS

#if defined(CONFIG_EXYNOS_MAX_G3DFREQ_80)
#define MALI_DVFS_STEPS 2
#elif defined(CONFIG_EXYNOS_MAX_G3DFREQ_160)
#define MALI_DVFS_STEPS 3
#else
#define MALI_DVFS_STEPS 6
#endif

#define MALI_DVFS_WATING 10 /* msec */
#define MALI_DVFS_DEFAULT_STEP 0
#define PD_G3D_LOCK_FLAG 2

#define MALI_DVFS_CLK_DEBUG 0

static int bMaliDvfsRun = 0;

typedef struct mali_dvfs_tableTag{
	unsigned int clock;
	unsigned int freq;
	unsigned int int_freq;
	unsigned int downthreshold;
	unsigned int upthreshold;
}mali_dvfs_table;

int mali_dvfs_control;

typedef struct mali_runtime_resumeTag{
		int clk;
		int vol;
		unsigned int step;
}mali_runtime_resume_table;

mali_runtime_resume_table mali_runtime_resume = {80, 50000, 0};

/* dvfs table updated on 07/07/2014 */
mali_dvfs_table mali_dvfs[MALI_DVFS_STEPS]={
	{80, 	1000000, 50000, 0, 	70},
	{80, 	1000000, 80000, 62, 	90},
#if defined(CONFIG_EXYNOS_MAX_G3DFREQ_266) || defined(CONFIG_EXYNOS_MAX_G3DFREQ_160)
	{160, 	1000000, 100000, 85, 	90},
#if defined(CONFIG_EXYNOS_MAX_G3DFREQ_266)
	{266, 	1000000, 133000, 85,  	100}, /* No ISP */
	{266, 	1000000, 134000, 85,  	100}, /* ISP */
	{266, 	1000000, 135000, 85,  	100}  /* ISP Boost */
#endif
#endif
};

char *mali_freq_table = "266 160 80";
typedef struct mali_dvfs_statusTag{
	unsigned int currentStep;
	mali_dvfs_table * pCurrentDvfs;
} mali_dvfs_status_t;
/*dvfs status*/
mali_dvfs_status_t maliDvfsStatus;
#endif//CONFIG_MALI_DVFS
char *dvfs_status = "off";

#define EXTXTALCLK_NAME  "ext_xtal"
#define VPLLSRCCLK_NAME  "mout_vpllsrc"
#define FOUTVPLLCLK_NAME "fout_vpll"
#define SCLVPLLCLK_NAME  "mout_vpll"
#define GPUMOUT1CLK_NAME "mout_g3d1"
#define MPLLCLK_NAME     "mout_mpll"
#define GPUMOUT0CLK_NAME "mout_g3d0"
#define GPUMOUTCLK_NAME "mout_g3d"
#define GPUCLK_NAME      "sclk_g3d"
#define GPU_NAME      "g3d"
#define CLK_DIV_STAT_G3D 0x1003C62C
#define CLK_DESC         "clk-divider-status"

static struct clk *ext_xtal_clock = NULL;
static struct clk *vpll_src_clock = NULL;
static struct clk *mpll_clock = NULL;
static struct clk *mali_mout0_clock = NULL;
static struct clk *mali_mout_clock = NULL;
static struct clk *fout_vpll_clock = NULL;
static struct clk *sclk_vpll_clock = NULL;
static struct clk *mali_parent_clock = NULL;
static struct clk *mali_clock = NULL;
static struct clk *g3d_clock = NULL;

/* PegaW1 */
int mali_gpu_clk = 80;
int mali_gpu_vol = 100000;
static unsigned int GPU_MHZ	= 1000000;
static unsigned int const GPU_ASV_VOLT = 1000;
static int nPowermode;
static atomic_t clk_active;

_mali_osk_mutex_t *mali_dvfs_lock;
mali_io_address clk_register_map = 0;

/* export GPU frequency as a read-only parameter so that it can be read in /sys */
#ifdef CONFIG_MALI400_DEBUG_SYS
DEVICE_ATTR(mali_gpu_clk, S_IRUGO, show_mali_gpu_clk, NULL);
MODULE_PARM_DESC(mali_gpu_clk, "Mali Current Clock");
#ifdef CONFIG_MALI_DVFS
DEVICE_ATTR(mali_avlbl_freq, S_IRUGO,show_mali_freq_table, NULL);
MODULE_PARM_DESC(mali_avlbl_freq, "Mali allowed frequencies");
DEVICE_ATTR(time_in_state, S_IRUGO|S_IWUSR, show_time_in_state, set_time_in_state);
MODULE_PARM_DESC(time_in_state, "Time-in-state of Mali DVFS");
#ifdef CONFIG_REGULATOR
DEVICE_ATTR(mali_gpu_vol, S_IRUGO, show_mali_gpu_vol, NULL);
MODULE_PARM_DESC(mali_gpu_vol, "Mali Current Voltage");
#endif
#endif
DEVICE_ATTR(dvfs_status, S_IRUGO, show_dvfs_status, NULL);
MODULE_PARM_DESC(time_in_state, "Mali DVFS status in kernel");
#endif
#ifdef CONFIG_REGULATOR
struct regulator *g3d_regulator = NULL;
#endif

/* DVFS */
#ifdef CONFIG_MALI_DVFS
static unsigned int mali_dvfs_utilization = 255;
static void update_time_in_state(int level);
u64 mali_dvfs_time[MALI_DVFS_STEPS];

static void mali_dvfs_work_handler(struct work_struct *w);
static struct workqueue_struct *mali_dvfs_wq = 0;
_mali_osk_mutex_t *mali_dvfs_lock;
int mali_runtime_resumed = -1;
static DECLARE_WORK(mali_dvfs_work, mali_dvfs_work_handler);
#endif


#ifdef CONFIG_MALI400_DEBUG_SYS
int mali400_create_sysfs_file(struct device *dev)
{
#ifdef CONFIG_MALI_DVFS
	if (device_create_file(dev, &dev_attr_mali_avlbl_freq)) {
		MALI_PRINT_ERROR(("Couldn't create sysfs file [mali avlbl freq]\n"));
		return -ENOENT;
	}
	if (device_create_file(dev, &dev_attr_time_in_state)) {
		MALI_PRINT_ERROR(("Couldn't create sysfs file [time_in_state]\n"));
		return -ENOENT;
	}
	if (device_create_file(dev, &dev_attr_mali_gpu_vol)) {
		MALI_PRINT_ERROR(("Couldn't create sysfs file [mali_gpu_vol]\n"));
		return -ENOENT;
	}
#endif
	if (device_create_file(dev, &dev_attr_mali_gpu_clk)) {
		MALI_PRINT_ERROR(("Couldn't create sysfs file [mali_gpu_clk]\n"));
		return -ENOENT;
	}
	if (device_create_file(dev, &dev_attr_dvfs_status)) {
		MALI_PRINT_ERROR(("Couldn't create sysfs file [dvfs_status]\n"));
		return -ENOENT;
	}
	return 0;
}

void mali400_remove_sysfs_file(struct device *dev)
{
#ifdef CONFIG_MALI_DVFS
	device_remove_file(dev, &dev_attr_time_in_state);
	device_remove_file(dev, &dev_attr_mali_gpu_vol);
	device_remove_file(dev, &dev_attr_mali_avlbl_freq);
#endif
	device_remove_file(dev, &dev_attr_mali_gpu_clk);
	device_remove_file(dev, &dev_attr_dvfs_status);
}
#endif

#ifdef CONFIG_MALI_DVFS
#ifdef CONFIG_REGULATOR
extern int g3d_regulator_set_voltage(int int_target_freq);
extern void int_g3d_regulator_init(struct regulator *regulator);
void mali_regulator_set_voltage(int int_target_freq)
{
	int g3d_voltage;
	_mali_osk_mutex_wait(mali_dvfs_lock);
	if(IS_ERR_OR_NULL(g3d_regulator)) {
		MALI_DEBUG_PRINT(1, ("error on mali_regulator_set_voltage : g3d_regulator is null\n"));
		_mali_osk_mutex_signal(mali_dvfs_lock);
		return;
	}
	g3d_voltage = g3d_regulator_set_voltage(int_target_freq);
	MALI_DEBUG_PRINT(1, ("= regulator_set_voltage: %d \n", g3d_voltage));

	mali_gpu_vol = regulator_get_voltage(g3d_regulator);
	MALI_DEBUG_PRINT(1, ("Mali voltage: %d\n", mali_gpu_vol));
	_mali_osk_mutex_signal(mali_dvfs_lock);
}
#endif

static unsigned int get_mali_dvfs_status(void)
{
	return maliDvfsStatus.currentStep;
}
#endif
mali_bool mali_clk_get(struct platform_device *pdev)
{
	if (ext_xtal_clock == NULL)	{
		ext_xtal_clock = clk_get(&pdev->dev, EXTXTALCLK_NAME);
		if (IS_ERR(ext_xtal_clock)) {
			MALI_PRINT(("MALI Error : failed to get source ext_xtal_clock\n"));
			return MALI_FALSE;
		}
	}

	if (vpll_src_clock == NULL)	{
		vpll_src_clock = clk_get(&pdev->dev, VPLLSRCCLK_NAME);
		if (IS_ERR(vpll_src_clock)) {
			MALI_PRINT(("MALI Error : failed to get source vpll_src_clock\n"));
			return MALI_FALSE;
		}
	}

	if (fout_vpll_clock == NULL) {
		fout_vpll_clock = clk_get(&pdev->dev, FOUTVPLLCLK_NAME);
		if (IS_ERR(fout_vpll_clock)) {
			MALI_PRINT(("MALI Error : failed to get source fout_vpll_clock\n"));
			return MALI_FALSE;
		}
	}

	if (mpll_clock == NULL)
	{
		mpll_clock = clk_get(&pdev->dev,MPLLCLK_NAME);

		if (IS_ERR(mpll_clock)) {
			MALI_PRINT( ("MALI Error : failed to get source mpll clock\n"));
			return MALI_FALSE;
		}
	}

	if (mali_mout0_clock == NULL)
	{
		mali_mout0_clock = clk_get(&pdev->dev, GPUMOUT0CLK_NAME);

		if (IS_ERR(mali_mout0_clock)) {
			MALI_PRINT( ( "MALI Error : failed to get source mali mout0 clock\n"));
			return MALI_FALSE;
		}
	}

	if (mali_mout_clock == NULL)
	{
		mali_mout_clock = clk_get(&pdev->dev, GPUMOUTCLK_NAME);

		if (IS_ERR(mali_mout_clock)) {
			MALI_PRINT( ( "MALI Error : failed to get source mali mout clock\n"));
			return MALI_FALSE;
		}
	}

	if (sclk_vpll_clock == NULL) {
		sclk_vpll_clock = clk_get(&pdev->dev, SCLVPLLCLK_NAME);
		if (IS_ERR(sclk_vpll_clock)) {
			MALI_PRINT(("MALI Error : failed to get source sclk_vpll_clock\n"));
			return MALI_FALSE;
		}
	}

	if (mali_parent_clock == NULL) {
		mali_parent_clock = clk_get(&pdev->dev, GPUMOUT1CLK_NAME);

		if (IS_ERR(mali_parent_clock)) {
			MALI_PRINT(("MALI Error : failed to get source mali parent clock\n"));
			return MALI_FALSE;
		}
	}

	if (mali_clock == NULL) {
		mali_clock = clk_get(&pdev->dev, GPUCLK_NAME);

		if (IS_ERR(mali_clock)) {
			MALI_PRINT(("MALI Error : failed to get source mali clock\n"));
			return MALI_FALSE;
		}
	}
	if (g3d_clock == NULL) {
		g3d_clock = clk_get(&pdev->dev, GPU_NAME);

		if (IS_ERR(g3d_clock)) {
			MALI_PRINT(("MALI Error : failed to get mali g3d clock\n"));
			return MALI_FALSE;
		}
	}

	return MALI_TRUE;
}

void mali_clk_put(mali_bool binc_mali_clock)
{

	if (mali_parent_clock)
		clk_put(mali_parent_clock);

	if (sclk_vpll_clock)
		clk_put(sclk_vpll_clock);

	if (binc_mali_clock && fout_vpll_clock)
		clk_put(fout_vpll_clock);

	if (mpll_clock)
		clk_put(mpll_clock);

	if (mali_mout0_clock)
		clk_put(mali_mout0_clock);

	if (mali_mout_clock)
		clk_put(mali_mout_clock);

	if (vpll_src_clock)
		clk_put(vpll_src_clock);

	if (ext_xtal_clock)
		clk_put(ext_xtal_clock);

}

void mali_dvfs_clk_set_rate(unsigned int clk, unsigned int mhz)
{
	int err;
	unsigned long rate = (unsigned long)clk * (unsigned long)mhz;
	unsigned long CurRate = 0, MaliRate;

	_mali_osk_mutex_wait(mali_dvfs_lock);
	MALI_DEBUG_PRINT(3, ("Mali platform: Setting frequency to %d mhz\n", clk));

	CurRate = clk_get_rate(mali_clock);

	if (CurRate == 0) {
		_mali_osk_mutex_signal(mali_dvfs_lock);
		MALI_PRINT_ERROR(("clk_get_rate[mali_clock] is 0 - return\n"));
		return;
	}
	err = clk_set_rate(fout_vpll_clock, (unsigned int)clk * GPU_MHZ);
	if (err) {
		_mali_osk_mutex_signal(mali_dvfs_lock);
		MALI_PRINT_ERROR(("Failed to set fout_vpll clock:\n"));
		return;
	}

	MaliRate = clk_get_rate(mali_clock);
	if(MaliRate != rate) {
		_mali_osk_mutex_signal(mali_dvfs_lock);
		MALI_PRINT_ERROR(("Failed to set mali rate to %lu\n",rate));
		return;
	}
	MALI_DEBUG_PRINT(1, ("Mali frequency %d\n", rate / mhz));
	GPU_MHZ = mhz;

	mali_gpu_clk = (int)(rate / mhz);
	mali_clk_put(MALI_FALSE);
	_mali_osk_mutex_signal(mali_dvfs_lock);
}

void mali_clk_set_rate(struct platform_device *pdev, unsigned int clk, unsigned int mhz)
{
	int err;
	unsigned long rate = (unsigned long)clk * (unsigned long)mhz;

	_mali_osk_mutex_wait(mali_dvfs_lock);
	MALI_DEBUG_PRINT(3, ("Mali platform: Setting frequency to %d mhz\n", clk));

	if (mali_clk_get(pdev) == MALI_FALSE) {
		_mali_osk_mutex_signal(mali_dvfs_lock);
		return;
	}

	err = clk_set_rate(mali_clock, rate);
	if (err)
		MALI_PRINT_ERROR(("Failed to set Mali clock: %d\n", err));

	rate = clk_get_rate(mali_clock);
	GPU_MHZ = mhz;

	mali_gpu_clk = rate / mhz;
	MALI_DEBUG_PRINT(1, ("Mali frequency %dMhz\n", rate / mhz));

	mali_clk_put(MALI_FALSE);

	_mali_osk_mutex_signal(mali_dvfs_lock);
}

#ifdef CONFIG_MALI_DVFS
mali_bool set_mali_dvfs_current_step(unsigned int step)
{
	_mali_osk_mutex_wait(mali_dvfs_lock);
	maliDvfsStatus.currentStep = step % MALI_DVFS_STEPS;
	_mali_osk_mutex_signal(mali_dvfs_lock);
	return MALI_TRUE;
}

static mali_bool set_mali_dvfs_status(u32 step,mali_bool boostup)
{

	if(boostup)	{
#ifdef CONFIG_REGULATOR
		/*change the voltage*/
		mali_regulator_set_voltage(mali_dvfs[step].int_freq);
#endif
		/*change the clock*/
		mali_dvfs_clk_set_rate(mali_dvfs[step].clock, mali_dvfs[step].freq);
	} else {
		/*change the clock*/
		mali_dvfs_clk_set_rate(mali_dvfs[step].clock, mali_dvfs[step].freq);
#ifdef CONFIG_REGULATOR
		mali_regulator_set_voltage(mali_dvfs[step].int_freq);
#endif
	}

	mali_clk_put(MALI_FALSE);

	set_mali_dvfs_current_step(step);
	/*for future use*/
	maliDvfsStatus.pCurrentDvfs = &mali_dvfs[step];

	return MALI_TRUE;
}

static void mali_platform_wating(u32 msec)
{
	/*
	* sample wating
	* change this in the future with proper check routine.
	*/
	unsigned int read_val;
	while(1) {
		read_val = _mali_osk_mem_ioread32(clk_register_map, 0x00);
		if ((read_val & 0x8000)==0x0000) break;

		_mali_osk_time_ubusydelay(100); /* 1000 -> 100 : 20101218 */
	}
}

static mali_bool change_mali_dvfs_status(u32 step, mali_bool boostup )
{
	MALI_DEBUG_PRINT(4, ("> change_mali_dvfs_status: %d, %d \n", step, boostup));

	if (!set_mali_dvfs_status(step, boostup)) {
		MALI_DEBUG_PRINT(1, ("error on set_mali_dvfs_status: %d, %d \n",step, boostup));
		return MALI_FALSE;
	}

	/* wait until clock and voltage is stablized */
	mali_platform_wating(MALI_DVFS_WATING); /* msec */
	return MALI_TRUE;
}

static unsigned int decideNextStatus(unsigned int utilization)
{
	static unsigned int level = 0;
	int iStepCount = 0;

	if (mali_runtime_resumed >= 0) {
		level = mali_runtime_resumed;
		mali_runtime_resumed = -1;
	}

	if (mali_dvfs_control == 0) {
		if (utilization > (int)(255 * mali_dvfs[maliDvfsStatus.currentStep].upthreshold / 100) &&
				level < MALI_DVFS_STEPS - 1) {
			level++;
		} else if (utilization < (int)(255 * mali_dvfs[maliDvfsStatus.currentStep].downthreshold / 100) &&
			level > 0) {
			level--;
		}
	} else {
		for (iStepCount = MALI_DVFS_STEPS - 1; iStepCount >= 0; iStepCount--) {
			if (mali_dvfs_control >= mali_dvfs[iStepCount].clock) {
				level = iStepCount;
				break;
			}
		}
	}

	return level;
}

static mali_bool mali_dvfs_status(unsigned int utilization)
{
	unsigned int nextStatus = 0;
	unsigned int curStatus = 0;
	mali_bool boostup = MALI_FALSE;
	static int stay_count = 5;

	MALI_DEBUG_PRINT(4, ("> mali_dvfs_status: %d \n", utilization));

	/* decide next step */
	curStatus = get_mali_dvfs_status();
	nextStatus = decideNextStatus(utilization);

	MALI_DEBUG_PRINT(4, ("= curStatus %d, nextStatus %d, maliDvfsStatus.currentStep %d \n", curStatus, nextStatus, maliDvfsStatus.currentStep));
	/* if next status is same with current status, don't change anything */
	if(curStatus != nextStatus) {
		/*check if boost up or not*/
		if(maliDvfsStatus.currentStep < nextStatus) {
			boostup = 1;
			stay_count = 5;
		} else if (maliDvfsStatus.currentStep > nextStatus){
			stay_count--;
		}

		if( boostup == 1 || stay_count <= 0){
			/*change mali dvfs status*/
			update_time_in_state(curStatus);
			if (!change_mali_dvfs_status(nextStatus, boostup)) {
				MALI_DEBUG_PRINT(1, ("error on change_mali_dvfs_status \n"));
				return MALI_FALSE;
			}
			boostup = 0;
			stay_count = 5;
		}
	}
	else
		stay_count = 5;

	return MALI_TRUE;
}

static void mali_dvfs_work_handler(struct work_struct *w)
{
	bMaliDvfsRun = 1;
	MALI_DEBUG_PRINT(3, ("=== mali_dvfs_work_handler\n"));

	if(!mali_dvfs_status(mali_dvfs_utilization))
		MALI_DEBUG_PRINT(1, ( "error on mali dvfs status in mali_dvfs_work_handler"));

	bMaliDvfsRun = 0;
}

mali_bool init_mali_dvfs_status(void)
{
	/*
	* default status
	* add here with the right function to get initilization value.
	*/
	if (!mali_dvfs_wq)
		mali_dvfs_wq = create_singlethread_workqueue("mali_dvfs");

	/* add a error handling here */
	maliDvfsStatus.currentStep = MALI_DVFS_DEFAULT_STEP;

	return MALI_TRUE;
}

void deinit_mali_dvfs_status(void)
{
	if (mali_dvfs_wq)
		destroy_workqueue(mali_dvfs_wq);

	mali_dvfs_wq = NULL;
}

mali_bool mali_dvfs_handler(unsigned int utilization)
{
	mali_dvfs_utilization = utilization;
	queue_work_on(0, mali_dvfs_wq, &mali_dvfs_work);

	return MALI_TRUE;
}
#endif

static mali_bool configure_mali_clocks(struct platform_device *pdev)
{
	int err = 0;
	mali_bool ret = MALI_TRUE;

	if (!mali_clk_get(pdev))
	{
		MALI_PRINT_ERROR(("Failed to get Mali clock\n"));
		ret = MALI_FALSE;
		goto err_clk;
	}

	err = clk_set_rate(fout_vpll_clock, (unsigned int)mali_gpu_clk * GPU_MHZ);
	if (err)
		MALI_PRINT_ERROR(("Failed to set fout_vpll clock:\n"));

	err = clk_set_parent(vpll_src_clock, ext_xtal_clock);
	if (err)
		MALI_PRINT_ERROR(("vpll_src_clock set parent to ext_xtal_clock failed\n"));

	err = clk_set_parent(sclk_vpll_clock, fout_vpll_clock);
	if (err)
		MALI_PRINT_ERROR(("sclk_vpll_clock set parent to fout_vpll_clock failed\n"));

	err = clk_set_parent(mali_parent_clock, sclk_vpll_clock);
	if (err)
		MALI_PRINT_ERROR(("mali_parent_clock set parent to sclk_vpll_clock failed\n"));

	err = clk_set_parent(mali_mout_clock, mali_parent_clock);
	if (err)
		MALI_PRINT_ERROR(("mali_clock set parent to mali_parent_clock failed\n"));
	if (!atomic_read(&clk_active)) {
		if ((clk_prepare_enable(mali_clock)  < 0)
				|| (clk_prepare_enable(g3d_clock)  < 0)) {
			MALI_PRINT_ERROR(("Failed to enable clock\n"));
			goto err_clk;
		}
		atomic_set(&clk_active, 1);
	}

	mali_clk_set_rate(pdev,(unsigned int)mali_gpu_clk, GPU_MHZ);
	mali_clk_put(MALI_FALSE);

	return MALI_TRUE;
err_clk:
	mali_clk_put(MALI_TRUE);
	return ret;
}

static mali_bool init_mali_clock(struct platform_device *pdev)
{
	mali_bool ret = MALI_TRUE;
	nPowermode = MALI_POWER_MODE_DEEP_SLEEP;

	if (mali_clock != 0)
		return ret; /* already initialized */

	mali_dvfs_lock = _mali_osk_mutex_init(_MALI_OSK_LOCKFLAG_ORDERED, 0);

	if (mali_dvfs_lock == NULL)
		return _MALI_OSK_ERR_FAULT;

        ret = configure_mali_clocks(pdev);
	/* The VPLL clock should not be clock-gated for it caused
	 * hangs in S2R */
	clk_prepare_enable(fout_vpll_clock);

	if (ret != MALI_TRUE)
		goto err_mali_clock;

#ifdef CONFIG_MALI_DVFS
#ifdef CONFIG_REGULATOR
	if(g3d_regulator == NULL) {
		g3d_regulator = regulator_get(NULL, "vdd_int");
		if (IS_ERR(g3d_regulator)) {
			MALI_PRINT(("MALI Error : failed to get vdd_int for g3d\n"));
			ret = MALI_FALSE;
			regulator_put(g3d_regulator);
			g3d_regulator = NULL;
			return MALI_FALSE;
		}

		int_g3d_regulator_init(g3d_regulator);

		mali_gpu_vol = mali_runtime_resume.vol;
#ifdef EXYNOS4_ASV_ENABLED
		mali_gpu_vol = get_match_volt(ID_G3D, mali_gpu_clk * GPU_ASV_VOLT);
		mali_runtime_resume.vol = get_match_volt(ID_G3D, mali_runtime_resume.clk * GPU_ASV_VOLT);
#endif
		//regulator_enable(g3d_regulator);
		//mali_regulator_set_voltage(mali_gpu_vol);
#if defined(EXYNOS4_ASV_ENABLED) && defined(EXYNOS4_ABB_ENABLED)
		exynos_set_abb(ID_G3D, get_match_abb(ID_G3D, mali_runtime_resume.clk * GPU_ASV_VOLT));
#endif
	}
#endif
#endif
	mali_clk_put(MALI_FALSE);

	return MALI_TRUE;

#ifdef CONFIG_MALI_DVFS
#endif
err_mali_clock:
	return ret;
}

static mali_bool deinit_mali_clock(void)
{
	if (mali_clock == 0 || g3d_clock == 0)
		return MALI_TRUE;

	mali_clk_put(MALI_TRUE);
	return MALI_TRUE;
}

static _mali_osk_errcode_t enable_mali_clocks(struct device *dev)
{
	int err;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	if (!atomic_read(&clk_active)) {
		err = clk_prepare_enable(mali_clock);
		err = clk_prepare_enable(g3d_clock);
		atomic_set(&clk_active, 1);
	}

	MALI_DEBUG_PRINT(3,("enable_mali_clocks mali_clock %p error %d \n", mali_clock, err));
	/*
	 * Right now we are configuring clock each time, during runtime
	 * s2r and s2r as it has been observed mali failed to enter into
	 * deep sleep state during s2r.
	 * If this gets fixed we *MUST* remove LIGHT_SLEEP condition from below
	 */
	if (nPowermode == MALI_POWER_MODE_DEEP_SLEEP ||
			nPowermode == MALI_POWER_MODE_LIGHT_SLEEP)
		configure_mali_clocks(pdev);


	/* set clock rate */
#ifdef CONFIG_MALI_DVFS
	if (mali_dvfs_control != 0 || mali_gpu_clk >= mali_runtime_resume.clk) {
		mali_dvfs_clk_set_rate(mali_gpu_clk, GPU_MHZ);
	} else {
#ifdef CONFIG_REGULATOR
		mali_regulator_set_voltage(mali_runtime_resume.vol);
#if defined(EXYNOS4_ASV_ENABLED) && defined(EXYNOS4_ABB_ENABLED)
		exynos_set_abb(ID_G3D, get_match_abb(ID_G3D, mali_runtime_resume.clk * GPU_ASV_VOLT));
#endif
#endif
		mali_dvfs_clk_set_rate(mali_runtime_resume.clk, GPU_MHZ);
		set_mali_dvfs_current_step(mali_runtime_resume.step);
	}
#endif

	MALI_SUCCESS;
}

static _mali_osk_errcode_t disable_mali_clocks(void)
{
#if defined(EXYNOS4_ASV_ENABLED) && defined(EXYNOS4_ABB_ENABLED)
	if (samsung_rev() == EXYNOS3470_REV_2_0)
		exynos_set_abb(ID_G3D, ABB_BYPASS);
#endif

	if (atomic_read(&clk_active)) {
		clk_disable_unprepare(mali_clock);
		clk_put(mali_clock);
		clk_disable_unprepare(g3d_clock);
		clk_put(g3d_clock);
		deinit_mali_clock();
		MALI_DEBUG_PRINT(3, ("disable_mali_clocks mali_clock %p g3d %p\n", mali_clock,g3d_clock));
		atomic_set(&clk_active, 0);
	}

	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_init(struct platform_device *pdev)
{
	atomic_set(&clk_active, 0);
#ifdef CONFIG_MALI_DVFS
	if (!clk_register_map)
		clk_register_map = _mali_osk_mem_mapioregion(CLK_DIV_STAT_G3D, 0x20, CLK_DESC);

	if (!init_mali_dvfs_status())
		MALI_DEBUG_PRINT(1, ("mali_platform_init failed\n"));

	maliDvfsStatus.currentStep = MALI_DVFS_DEFAULT_STEP;
	dvfs_status = "on";
#endif
	MALI_CHECK(init_mali_clock(pdev), _MALI_OSK_ERR_FAULT);
	mali_platform_power_mode_change(&pdev->dev, MALI_POWER_MODE_ON);

	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit(struct platform_device *pdev)
{
	mali_platform_power_mode_change(&pdev->dev, MALI_POWER_MODE_DEEP_SLEEP);
	deinit_mali_clock();
	clk_disable_unprepare(fout_vpll_clock);

#ifdef CONFIG_MALI_DVFS
	deinit_mali_dvfs_status();
	if (clk_register_map) {
		_mali_osk_mem_unmapioregion(CLK_DIV_STAT_G3D, 0x20, clk_register_map);
		clk_register_map = NULL;
	}
#ifdef CONFIG_REGULATOR
	if (g3d_regulator) {
		regulator_put(g3d_regulator);
		g3d_regulator = NULL;
	}
#endif
#endif
	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_power_mode_change(struct device *dev, mali_power_mode power_mode)
{
	switch (power_mode)
	{
	case MALI_POWER_MODE_ON:
		MALI_DEBUG_PRINT(3, ("Mali platform: Got MALI_POWER_MODE_ON event, %s\n",
					nPowermode ? "powering on" : "already on"));
		if (nPowermode == MALI_POWER_MODE_LIGHT_SLEEP || nPowermode == MALI_POWER_MODE_DEEP_SLEEP)	{
			MALI_DEBUG_PRINT(4, ("enable clock\n"));
			enable_mali_clocks(dev);
			nPowermode = power_mode;
		}
		break;
	case MALI_POWER_MODE_DEEP_SLEEP:
	case MALI_POWER_MODE_LIGHT_SLEEP:
		MALI_DEBUG_PRINT(3, ("Mali platform: Got %s event, %s\n", power_mode == MALI_POWER_MODE_LIGHT_SLEEP ?
					"MALI_POWER_MODE_LIGHT_SLEEP" : "MALI_POWER_MODE_DEEP_SLEEP",
					nPowermode ? "already off" : "powering off"));
		if (nPowermode == MALI_POWER_MODE_ON)	{
			disable_mali_clocks();
			nPowermode = power_mode;
		}
		break;
	}
	MALI_SUCCESS;
}

void mali_gpu_utilization_handler(struct mali_gpu_utilization_data *data)
{
	if (nPowermode == MALI_POWER_MODE_ON)
	{
#ifdef CONFIG_MALI_DVFS
		if(!mali_dvfs_handler(data->utilization_gpu))
			MALI_DEBUG_PRINT(1, ("error on mali dvfs status in utilization\n"));
#endif
	}
}

#ifdef CONFIG_MALI_DVFS
static void update_time_in_state(int level)
{
	u64 current_time;
	static u64 prev_time = 0;

	if (prev_time == 0)
		prev_time = get_jiffies_64();

	current_time = get_jiffies_64();
	mali_dvfs_time[level] += current_time - prev_time;
	prev_time = current_time;
}

#ifdef CONFIG_MALI400_DEBUG_SYS

ssize_t show_mali_freq_table(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "%s\n", mali_freq_table);
	return ret;
}
#ifdef CONFIG_REGULATOR
ssize_t show_mali_gpu_vol(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d\n", regulator_get_voltage(g3d_regulator));
	return ret;
}
#endif
ssize_t show_time_in_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i;

	update_time_in_state(maliDvfsStatus.currentStep);

	for (i = 0; i < MALI_DVFS_STEPS; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d %llu\n",
						mali_dvfs[i].clock,
						mali_dvfs_time[i]);
	}

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	} else {
		buf[PAGE_SIZE - 2] = '\n';
		buf[PAGE_SIZE - 1] = '\0';
		ret = PAGE_SIZE - 1;
	}
	return ret;
}

ssize_t set_time_in_state(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int i;

	for (i = 0; i < MALI_DVFS_STEPS; i++) {
		mali_dvfs_time[i] = 0;
	}
	return count;
}

#endif// CONFIG_MALI400_DEBUG_SYS
#endif
#ifdef CONFIG_MALI400_DEBUG_SYS
ssize_t show_mali_gpu_clk(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	ret += snprintf(buf + ret , PAGE_SIZE - ret, "%d\n", (int)(clk_get_rate(mali_clock)/GPU_MHZ));
	return ret;
}
ssize_t show_dvfs_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	ret += snprintf(buf + ret , PAGE_SIZE - ret, "%s\n", dvfs_status);
	return ret;
}
#endif
