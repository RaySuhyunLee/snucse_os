/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos3250 SoC.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <dt-bindings/clk/exynos3250-clk.h>

#include <mach/regs-clock-exynos3.h>

#include "clk.h"
#include "clk-pll.h"

#define APLL_LOCK		EXYNOS3_CPU_ISP_REG(0x4000)
#define APLL_CON0		EXYNOS3_CPU_ISP_REG(0x4100)
#define BPLL_LOCK		EXYNOS3_MIF_L_REG(0x0118)
#define BPLL_CON0		EXYNOS3_MIF_L_REG(0x0218)
#define EPLL_LOCK		EXYNOS3_MIF_L_REG(0x1110)
#define EPLL_CON0		EXYNOS3_MIF_L_REG(0x1114)
#define MPLL_LOCK		EXYNOS3_CLK_BUS_TOP_REG(0xC010)
#define MPLL_CON0		EXYNOS3_CLK_BUS_TOP_REG(0xC110)
#define VPLL_LOCK		EXYNOS3_CLK_BUS_TOP_REG(0xC020)
#define VPLL_CON0		EXYNOS3_CLK_BUS_TOP_REG(0xC120)
#define UPLL_LOCK		EXYNOS3_CLK_BUS_TOP_REG(0xC030)
#define UPLL_CON0		EXYNOS3_CLK_BUS_TOP_REG(0xC130)
#define PWR_CTRL1		EXYNOS3_CPU_ISP_REG(0x5020)
#define PWR_CTRL2		EXYNOS3_CPU_ISP_REG(0x5024)

/* Below definitions are used for PWR_CTRL settings */
#define PWR_CTRL1_CORE2_DOWN_RATIO(x)		(((x) & 0x7) << 28)
#define PWR_CTRL1_CORE1_DOWN_RATIO(x)		(((x) & 0x7) << 16)
#define PWR_CTRL1_DIV2_DOWN_EN			(1 << 9)
#define PWR_CTRL1_DIV1_DOWN_EN			(1 << 8)
#define PWR_CTRL1_USE_CORE3_WFE			(1 << 7)
#define PWR_CTRL1_USE_CORE2_WFE			(1 << 6)
#define PWR_CTRL1_USE_CORE1_WFE			(1 << 5)
#define PWR_CTRL1_USE_CORE0_WFE			(1 << 4)
#define PWR_CTRL1_USE_CORE3_WFI			(1 << 3)
#define PWR_CTRL1_USE_CORE2_WFI			(1 << 2)
#define PWR_CTRL1_USE_CORE1_WFI			(1 << 1)
#define PWR_CTRL1_USE_CORE0_WFI			(1 << 0)

static __initdata void *exynos3250_clk_regs[] = {
	BPLL_LOCK,
	BPLL_CON0,
	EPLL_LOCK,
	EPLL_CON0,
	VPLL_LOCK,
	VPLL_CON0,
	UPLL_LOCK,
	UPLL_CON0,
	DIV_LEFTBUS,
	DIV_RIGHTBUS,
	DIV_TOP,
	DIV_FSYS0,
	DIV_FSYS1,
	DIV_FSYS2,
	DIV_PERIL0,
	DIV_PERIL1,
	DIV_PERIL4,
	DIV_LCD,
	DIV_G3D,
	DIV_MFC,
	DIV_CAM,
	SRC_LEFTBUS,
	SRC_RIGHTBUS,
	SRC_TOP0,
	SRC_TOP1,
	SRC_PERIL0,
	SRC_LCD,
	SRC_FSYS,
	SRC_G3D,
	SRC_MFC,
	SRC_CAM,
	SRC_PERIL1,
	GATE_SCLK_PERIL,
	GATE_IP_PERIR,
	GATE_SCLK_PERIL,
	GATE_IP_PERIR,
	GATE_IP_PERIL,
	GATE_BUS_LCD,
	GATE_SCLK_LCD,
	GATE_IP_LCD,
	GATE_BUS_FSYS0,
	GATE_SCLK_FSYS,
	GATE_IP_FSYS,
	GATE_IP_LEFTBUS,
	GATE_IP_RIGHTBUS,
	GATE_SCLK_MFC,
	GATE_SCLK_G3D,
	GATE_SCLK_CAM,
	SRC_MASK_CAM,
	GATE_IP_G3D,
	GATE_IP_MFC,
	GATE_IP_CAM,
	GATE_IP_PERIR,
	GATE_IP_ACP0,
	PWR_CTRL1,
	PWR_CTRL2,
};

PNAME(mout_gdl_p)		= { "mout_mpll_user_l" };
PNAME(mout_gdr_p)		= { "mout_mpll_user_r" };
PNAME(mout_mpll_user_r_p)	= { "fin_pll", "dout_mpll_pre" };
PNAME(mout_mpll_user_p)		= { "fin_pll", "dout_mpll_pre" };
PNAME(group_sclk_p)		= { "xxti", "xusbxti", "none", "none", "none",
					"none",	"dout_mpll_pre",
					"mout_epll", "mout_vpll" };
PNAME(group_sclk_audio_p)       = { "audiocdclk", "none",
					"none", "none", "xxti", "xusbxti",
					"dout_mpll_pre", "mout_epll",
					"mout_vpll" };
PNAME(group_sclk_fimd0_p)	= { "xxti", "xusbxti", "m_bitclkhsdiv4_2l",
					"none", "none",	"none",	"dout_mpll_pre",
					"mout_epll", "mout_vpll", "none",
					"none", "none", "div_lcd_blk_145" };
PNAME(mout_mpll_p)		= { "fin_pll", "fout_mpll" };
PNAME(mout_vpllsrc_p)		= { "fin_pll", "none" };
PNAME(mout_g3d_p)		= { "mout_g3d_0", "mout_g3d_1" };
PNAME(group_div_mpll_pre_p)	= { "dout_mpll_pre", "none" };
PNAME(group_epll_vpll_p)	= { "mout_epll", "mout_vpll" };
PNAME(mout_epll_p)		= { "fin_pll", "fout_epll" };
PNAME(mout_vpll_p)		= { "fin_pll", "fout_vpll" };
PNAME(mout_upll_p)		= { "fin_pll", "fout_upll", };
PNAME(mout_mfc_p)		= { "mout_mfc_0", "mout_mfc_1" };
PNAME(mout_cam_blk_p)		= { "xxti", "xusbxti", "none", "none", "none",
					"none", "dout_mpll_pre", "mout_epll",
					"mout_vpll", "none", "none", "none",
					"div_cam_blk_320" };
PNAME(mout_mpll_user_sel_p)	= { "fin_pll", "dout_mpll_pre" };
PNAME(mout_apll_sel_p)		= { "fin_pll", "fout_apll" };
PNAME(mout_core_sel_p)		= { "mout_apll_sel", "mout_mpll_user_sel" };
PNAME(mout_aclk_400_mcuisp_p)	= { "dout_mpll_pre", "none" };
PNAME(mout_aclk_266_0_p)	= { "dout_mpll_pre", "mout_vpll" };
PNAME(mout_aclk_266_1_p)	= { "mout_epll", "none" };
PNAME(mout_aclk_266_p)		= { "mout_aclk_266_0", "mout_aclk_266_1" };
PNAME(mout_aclk_400_mcuisp_sub_p) = { "fin_pll", "dout_aclk_400_mcuisp" };
PNAME(mout_aclk_266_sub_p)	= { "fin_pll", "dout_aclk_266" };
PNAME(mout_isp_p)		= { "fin_pll", "xusbxti", "dout_mpll_pre",
					"mout_epll", "mout_vpll" };

static struct samsung_fixed_factor_clock fixed_factor_clks[] __initdata = {
	FFACTOR(0, "sclk_mpll_1600", "mout_mpll", 1, 1, 0),
	FFACTOR(0, "sclk_mpll_mif", "mout_mpll", 1, 2, 0),
	FFACTOR(0, "sclk_bpll", "fout_bpll", 1, 2, 0),
	FFACTOR(CLK_CAM_BLK_320, "div_cam_blk_320", "sclk_mpll_1600", 1, 5, 0),
	FFACTOR(0, "div_lcd_blk_145", "sclk_mpll_1600", 1, 11, 0),
	/* HACK:
	 * As per user manual:
	 * We can use XTCXO or XUSBXTI as main clock source.
	 * It's selected by XOSCSEL pad, while XOSCSEL = 0, clock source is from XTCXO
	 * While XOSCSEL = 1, clock source is from XUSBXTI
	 * So till this detection machnism happens let clock source be XUSBXTI (24MHz)
	 */
	FFACTOR(CLK_FIN_PLL, "fin_pll", "xusbxti", 1, 1, 0),
};


#define CMUX(_id, cname, pnames, o, s, w) \
	MUX(_id, cname, pnames, (unsigned long)o, s, w)
#define CMUX_A(_id, cname, pnames, o, s, w, a) \
	MUX_A(_id, cname, pnames, (unsigned long)o, s, w, a)

static struct samsung_mux_clock exynos3250_mux_clks[] __initdata = {
	CMUX(CLK_MOUT_MPLL_USER_L, "mout_mpll_user_l", mout_mpll_user_p,
			SRC_LEFTBUS, 4, 1),
	CMUX(CLK_MOUT_GDL, "mout_gdl", mout_gdl_p, SRC_LEFTBUS, 0, 1),

	CMUX(CLK_MOUT_MPLL_USER_R, "mout_mpll_user_r", mout_mpll_user_r_p,
		SRC_RIGHTBUS, 4, 1),
	CMUX(CLK_MOUT_GDR, "mout_gdr", mout_gdr_p, SRC_RIGHTBUS, 0, 1),

	CMUX(CLK_MOUT_UPLL, "mout_upll", mout_upll_p, SRC_TOP1, 28, 1),
	CMUX(CLK_MOUT_ACLK_400_MCUISP_SUB, "mout_aclk_400_mcuisp_sub",
		mout_aclk_400_mcuisp_sub_p, SRC_TOP1, 24, 1),
	CMUX(CLK_MOUT_ACLK_266_SUB, "mout_aclk_266_sub", mout_aclk_266_sub_p,
		SRC_TOP1, 20, 1),
	CMUX_A(CLK_MOUT_MPLL, "mout_mpll", mout_mpll_p,
		SRC_TOP1, 12, 1, "sclk_mpll"),
	CMUX(CLK_MOUT_ACLK_400_MCUISP, "mout_aclk_400_mcuisp",
		mout_aclk_400_mcuisp_p, SRC_TOP1, 8, 1),
	CMUX(CLK_MOUT_VPLLSRC, "mout_vpllsrc", mout_vpllsrc_p,
		SRC_TOP1, 0, 1),

	CMUX(CLK_MOUT_ACLK_200, "mout_aclk200", group_div_mpll_pre_p,
		SRC_TOP0, 24, 1),
	CMUX(CLK_MOUT_ACLK_160, "mout_aclk_160", group_div_mpll_pre_p,
		SRC_TOP0, 20, 0),
	CMUX(CLK_MOUT_ACLK_100, "mout_aclk100", group_div_mpll_pre_p,
		SRC_TOP0, 16, 1),
	CMUX(CLK_MOUT_ACLK_266_1, "mout_aclk_266_1", mout_aclk_266_1_p,
		SRC_TOP0, 14, 1),
	CMUX(CLK_MOUT_ACLK_266_0, "mout_aclk_266_0", mout_aclk_266_0_p,
		SRC_TOP0, 13, 1),
	CMUX(CLK_MOUT_ACLK_266, "mout_aclk_266", mout_aclk_266_p,
		SRC_TOP0, 12, 1),
	CMUX(CLK_MOUT_VPLL, "mout_vpll", mout_vpll_p, SRC_TOP0, 8, 1),
	CMUX(CLK_MOUT_EPLL, "mout_epll", mout_epll_p, SRC_TOP0, 4, 1),

	CMUX(CLK_MOUT_UART3, "mout_uart3", group_sclk_p, SRC_PERIL0, 12, 4),
	CMUX(CLK_MOUT_UART2, "mout_uart2", group_sclk_p, SRC_PERIL0, 8, 4),
	CMUX(CLK_MOUT_UART1, "mout_uart1", group_sclk_p, SRC_PERIL0, 4, 4),
	CMUX(CLK_MOUT_UART0, "mout_uart0", group_sclk_p, SRC_PERIL0, 0, 4),

	CMUX(CLK_MOUT_MIPI0, "mout_mipi0", group_sclk_p, SRC_LCD, 12, 4),
	CMUX(CLK_MOUT_FIMD0, "mout_fimd0", group_sclk_fimd0_p, SRC_LCD, 0, 4),

	CMUX(CLK_MOUT_TSADC, "mout_tsadc", group_sclk_p, SRC_FSYS, 28, 4),
	CMUX(CLK_MOUT_MMC2, "mout_mmc2", group_sclk_p, SRC_FSYS, 8, 4),
	CMUX(CLK_MOUT_MMC1, "mout_mmc1", group_sclk_p, SRC_FSYS, 4, 4),
	CMUX(CLK_MOUT_MMC0, "mout_mmc0", group_sclk_p, SRC_FSYS, 0, 4),

	CMUX(CLK_MOUT_G3D, "mout_g3d", mout_g3d_p, SRC_G3D, 8, 1),
	CMUX(CLK_MOUT_G3D_1, "mout_g3d_1", group_epll_vpll_p, SRC_G3D, 4, 1),
	CMUX(CLK_MOUT_G3D_0, "mout_g3d_0", group_div_mpll_pre_p, SRC_G3D, 0, 1),

	CMUX(CLK_MOUT_MFC, "mout_mfc", mout_mfc_p, SRC_MFC, 8, 1),
	CMUX(CLK_MOUT_MFC_1, "mout_mfc_1", group_epll_vpll_p, SRC_MFC, 4, 1),
	CMUX(CLK_MOUT_MFC_0, "mout_mfc_0", group_div_mpll_pre_p, SRC_MFC, 0, 1),

	CMUX(CLK_MOUT_CAM1, "mout_cam1", group_sclk_p, SRC_CAM, 20, 4),
	CMUX(CLK_MOUT_CAM_BLK, "mout_cam_blk", mout_cam_blk_p, SRC_CAM, 0, 4),
	CMUX(CLK_MOUT_SPI1, "mout_spi1", group_sclk_p, SRC_PERIL1, 20, 4),
	CMUX(CLK_MOUT_SPI0, "mout_spi0", group_sclk_p, SRC_PERIL1, 16, 4),
	CMUX_A(0, "mout_mpll_user_sel", mout_mpll_user_sel_p, SRC_CPU, 24, 1,
		"mout_mpll"),
	CMUX_A(0, "mout_core_sel", mout_core_sel_p, SRC_CPU, 16, 1,
		"mout_core"),
	CMUX_A(0, "mout_apll_sel", mout_apll_sel_p, SRC_CPU, 0, 1,
		"mout_apll"),

	CMUX(CLK_MOUT_UART_ISP, "mout_uart_isp", mout_isp_p, SRC_ISP, 12, 3),
	CMUX(CLK_MOUT_SPI1_ISP, "mout_spi1_isp", mout_isp_p, SRC_ISP, 8, 3),
	CMUX(CLK_MOUT_SPI0_ISP, "mout_spi0_isp", mout_isp_p, SRC_ISP, 4, 3),
	CMUX(CLK_MOUT_AUDIO, "mout_audio", group_sclk_audio_p, SRC_PERIL1, 4, 4),
};

#define CDIV(_id, cname, pname, o, s, w) \
			DIV(_id, cname, pname, (unsigned long)o, s, w)
#define CDIV_F(_id, cname, pname, o, s, w, f, df) \
			DIV_F(_id, cname, pname, (unsigned long)o, s, w, f, df)

static struct samsung_div_clock exynos3250_div_clks[] __initdata = {
	CDIV(CLK_DIV_GPL, "dout_gpl", "dout_gdl", DIV_LEFTBUS, 4, 3),
	CDIV(CLK_DIV_GDL, "dout_gdl", "mout_gdl", DIV_LEFTBUS, 0, 4),

	CDIV(CLK_DIV_GPR, "dout_gpr", "dout_gdr", DIV_RIGHTBUS, 4, 3),
	CDIV(CLK_DIV_GDR, "dout_gdr", "mout_gdr", DIV_RIGHTBUS, 0, 4),

	CDIV(CLK_DIV_MPLL_PRE, "dout_mpll_pre", "sclk_mpll_mif", DIV_TOP, 28, 2),
	CDIV(CLK_DIV_ACLK_400_MCUISP, "dout_aclk_400_mcuisp",
			"mout_aclk_400_mcuisp", DIV_TOP, 24, 3),
	CDIV(CLK_DIV_ACLK_200, "dout_aclk_200", "mout_aclk200", DIV_TOP, 12, 3),
	CDIV(CLK_DIV_ACLK_160, "dout_aclk_160", "mout_aclk_160", DIV_TOP, 8, 3),
	CDIV(CLK_DIV_ACLK_100, "dout_aclk_100", "mout_aclk100", DIV_TOP, 4, 4),
	CDIV(CLK_DIV_ACLK_266, "dout_aclk_266", "mout_aclk_266", DIV_TOP, 0, 3),

	CDIV_F(CLK_DIV_TSADC_PRE, "dout_tsadc_pre", "dout_tsadc", DIV_FSYS0, 8,
			8, CLK_SET_RATE_PARENT, 0),
	CDIV(CLK_DIV_TSADC, "dout_tsadc", "mout_tsadc", DIV_FSYS0, 0, 4),

	CDIV_F(CLK_DIV_MMC2_PRE, "dout_mmc2_pre", "dout_mmc2", DIV_FSYS2, 8, 8,
			CLK_SET_RATE_PARENT, 0),
	CDIV(CLK_DIV_MMC2, "dout_mmc2", "mout_mmc2", DIV_FSYS2, 0, 4),

	CDIV_F(CLK_DIV_MMC1_PRE, "dout_mmc1_pre", "dout_mmc1", DIV_FSYS1, 24, 8,
			CLK_SET_RATE_PARENT, 0),
	CDIV(CLK_DIV_MMC1, "dout_mmc1", "mout_mmc1", DIV_FSYS1, 16, 4),

	CDIV_F(CLK_DIV_MMC0_PRE, "dout_mmc0_pre", "dout_mmc0", DIV_FSYS1, 8, 8,
			CLK_SET_RATE_PARENT, 0),
	CDIV(CLK_DIV_MMC0, "dout_mmc0", "mout_mmc0", DIV_FSYS1, 0, 4),

	CDIV(CLK_DIV_UART3, "dout_uart3", "mout_uart3",	DIV_PERIL0, 12, 4),
	CDIV(CLK_DIV_UART2, "dout_uart2", "mout_uart2",	DIV_PERIL0, 8, 4),
	CDIV(CLK_DIV_UART1, "dout_uart1", "mout_uart1",	DIV_PERIL0, 4, 4),
	CDIV(CLK_DIV_UART0, "dout_uart0", "mout_uart0",	DIV_PERIL0, 0, 4),

	CDIV(CLK_DIV_AUDIO, "dout_audio", "mout_audio", DIV_PERIL4, 16, 4),
	CDIV(CLK_DIV_I2S, "dout_i2s", "dout_audio", DIV_PERIL5, 8, 6),

	CDIV(CLK_DIV_MIPI0_PRE, "dout_mipi0_pre", "dout_mipi0",	DIV_LCD, 20, 4),
	CDIV(CLK_DIV_MIPI0, "dout_mipi0", "mout_mipi0", DIV_LCD, 16, 4),
	CDIV(CLK_DIV_FIMD0, "dout_fimd0", "mout_fimd0", DIV_LCD, 0, 4),
	CDIV(CLK_DIV_G3D, "dout_g3d", "mout_g3d", DIV_G3D, 0, 4),

	CDIV(CLK_DIV_MFC, "dout_mfc", "mout_mfc", DIV_MFC, 0, 4),

	CDIV(CLK_DIV_CAM1, "dout_cam1", "mout_cam1", DIV_CAM, 20, 4),
	CDIV(CLK_DIV_CAM_BLK, "dout_cam_blk", "mout_cam_blk", DIV_CAM, 0, 4),

	CDIV(CLK_DIV_SPI1_PRE, "dout_spi1_pre", "dout_spi1", DIV_PERIL1, 24, 8),
	CDIV(CLK_DIV_SPI1, "dout_spi1", "mout_spi1", DIV_PERIL1, 16, 4),
	CDIV(CLK_DIV_SPI0_PRE, "dout_spi0_pre", "dout_spi0", DIV_PERIL1, 8, 8),
	CDIV(CLK_DIV_SPI0, "dout_spi0", "mout_spi0", DIV_PERIL1, 0, 4),

	CDIV(CLK_DIV_UART_ISP, "dout_uart_isp", "mout_uart_isp",
			DIV_ISP, 28, 4),
	CDIV(CLK_DIV_SPI1_ISP_PRE, "dout_spi1_isp_pre", "dout_spi1_isp",
			DIV_ISP, 20, 8),
	CDIV(CLK_DIV_SPI1_ISP, "dout_spi1_isp", "mout_spi1_isp",
			DIV_ISP, 16, 4),
	CDIV(CLK_DIV_SPI0_ISP_PRE, "dout_spi0_isp_pre", "dout_spi0_isp",
			DIV_ISP, 8, 8),
	CDIV(CLK_DIV_SPI0_ISP, "dout_spi0_isp", "mout_spi0_isp",
			DIV_ISP, 4, 4),

	CDIV(CLK_DIV_ISP1, "dout_isp1", "mout_aclk_266_sub", DIV_ISP0, 4, 3),
	CDIV(CLK_DIV_ISP0, "dout_isp0", "mout_aclk_266_sub", DIV_ISP0, 0, 3),

	CDIV(CLK_DIV_MCUISP1, "dout_mcuisp1", "mout_aclk_400_mcuisp_sub",
			DIV_ISP1, 8, 3),
	CDIV(CLK_DIV_MCUISP0, "dout_mcuisp0", "mout_aclk_400_mcuisp_sub",
			DIV_ISP1, 4, 3),
	CDIV(CLK_DIV_MPWM, "dout_mpwm", "dout_isp1", DIV_ISP1, 0, 3),
};

#define CGATE(_id, cname, pname, o, b, f, gf) \
	GATE(_id, cname, pname, (unsigned long)o, b, f, gf)

static struct samsung_gate_clock exynos3250_gate_clks[] __initdata = {
	CGATE(CLK_SCLK_UART3, "sclk_uart3", "dout_uart3",
			GATE_SCLK_PERIL, 3, CLK_SET_RATE_PARENT, 0),
	CGATE(CLK_SCLK_UART2, "sclk_uart2", "dout_uart2",
			GATE_SCLK_PERIL, 2, CLK_SET_RATE_PARENT, 0),
	CGATE(CLK_SCLK_UART1, "sclk_uart1", "dout_uart1",
			GATE_SCLK_PERIL, 1, CLK_SET_RATE_PARENT, 0),
	CGATE(CLK_SCLK_UART0, "sclk_uart0", "dout_uart0",
			GATE_SCLK_PERIL, 0, CLK_SET_RATE_PARENT, 0),

	CGATE(CLK_SCLK_SPI1, "sclk_spi1", "dout_spi1_pre",
			GATE_SCLK_PERIL, 7, CLK_SET_RATE_PARENT, 0),
	CGATE(CLK_SCLK_SPI0, "sclk_spi0", "dout_spi0_pre",
			GATE_SCLK_PERIL, 6, CLK_SET_RATE_PARENT, 0),

	CGATE(CLK_TMU_APBIF, "tmu_apbif", "dout_aclk_100",
				GATE_IP_PERIR, 17, 0, 0),
	CGATE(CLK_SCLK_I2S, "sclk_i2s", "dout_i2s",
				GATE_SCLK_PERIL, 18, CLK_SET_RATE_PARENT, 0),
	CGATE(CLK_KEYIF, "keyif", "dout_aclk_100", GATE_IP_PERIR, 16, 0, 0),
	CGATE(CLK_RTC, "rtc", "dout_aclk_100", GATE_IP_PERIR, 15, 0, 0),
	CGATE(CLK_WDT, "wdt", "dout_aclk_100", GATE_IP_PERIR, 14, 0, 0),
	CGATE(CLK_MCT, "mct", "dout_aclk_100", GATE_IP_PERIR, 13, 0, 0),

	CGATE(CLK_UART3, "uart3", "dout_uart3", GATE_IP_PERIL, 3, 0, 0),
	CGATE(CLK_UART2, "uart2", "dout_uart2",	GATE_IP_PERIL, 2, 0, 0),
	CGATE(CLK_UART1, "uart1", "dout_uart1", GATE_IP_PERIL, 1, 0, 0),
	CGATE(CLK_UART0, "uart0", "dout_uart0", GATE_IP_PERIL, 0, 0, 0),

	CGATE(CLK_PWM, "pwm-clock", "dout_aclk_100", GATE_IP_PERIL, 24, 0, 0),
	CGATE(CLK_I2C7, "i2c7", "dout_aclk_100", GATE_IP_PERIL, 13, 0, 0),
	CGATE(CLK_I2C6, "i2c6", "dout_aclk_100", GATE_IP_PERIL, 12, 0, 0),
	CGATE(CLK_I2C5, "i2c5", "dout_aclk_100", GATE_IP_PERIL, 11, 0, 0),
	CGATE(CLK_I2C4, "i2c4", "dout_aclk_100", GATE_IP_PERIL, 10, 0, 0),
	CGATE(CLK_I2C3, "i2c3", "dout_aclk_100", GATE_IP_PERIL, 9, 0, 0),
	CGATE(CLK_I2C2, "i2c2", "dout_aclk_100", GATE_IP_PERIL, 8, 0, 0),
	CGATE(CLK_I2C1, "i2c1", "dout_aclk_100", GATE_IP_PERIL, 7, 0, 0),
	CGATE(CLK_I2C0, "i2c0", "dout_aclk_100", GATE_IP_PERIL, 6, 0, 0),
	CGATE(CLK_SPI1, "spi1", "dout_aclk_100", GATE_IP_PERIL, 17, 0, 0),
	CGATE(CLK_SPI0, "spi0", "dout_aclk_100", GATE_IP_PERIL, 16, 0, 0),
	CGATE(CLK_I2S, "iis", "dout_aclk_100",GATE_IP_PERIL, 21, 0, 0),

	CGATE(CLK_ACLK_FIMD0, "aclk_fimd0", NULL, GATE_BUS_LCD, 0, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_SCLK_MIPIDPHY2L, "sclk_mipidphy2l", "dout_mipi0",
			GATE_SCLK_LCD, 4, CLK_SET_RATE_PARENT, 0),
	CGATE(CLK_SCLK_MIPI0, "sclk_mipi0", "dout_mipi0_pre",
			GATE_SCLK_LCD, 3, CLK_SET_RATE_PARENT, 0),
	CGATE(CLK_SCLK_FIMD0, "sclk_fimd0", "dout_fimd0",
			GATE_SCLK_LCD, 0, CLK_SET_RATE_PARENT, 0),

	CGATE(CLK_SMMUFIMD0, "smmufimd0", "dout_aclk_160", GATE_IP_LCD, 4, 0, 0),
	CGATE(CLK_DSIM0, "dsim0", "dout_aclk_160", GATE_IP_LCD, 3, 0, 0),
	CGATE(CLK_SMIES, "smies", "dout_aclk_160", GATE_IP_LCD, 2, 0, 0),
	CGATE(CLK_FIMD0, "fimd0", "dout_aclk_160", GATE_IP_LCD, 0, 0, 0),


	CGATE(CLK_ACLK_MMC0, "aclk_mmc0", "dout_aclk_200",
			GATE_BUS_FSYS0, 5, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_SCLK_UPLL, "sclk_upll", "mout_upll",
			GATE_SCLK_FSYS, 10, 0, 0),
	CGATE(CLK_SCLK_TSADC, "sclk_tsadc", "dout_tsadc_pre",
			GATE_SCLK_FSYS, 9, CLK_SET_RATE_PARENT, 0),
	CGATE(CLK_SCLK_MMC2, "sclk_mmc2", "dout_mmc2_pre",
			GATE_SCLK_FSYS, 2, CLK_SET_RATE_PARENT, 0),
	CGATE(CLK_SCLK_MMC1, "sclk_mmc1", "dout_mmc1_pre",
			GATE_SCLK_FSYS, 1, CLK_SET_RATE_PARENT, 0),
	CGATE(CLK_SCLK_MMC0, "sclk_mmc0", "dout_mmc0_pre",
			GATE_SCLK_FSYS, 0, CLK_SET_RATE_PARENT, 0),

	CGATE(CLK_TSADC, "tsadc", "dout_aclk_200", GATE_IP_FSYS, 20, 0, 0),
	CGATE(CLK_USBOTG, "usbotg", "dout_aclk_200", GATE_IP_FSYS, 13, 0, 0),
	CGATE(CLK_USBHOST, "usbhost", "dout_aclk_200", GATE_IP_FSYS, 12, 0, 0),
	CGATE(CLK_SDMMC2, "sdmmc2", "dout_aclk_200", GATE_IP_FSYS, 7, 0, 0),
	CGATE(CLK_SDMMC1, "sdmmc1", "dout_aclk_200", GATE_IP_FSYS, 6, 0, 0),
	CGATE(CLK_SDMMC0, "sdmmc0", "dout_aclk_200", GATE_IP_FSYS, 5, 0, 0),
	CGATE(CLK_PDMA1, "pdma1", "dout_aclk_200",
			GATE_IP_FSYS, 1, 0, 0),
	CGATE(CLK_PDMA0, "pdma0", "dout_aclk_200",
			GATE_IP_FSYS, 0, 0, 0),

	CGATE(CLK_ASYNC_G3D, "async_g3d", "dout_aclk_100", GATE_IP_LEFTBUS, 6,
			0, 0),
	CGATE(CLK_ASYNC_MFCL, "async_mfcl", "dout_aclk_100", GATE_IP_LEFTBUS, 4,
			0, 0),

	CGATE(CLK_ASYNC_CAMX, "async_camx", "dout_aclk_100",
			GATE_IP_RIGHTBUS, 2, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_SCLK_MFC, "sclk_mfc", "dout_mfc",
			GATE_SCLK_MFC, 0, CLK_SET_RATE_PARENT, 0),
	CGATE(CLK_SCLK_G3D, "sclk_g3d", "dout_g3d",
			GATE_SCLK_G3D, 0, CLK_SET_RATE_PARENT, 0),

	CGATE(CLK_SCLK_JPEG, "sclk_jpeg", "dout_cam_blk",
			GATE_SCLK_CAM, 8, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_SCLK_CAM1, "sclk_cam1", "dout_cam1",
			GATE_SCLK_ISP, 4, CLK_SET_RATE_PARENT, 0),
	CGATE(CLK_SCLK_UART_ISP, "sclk_uart_isp", "dout_uart_isp",
			GATE_SCLK_ISP, 3, CLK_SET_RATE_PARENT, 0),
	CGATE(CLK_SCLK_SPI1_ISP, "sclk_spi1_isp", "dout_spi1_isp_pre",
			GATE_SCLK_ISP, 2, CLK_SET_RATE_PARENT, 0),
	CGATE(CLK_SCLK_SPI0_ISP, "sclk_spi0_isp", "dout_spi0_isp_pre",
			GATE_SCLK_ISP, 1, CLK_SET_RATE_PARENT, 0),

	CGATE(CLK_MASK_CAM1, "mask_cam1", "dout_cam1",
			SRC_MASK_CAM, 20, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_MASK_CAM_BLK, "mask_camblk", "dout_cam_blk",
			SRC_MASK_CAM, 0, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_QEG3D, "qeg3d", "dout_aclk_200", GATE_IP_G3D, 2,
			0, 0),
	CGATE(CLK_PPMUG3D, "ppmug3d", "dout_aclk_200", GATE_IP_G3D, 1,
			0, 0),
	CGATE(CLK_G3D, "g3d", "dout_aclk_200", GATE_IP_G3D, 0, 0, 0),

	CGATE(CLK_QEMFC, "qemfc", "dout_aclk_200", GATE_IP_MFC, 5,
			0, 0),
	CGATE(CLK_PPMUMFC_L, "ppmumfc_l", "dout_aclk_200", GATE_IP_MFC, 3,
			0, 0),
	CGATE(CLK_SMMUMFC_L, "smmumfc_l", "dout_aclk_200", GATE_IP_MFC, 1, 0, 0),
	CGATE(CLK_MFC, "mfc", "dout_aclk_200", GATE_IP_MFC, 0, 0, 0),

	CGATE(CLK_QEJPEG, "qejpeg", "div_cam_blk_320",
			GATE_IP_CAM, 19, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_PIXELASYNCM1, "pixelasyncm1", "div_cam_blk_320",
			GATE_IP_CAM, 18, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_PIXELASYNCM0, "pixelasyncm0", "div_cam_blk_320",
			GATE_IP_CAM, 17, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_PPMUCAMIF, "ppmucamif", "div_cam_blk_320",
			GATE_IP_CAM, 16, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_QEM2MSCALER, "qem2mscaler", "div_cam_blk_320",
			GATE_IP_CAM, 14, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_QESCALER1, "qescaler1", "div_cam_blk_320",
			GATE_IP_CAM, 13, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_QESCALER0, "qescaler0", "div_cam_blk_320",
			GATE_IP_CAM, 12, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_SMMUJPEG, "smmujpeg", "div_cam_blk_320",
			GATE_IP_CAM, 11, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_SMMUM2MSCALER, "smmum2mscaler", "div_cam_blk_320",
			GATE_IP_CAM, 9, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_SMMUGSCALER1, "smmugscaler1", "div_cam_blk_320",
			GATE_IP_CAM, 8, 0, 0),
	CGATE(CLK_SMMUGSCALER0, "smmugscaler0", "div_cam_blk_320",
			GATE_IP_CAM, 7, 0, 0),
	CGATE(CLK_JPEG, "jpeg", "div_cam_blk_320",
			GATE_IP_CAM, 6, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_M2MSCALER, "m2mscaler", "div_cam_blk_320",
			GATE_IP_CAM, 2, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_GSCALER1, "gscaler1", "div_cam_blk_320",
			GATE_IP_CAM, 1, 0, 0),
	CGATE(CLK_GSCALER0, "gscaler0", "div_cam_blk_320",
			GATE_IP_CAM, 0, 0, 0),

	CGATE(CLK_HCLK_AXI_DIV1, "hclk_axi_div1", "dout_isp1",
			GATE_BUS_ISP0, 24, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_ACLK_AXI_DIV0, "aclk_axi_div0", "dout_isp0",
			GATE_BUS_ISP0, 23, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_ACLK_MCUISP, "aclk_mcuisp", "mout_aclk_400_mcuisp_sub",
			GATE_BUS_ISP0, 6, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_ACLK_AXI_266, "aclk_axi266", "mout_aclk_266_sub",
			GATE_BUS_ISP0, 0, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_PCLK_I2C1ISP, "pclk_i2c1isp", "dout_aclk_100",
			GATE_BUS_ISP2, 26, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_PCLK_I2C0ISP, "pclk_i2c0isp", "dout_aclk_100",
			GATE_BUS_ISP2, 25, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_PCLKDBG_MCUISP, "pclkdbg_mcuisp", "dout_mcuisp1",
			GATE_BUS_ISP3, 3, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_SCLK_MPWM_ISP, "sclk_mpwm_isp", "dout_mpwm",
			GATE_IP_SCLK_ISP, 0, CLK_IGNORE_UNUSED, 0),

	CGATE(CLK_I2C1ISP, "i2c1isp", NULL,
			GATE_IP_ISP0, 26, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_I2C0ISP, "i2c0isp", NULL,
			GATE_IP_ISP0, 25, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_CSIS1, "csis1", NULL,
			GATE_IP_ISP0, 13, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_SMMULITE1, "smmulite1", NULL,
			GATE_IP_ISP0, 12, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_SMMULITE0, "smmulite0", NULL,
			GATE_IP_ISP0, 11, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_SMMUFD, "smmufd", NULL,
			GATE_IP_ISP0, 10, 0, 0),
	CGATE(CLK_SMMUDRC, "smmudrc", NULL,
			GATE_IP_ISP0, 9, 0, 0),
	CGATE(CLK_SMMUISP, "smmuisp", NULL,
			GATE_IP_ISP0, 8, 0, 0),
	CGATE(CLK_CSIS0, "csis0", NULL,
			GATE_IP_ISP0, 6, 0, 0),
	CGATE(CLK_MCUISP, "mcuisp", NULL,
			GATE_IP_ISP0, 5, 0, 0),
	CGATE(CLK_LITE1, "lite1", NULL,
			GATE_IP_ISP0, 4, 0, 0),
	CGATE(CLK_LITE0, "lite0", NULL,
			GATE_IP_ISP0, 3, 0, 0),

	CGATE(CLK_SMMUSCALERC, "smmuscalerc", NULL,
			GATE_IP_ISP1, 15, CLK_IGNORE_UNUSED, 0),
	CGATE(CLK_SMMUISPCX, "smmuispcx", NULL,
			GATE_IP_ISP1, 4, 0, 0),
};

static struct samsung_gate_clock exynos3250_unused_gate_clks[] __initdata = {
	CGATE(0, "ppmuacp", 	NULL,	GATE_IP_ACP0,	16, 0, 0),
	CGATE(0, "id_remapper",	NULL,	GATE_IP_ACP0,	13, 0, 0),
	CGATE(0, "wdt_isp",	NULL,	GATE_IP_ISP0,	30, 0, 0),
	CGATE(0, "ppmuispmx",	NULL,	GATE_IP_ISP0,	20, 0, 0),
	CGATE(0, "qe_lite1",	NULL,	GATE_IP_ISP0,	18, 0, 0),
	CGATE(0, "qe_lite0",	NULL,	GATE_IP_ISP0,	17, 0, 0),
	CGATE(0, "qe_fd",	NULL,	GATE_IP_ISP0,	16, 0, 0),
	CGATE(0, "qe_drc",	NULL,	GATE_IP_ISP0,	15, 0, 0),
	CGATE(0, "qe_isp",	NULL,	GATE_IP_ISP0,	14, 0, 0),
	CGATE(0, "qe_ispcx",	NULL,	GATE_IP_ISP1,	21, 0, 0),
	CGATE(0, "qe_scalerc",	NULL,	GATE_IP_ISP1,	19, 0, 0),
	CGATE(0, "spi1_isp",	NULL,	GATE_IP_ISP1,	13, 0, 0),
	CGATE(0, "ASYNCAXIM",	NULL,	GATE_IP_ISP1,	0, 0, 0),
	CGATE(0, "ip_spi1_isp",	NULL,	EXYNOS3_CLKGATE_IP_ISP,	2, 0, 0),
	CGATE(0, "monocnt",	NULL,	GATE_IP_PERIR, 22, 0, 0),
	CGATE(0, "provisionkey2",NULL,	GATE_IP_PERIR, 20, 0, 0),
	CGATE(0, "provisionkey1",NULL,	GATE_IP_PERIR, 19, 0, 0),
	CGATE(0, "qe_ch1_lcd",	NULL,	GATE_IP_LCD, 7, 0, 0),
	CGATE(0, "qe_ch0_lcd",	NULL,	GATE_IP_LCD, 6, 0, 0),
	CGATE(0, "ppmulcd0",	NULL,	GATE_IP_LCD, 5, 0, 0),
};

static struct samsung_pll_rate_table exynos3250_pll_rates[] = {
	PLL_35XX_RATE(1200000000, 400, 4, 1),
	PLL_35XX_RATE(1100000000, 275, 3, 1),
	PLL_35XX_RATE(1066000000, 533, 6, 1),
	PLL_35XX_RATE(1000000000, 250, 3, 1),
	PLL_35XX_RATE( 960000000, 320, 4, 1),
	PLL_35XX_RATE( 900000000, 300, 4, 1),
	PLL_35XX_RATE( 850000000, 425, 6, 1),
	PLL_35XX_RATE( 800000000, 200, 3, 1),
	PLL_35XX_RATE( 700000000, 175, 3, 1),
	PLL_35XX_RATE( 667000000, 667, 12, 1),
	PLL_35XX_RATE( 600000000, 400, 4, 2),
	PLL_35XX_RATE( 533000000, 533, 6, 2),
	PLL_35XX_RATE( 520000000, 260, 3, 2),
	PLL_35XX_RATE( 500000000, 250, 3, 2),
	PLL_35XX_RATE( 400000000, 200, 3, 2),
	PLL_35XX_RATE( 300000000, 400, 4, 3),
	PLL_35XX_RATE( 200000000, 200, 3, 3),
	PLL_35XX_RATE( 100000000, 200, 3, 4),
	{ /* sentinel */ }
};

static struct samsung_pll_rate_table exynos3250_vpll_rates[] = {
       PLL_36XX_RATE(600000000, 100, 2, 1,     0),
       PLL_36XX_RATE(533000000, 267, 3, 2, 32668),
       PLL_36XX_RATE(519231000, 173, 2, 2,  5046),
       PLL_36XX_RATE(500000000, 250, 3, 2,     0),
       PLL_36XX_RATE(445500000, 149, 2, 2, 32768),
       PLL_36XX_RATE(445055000, 148, 2, 2, 23047),
       PLL_36XX_RATE(400000000, 200, 3, 2,     0),
       PLL_36XX_RATE(371250000, 124, 2, 2, 49512),
       PLL_36XX_RATE(370879000, 185, 3, 2, 28803),
       PLL_36XX_RATE(340000000, 170, 3, 2,     0),
       PLL_36XX_RATE(335000000, 112, 2, 2, 43691),
       PLL_36XX_RATE(333000000, 111, 2, 2,     0),
       PLL_36XX_RATE(330000000, 110, 2, 2,     0),
       PLL_36XX_RATE(320000000, 107, 2, 2, 43691),
       PLL_36XX_RATE(300000000, 100, 2, 2,     0),
       PLL_36XX_RATE(275000000, 275, 3, 3,     0),
       PLL_36XX_RATE(266000000, 266, 3, 3,     0),
       PLL_36XX_RATE(222750000, 149, 2, 3, 32768),
       PLL_36XX_RATE(222528000, 148, 2, 3, 23069),
       PLL_36XX_RATE(160000000, 160, 3, 3,     0),
       PLL_36XX_RATE(148500000,  99, 2, 3,     0),
       PLL_36XX_RATE(148352000,  99, 2, 3, 59070),
       PLL_36XX_RATE(108000000, 144, 2, 4,     0),
       PLL_36XX_RATE(80000000, 160, 3, 4,     0),
       PLL_36XX_RATE( 74250000,  99, 2, 4,     0),
       PLL_36XX_RATE( 74176000,  99, 3, 4, 59070),
       PLL_36XX_RATE( 54054000, 216, 3, 5, 14156),
       PLL_36XX_RATE( 54000000, 144, 2, 5,     0),
       { /* sentinel */ }
};

static struct samsung_pll_rate_table exynos3250_epll_rates[] = {
       PLL_36XX_RATE(800000000, 200, 3, 1, 0),
       PLL_36XX_RATE(288000000, 96, 2, 2, 0),
       PLL_36XX_RATE(192000000, 128, 2, 3, 0),
       PLL_36XX_RATE(144000000, 96, 2, 3, 0),
       PLL_36XX_RATE(96000000, 128, 2, 4, 0),
       PLL_36XX_RATE(84000000, 112, 2, 4, 0),
       PLL_36XX_RATE(80000000, 160, 3, 4, 0),
       PLL_36XX_RATE(73728000, 98, 2, 4, 19923),
       PLL_36XX_RATE(67737600, 452, 5, 5, -27263),
       PLL_36XX_RATE(65536000, 175, 2, 5, 49982),
       PLL_36XX_RATE(50000000, 200, 3, 5, 0),
       PLL_36XX_RATE(49152000, 197, 3, 5, -25690),
       PLL_36XX_RATE(48000000, 128, 2, 5, 0),
       PLL_36XX_RATE(45158400, 301, 5, 5, 3670),
       { /* sentinel */ }
};

static void __init exynos3_core_down_clock(void)
{
        unsigned int tmp;

        /*
         * Enable arm clock down (in idle) and set arm divider
         * ratios in WFI/WFE state.
         */
        tmp = (PWR_CTRL1_CORE2_DOWN_RATIO(7) | PWR_CTRL1_CORE1_DOWN_RATIO(7) |
                PWR_CTRL1_DIV2_DOWN_EN | PWR_CTRL1_DIV1_DOWN_EN |
                PWR_CTRL1_USE_CORE1_WFE | PWR_CTRL1_USE_CORE0_WFE |
                PWR_CTRL1_USE_CORE1_WFI | PWR_CTRL1_USE_CORE0_WFI);
        __raw_writel(tmp, PWR_CTRL1);

        /*
         * Disable the clock up feature on Exynos4x12, in case it was
         * enabled by bootloader.
         */
        __raw_writel(0x0, PWR_CTRL2);
}

void __init exynos3250_clk_init(struct device_node *np)
{
	struct clk *apll, *bpll, *epll, *mpll, *vpll, *upll;

	samsung_clk_init(np, 0, NR_CLKS, (unsigned long *)exynos3250_clk_regs,
			ARRAY_SIZE(exynos3250_clk_regs), NULL, 0);

	apll = samsung_clk_register_pll35xx("fout_apll", "fin_pll",
		APLL_LOCK, APLL_CON0, exynos3250_pll_rates,
		sizeof(exynos3250_pll_rates));

	clk_register_clkdev(apll, "fout_apll", NULL);

	bpll = samsung_clk_register_pll35xx("fout_bpll", "fin_pll",
		BPLL_LOCK, BPLL_CON0, exynos3250_pll_rates,
		sizeof(exynos3250_pll_rates));

	samsung_clk_add_lookup(bpll, CLK_FOUT_BPLL);

	epll = samsung_clk_register_pll36xx("fout_epll", "fin_pll",
		EPLL_LOCK, EPLL_CON0, exynos3250_epll_rates,
		sizeof(exynos3250_pll_rates));

	samsung_clk_add_lookup(epll, CLK_FOUT_EPLL);

	mpll = samsung_clk_register_pll35xx("fout_mpll", "fin_pll",
		MPLL_LOCK, MPLL_CON0, exynos3250_pll_rates,
		sizeof(exynos3250_pll_rates));

	samsung_clk_add_lookup(mpll, CLK_FOUT_MPLL);

	vpll = samsung_clk_register_pll36xx("fout_vpll", "mout_vpllsrc",
		VPLL_LOCK, VPLL_CON0, exynos3250_vpll_rates,
		sizeof(exynos3250_vpll_rates));

	samsung_clk_add_lookup(vpll, CLK_FOUT_VPLL);

	upll = samsung_clk_register_pll35xx("fout_upll", "fin_pll",
		UPLL_LOCK, UPLL_CON0, exynos3250_pll_rates,
		sizeof(exynos3250_pll_rates));

	samsung_clk_add_lookup(upll, CLK_FOUT_UPLL);

	samsung_clk_register_fixed_factor(fixed_factor_clks,
			ARRAY_SIZE(fixed_factor_clks));
	samsung_clk_register_mux(exynos3250_mux_clks,
			ARRAY_SIZE(exynos3250_mux_clks));
	samsung_clk_register_div(exynos3250_div_clks,
			ARRAY_SIZE(exynos3250_div_clks));
	samsung_clk_register_gate(exynos3250_gate_clks,
			ARRAY_SIZE(exynos3250_gate_clks));
	samsung_clk_register_gate(exynos3250_unused_gate_clks,
			ARRAY_SIZE(exynos3250_unused_gate_clks));

	exynos3_core_down_clock();

	pr_info("Exynos3250: clock setup completed\n");
}

CLK_OF_DECLARE(exynos3250_clk, "samsung,exynos3250-clock", exynos3250_clk_init);
