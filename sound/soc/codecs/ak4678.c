/*
 * ak4678.c  --  audio driver for ak4678
 *
 * Copyright (C) 2012 Asahi Kasei Microdevices Corporation
 *
 * Modified by InSignal Co.,LTD.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <linux/of_gpio.h>

#include "ak4678.h"

/* ghcstop_caution: not use pdn pin */
#define GHC_USE_PDN	1
//#undef GHC_USE_PDN

struct ak4678seq_t {
	u16 addr;
	u16 mask;
	u16 data;
};

#undef AK4678_DEBUG

#ifdef AK4678_DEBUG
#define gprintk(fmt, x... ) printk( "%s: " fmt, __FUNCTION__ , ## x)
#else
#define gprintk(x...) do { } while (0)
#endif


#define AK4678_PORTIIS 0
#define AK4678_PORTA 1
#define AK4678_PORTB 2

#define AK4678_PDNA 172

static struct ak4678seq_t akseq_stereoef_on[] = {
	{AK4678_1A_FILTER_SELECT1, 0xC, 0xC},
};

static struct ak4678seq_t akseq_stereoef_off[] = {
	{AK4678_1A_FILTER_SELECT1, 0xC, 0x0},
};

static struct ak4678seq_t akseq_dvlc_drc_on[] = {
	{AK4678_70_DRC_MODE_CONTROL, 0x3, 0x3},
	{AK4678_71_NS_CONTROL, 0x6, 0x6},
	{AK4678_80_DVLC_FILTER_SELECT, 0x55, 0x55}
};

static struct ak4678seq_t akseq_dvlc_drc_off[] = {
	{AK4678_70_DRC_MODE_CONTROL, 0x3, 0x0},
	{AK4678_71_NS_CONTROL, 0xff, 0x0},
	{AK4678_80_DVLC_FILTER_SELECT, 0xff, 0x0}
};

/* AK4678 Codec Private Data */
struct ak4678_priv {
	u8 reg_cache[AK4678_MAX_REGISTERS];
	u16 ext_clk_mode;
	u16 stereo_ef_on;	/* Stereo Enpahsis Filter ON */
	u16 dvlc_drc_on;	/* DVLC and DRC ON */
	u16 fsno;		/* fs  0 : fs <= 12kHz,  1: 12kHz < fs <= 24kHz, 2: fs > 24kHz */
	u16 pllmode;
	int pdn;
	bool suspended;
};

static struct snd_soc_codec *ak4678_codec;

/* ak4678 register cache & default register settings */
static const u8 ak4678_reg[AK4678_MAX_REGISTERS] = {
	0x00,			/*      0x00    AK4678_00_POWER_MANAGEMENT0     */
	0x00,			/*      0x01    AK4678_01_POWER_MANAGEMENT1     */
	0x00,			/*      0x02    AK4678_02_POWER_MANAGEMENT2     */
	0xF6,			/*      0x03    AK4678_03_PLL_MODE_SELECT0      */
	0x00,			/*      0x04    AK4678_04_PLL_MODE_SELECT1      */
	0x02,			/*      0x05    AK4678_05_FORMAT_SELECT */
	0x00,			/*      0x06    AK4678_06_MIC_SIGNAL_SELECT     */
	0x55,			/*      0x07    AK4678_07_MIC_AMP_GAIN          */
	0x00,			/*      0x08    AK4678_08_DIGITL_MIC    */
	0x00,			/*      0x09    AK4678_09_DAC_PATH_SELECT       */
	0x00,			/*      0x0A    AK4678_0A_LINE_MANAGEMENT       */
	0x00,			/*      0x0B    AK4678_0B_HP_MANAGEMENT */
	0x50,			/*      0x0C    AK4678_0C_CP_CONTROL    */
	0x00,			/*      0x0D    AK4678_0D_SPK_MANAGEMENT        */
	0x03,			/*      0x0E    AK4678_0E_LINEOUT_VOLUME        */
	0x19,			/*      0x0F    AK4678_0F_HP_VOLUME     */
	0xBB,			/*      0x10    AK4678_10_SPRC_VOLUME   */
	0x91,			/*      0x11    AK4678_11_LIN_VOLUME    */
	0x91,			/*      0x12    AK4678_12_RIN_VOLUME    */
	0xC9,			/*      0x13    AK4678_13_ALCREF_SELECT */
	0x00,			/*      0x14    AK4678_14_DIGMIX_CONTROL        */
	0x10,			/*      0x15    AK4678_15_ALCTIMER_SELECT       */
	0x01,			/*      0x16    AK4678_16_ALCMODE_CONTROL       */
	0x02,			/*      0x17    AK4678_17_MODE0_CONTROL */
	0x43,			/*      0x18    AK4678_18_MODE1_CONTROL */
	0x12,			/*      0x19    AK4678_19_FILTER_SELECT0        */
	0x00,			/*      0x1A    AK4678_1A_FILTER_SELECT1        */
	0x00,			/*      0x1B    AK4678_1B_FILTER_SELECT2        */
	0x00,			/*      0x1C    AK4678_1C_SIDETONE_VOLUMEA      */
	0x0C,			/*      0x1D    AK4678_1D_LOUT_VOLUME   */
	0x0C,			/*      0x1E    AK4678_1E_ROUT_VOLUME   */
	0x00,			/*      0x1F    AK4678_1F_PCM_IF_MANAGEMENT     */
	0x00,			/*      0x20    AK4678_20_PCM_IF_CONTROL0       */
	0x00,			/*      0x21    AK4678_21_PCM_IF_CONTROL1       */
	0x00,			/*      0x22    AK4678_22_SIDETONE_VOLUMEB      */
	0x0C,			/*      0x23    AK4678_23_DVOLB_CONTROL */
	0x0C,			/*      0x24    AK4678_24_DVOLC_CONTROL */
	0x00,			/*      0x25    AK4678_25_DMIX_CONTROL0 */
	0x00,			/*      0x26    AK4678_26_DMIX_CONTROL1 */
	0x00,			/*      0x27    AK4678_27_DMIX_CONTROL2 */
	0x00,			/*      0x28    AK4678_28_DMIX_CONTROL3 */
	0x71,			/*      0x29    AK4678_29_FIL1_COEFFICIENT0     */
	0x1F,			/*      0x2A    AK4678_2A_FIL1_COEFFICIENT1     */
	0x1F,			/*      0x2B    AK4678_2B_FIL1_COEFFICIENT2     */
	0x21,			/*      0x2C    AK4678_2C_FIL1_COEFFICIENT3     */
	0x7F,			/*      0x2D    AK4678_2D_FIL2_COEFFICIENT0     */
	0x0C,			/*      0x2E    AK4678_2E_FIL2_COEFFICIENT1     */
	0xFF,			/*      0x2F    AK4678_2F_FIL2_COEFFICIENT2     */
	0x38,			/*      0x30    AK4678_30_FIL2_COEFFICIENT3     */
	0xA2,			/*      0x31    AK4678_31_FIL3_COEFFICIENT0     */
	0x83,			/*      0x32    AK4678_32_FIL3_COEFFICIENT1     */
	0x80,			/*      0x33    AK4678_33_FIL3_COEFFICIENT2     */
	0x2E,			/*      0x34    AK4678_34_FIL3_COEFFICIENT3     */
	0x5B,			/*      0x35    AK4678_35_EQ_COEFFICIENT0       */
	0x23,			/*      0x36    AK4678_36_EQ_COEFFICIENT1       */
	0x07,			/*      0x37    AK4678_37_EQ_COEFFICIENT2       */
	0x28,			/*      0x38    AK4678_38_EQ_COEFFICIENT3       */
	0xAA,			/*      0x39    AK4678_39_EQ_COEFFICIENT4       */
	0xEC,			/*      0x3A    AK4678_3A_EQ_COEFFICIENT5       */
	0x00,			/*      0x3B    AK4678_3B_E1_COEFFICIENT0       */
	0x00,			/*      0x3C    AK4678_3C_E1_COEFFICIENT1       */
	0x21,			/*      0x3D    AK4678_3D_E1_COEFFICIENT2       */
	0x35,			/*      0x3E    AK4678_3E_E1_COEFFICIENT3       */
	0xE6,			/*      0x3F    AK4678_3F_E1_COEFFICIENT4       */
	0xE0,			/*      0x40    AK4678_40_E1_COEFFICIENT5       */
	0x00,			/*      0x41    AK4678_41_E2_COEFFICIENT0       */
	0x00,			/*      0x42    AK4678_42_E2_COEFFICIENT1       */
	0xC1,			/*      0x43    AK4678_43_E2_COEFFICIENT2       */
	0x2F,			/*      0x44    AK4678_44_E2_COEFFICIENT3       */
	0xE6,			/*      0x45    AK4678_45_E2_COEFFICIENT4       */
	0xE0,			/*      0x46    AK4678_46_E2_COEFFICIENT5       */
	0x00,			/*      0x47    AK4678_47_E3_COEFFICIENT0       */
	0x00,			/*      0x48    AK4678_48_E3_COEFFICIENT1       */
	0x3C,			/*      0x49    AK4678_49_E3_COEFFICIENT2       */
	0x22,			/*      0x4A    AK4678_4A_E3_COEFFICIENT3       */
	0xE6,			/*      0x4B    AK4678_4B_E3_COEFFICIENT4       */
	0xE0,			/*      0x4C    AK4678_4C_E3_COEFFICIENT5       */
	0x00,			/*      0x4D    Not be used     */
	0x00,			/*      0x4E    Not be used     */
	0x00,			/*      0x4F    Not be used     */
	0x48,			/*      0x50    AK4678_50_5BAND_E1_COEF0        */
	0x00,			/*      0x51    AK4678_51_5BAND_E1_COEF1        */
	0x91,			/*      0x52    AK4678_52_5BAND_E1_COEF2        */
	0xE0,			/*      0x53    AK4678_53_5BAND_E1_COEF3        */
	0xC9,			/*      0x54    AK4678_54_5BAND_E2_COEF0        */
	0x00,			/*      0x55    AK4678_55_5BAND_E2_COEF1        */
	0x8C,			/*      0x56    AK4678_56_5BAND_E2_COEF2        */
	0x3D,			/*      0x57    AK4678_57_5BAND_E2_COEF3        */
	0x6A,			/*      0x58    AK4678_58_5BAND_E2_COEF4        */
	0xE2,			/*      0x59    AK4678_59_5BAND_E2_COEF5        */
	0x9C,			/*      0x5A    AK4678_5A_5BAND_E3_COEF0        */
	0x02,			/*      0x5B    AK4678_5B_5BAND_E3_COEF1        */
	0x67,			/*      0x5C    AK4678_5C_5BAND_E3_COEF2        */
	0x37,			/*      0x5D    AK4678_5D_5BAND_E3_COEF3        */
	0x07,			/*      0x5E    AK4678_5E_5BAND_E3_COEF4        */
	0xE8,			/*      0x5F    AK4678_5F_5BAND_E3_COEF5        */
	0x20,			/*      0x60    AK4678_60_5BAND_E4_COEF0        */
	0x08,			/*      0x61    AK4678_61_5BAND_E4_COEF1        */
	0xD7,			/*      0x62    AK4678_62_5BAND_E4_COEF2        */
	0x20,			/*      0x63    AK4678_63_5BAND_E4_COEF3        */
	0xFF,			/*      0x64    AK4678_64_5BAND_E4_COEF4        */
	0xF8,			/*      0x65    AK4678_65_5BAND_E4_COEF5        */
	0x1B,			/*      0x66    AK4678_66_5BAND_E5_COEF0        */
	0x14,			/*      0x67    AK4678_67_5BAND_E5_COEF1        */
	0xCB,			/*      0x68    AK4678_68_5BAND_E5_COEF2        */
	0xF7,			/*      0x69    AK4678_69_5BAND_E5_COEF3        */
	0x18,			/*      0x6A    AK4678_6A_5BAND_E1_GAIN */
	0x18,			/*      0x6B    AK4678_6B_5BAND_E2_GAIN */
	0x18,			/*      0x6C    AK4678_6C_5BAND_E3_GAIN */
	0x18,			/*      0x6D    AK4678_6D_5BAND_E4_GAIN */
	0x18,			/*      0x6E    AK4678_6R_5BAND_E5_GAIN */
	0x00,			/*      0x6F    AK4678_6FH_RESERVED     */
	0x00,			/*      0x70    AK4678_70_DRC_MODE_CONTROL      */
	0x06,			/*      0x71    AK4678_71_NS_CONTROL    */
	0x11,			/*      0x72    AK4678_72_NS_GAIN_AND_ATT_CONTROL       */
	0x90,			/*      0x73    AK4678_73_NS_ON_LEVEL   */
	0x8A,			/*      0x74    AK4678_74_NS_OFF_LEVEL  */
	0x07,			/*      0x75    AK4678_75_NS_REFERENCE_SELECT   */
	0x40,			/*      0x76    AK4678_76_NS_LPF_CO_EFFICIENT_0 */
	0x07,			/*      0x77    AK4678_77_NS_LPF_CO_EFFICIENT_1 */
	0x80,			/*      0x78    AK4678_78_NS_LPF_CO_EFFICIENT_2 */
	0x2E,			/*      0x79    AK4678_79_NS_LPF_CO_EFFICIENT_3 */
	0xA9,			/*      0x7A    AK4678_7A_NS_HPF_CO_EFFICIENT_0 */
	0x1F,			/*      0x7B    AK4678_7B_NS_HPF_CO_EFFICIENT_1 */
	0xAD,			/*      0x7C    AK4678_7C_NS_HPF_CO_EFFICIENT_2 */
	0x20,			/*      0x7D    AK4678_7D_NS_HPF_CO_EFFICIENT_3 */
	0x00,			/*      0x7E    AK4678_7EH_RESERVED     */
	0x00,			/*      0x7F    AK4678_7FH_RESERVED     */
	0x00,			/*      0x80    AK4678_80_DVLC_FILTER_SELECT    */
	0x6F,			/*      0x81    AK4678_81_DVLC_MODE_CONTROL     */
	0x18,			/*      0x82    AK4678_82_DVLCL_CURVE_X1        */
	0x0C,			/*      0x83    AK4678_83_DVLCL_CURVE_Y1        */
	0x10,			/*      0x84    AK4678_84_DVLCL_CURVE_X2        */
	0x09,			/*      0x85    AK4678_85_DVLCL_CURVE_Y2        */
	0x08,			/*      0x86    AK4678_86_DVLCL_CURVE_X3        */
	0x08,			/*      0x87    AK4678_87_DVLCL_CURVE_Y3        */
	0x7F,			/*      0x88    AK4678_88_DVLCL_SLOPE_1 */
	0x1D,			/*      0x89    AK4678_89_DVLCL_SLOPE_2 */
	0x03,			/*      0x8A    AK4678_8A_DVLCL_SLOPE_3 */
	0x00,			/*      0x8B    AK4678_8B_DVLCL_SLOPE_4 */
	0x18,			/*      0x8C    AK4678_8C_DVLCM_CURVE_X1        */
	0x0C,			/*      0x8D    AK4678_8D_DVLCM_CURVE_Y1        */
	0x10,			/*      0x8E    AK4678_8E_DVLCM_CURVE_X2        */
	0x06,			/*      0x8F    AK4678_8F_DVLCM_CURVE_Y2        */
	0x08,			/*      0x90    AK4678_90_DVLCM_CURVE_X3        */
	0x04,			/*      0x91    AK4678_91_DVLCM_CURVE_Y3        */
	0x7F,			/*      0x92    AK4678_92_DVLCM_SLOPE_1 */
	0x4E,			/*      0x93    AK4678_93_DVLCM_SLOPE_2 */
	0x0C,			/*      0x94    AK4678_94_DVLCM_SLOPE_3 */
	0x00,			/*      0x95    AK4678_95_DVLCM_SLOPE_4 */
	0x1C,			/*      0x96    AK4678_96_DVLCH_CURVE_X1        */
	0x10,			/*      0x97    AK4678_97_DVLCH_CURVE_Y1        */
	0x10,			/*      0x98    AK4678_98_DVLCH_CURVE_X2        */
	0x0C,			/*      0x99    AK4678_99_DVLCH_CURVE_Y2        */
	0x08,			/*      0x9A    AK4678_9A_DVLCH_CURVE_X3        */
	0x09,			/*      0x9B    AK4678_9B_DVLCH_CURVE_Y3        */
	0x7F,			/*      0x9C    AK4678_9C_DVLCH_SLOPE_1 */
	0x12,			/*      0x9D    AK4678_9D_DVLCH_SLOPE_2 */
	0x07,			/*      0x9E    AK4678_9E_DVLCH_SLOPE_3 */
	0x01,			/*      0x9F    AK4678_9F_DVLCH_SLOPE_4 */
	0xAB,			/*      0xA0    AK4678_A0_DVLCL_LPF_CO_EFFICIENT_0      */
	0x00,			/*      0xA1    AK4678_A1_DVLCL_LPF_CO_EFFICIENT_1      */
	0x57,			/*      0xA2    AK4678_A2_DVLCL_LPF_CO_EFFICIENT_2      */
	0x21,			/*      0xA3    AK4678_A3_DVLCL_LPF_CO_EFFICIENT_3      */
	0x55,			/*      0xA4    AK4678_A4_DVLCM_HPF_CO_EFFICIENT_0      */
	0x1F,			/*      0xA5    AK4678_A5_DVLCM_HPF_CO_EFFICIENT_1      */
	0x57,			/*      0xA6    AK4678_A6_DVLCM_HPF_CO_EFFICIENT_2      */
	0x21,			/*      0xA7    AK4678_A7_DVLCM_HPF_CO_EFFICIENT_3      */
	0xB5,			/*      0xA8    AK4678_A8_DVLCM_LPF_CO_EFFICIENT_0      */
	0x05,			/*      0xA9    AK4678_A9_DVLCM_LPF_CO_EFFICIENT_1      */
	0x6A,			/*      0xAA    AK4678_AA_DVLCM_LPF_CO_EFFICIENT_2      */
	0x2B,			/*      0xAB    AK4678_AB_DVLCM_LPF_CO_EFFICIENT_3      */
	0x4B,			/*      0xAC    AK4678_AC_DVLCH_HPF_CO_EFFICIENT_0      */
	0x1A,			/*      0xAD    AK4678_AD_DVLCH_HPF_CO_EFFICIENT_1      */
	0x6A,			/*      0xAE    AK4678_AE_DVLCH_HPF_CO_EFFICIENT_2      */
	0x2B			/*      0xAF    AK4678_AF_DVLCH_HPF_CO_EFFICIENT_3      */
};

#define AK4678_FS_NUM     3
#define AK4678_FS_LOW     12000
#define AK4678_FS_MIDDLE  24000

#define AK4678__NUM     3

static u8 hpf2[AK4678_FS_NUM][4] = {
	{0xDE, 0x1D, 0x43, 0x24},
	{0xE6, 0x1E, 0x34, 0x22},
	{0x71, 0x1F, 0x1F, 0x21}
};

static u8 fil3band[AK4678_FS_NUM][16] = {
	{0x87, 0x02, 0x0D, 0x25, 0x79, 0x1D, 0x0D, 0x25,
	 0x1D, 0x11, 0x3A, 0x02, 0xE3, 0x0E, 0x3A, 0x02},
	{0x50, 0x01, 0xA0, 0x22, 0xB0, 0x1E, 0xA0, 0x22,
	 0x04, 0x0A, 0x07, 0x34, 0xFC, 0x15, 0x07, 0x34},
	{0xAB, 0x00, 0x57, 0x21, 0x55, 0x1F, 0x57, 0x21,
	 0xB5, 0x05, 0x6A, 0x2B, 0x4B, 0x1A, 0x6A, 0x2B},
};

static u8 fil2ns[AK4678_FS_NUM][8] = {
	{0xEC, 0x15, 0xD7, 0x0B, 0xB0, 0x1E, 0xA0, 0x22},
	{0x7F, 0x0C, 0xFF, 0x38, 0x55, 0x1F, 0x57, 0x21},
	{0x40, 0x07, 0x80, 0x2E, 0xA9, 0x1F, 0xAD, 0x20},
};

/*
 * Read ak4678 register cache
 */
static inline u32 ak4678_read_reg_cache(struct snd_soc_codec *codec, u32 reg)
{
	u8 *cache = codec->reg_cache;
	BUG_ON(reg > ARRAY_SIZE(ak4678_reg));
	return (u32) cache[reg];
}

/*
 * Write ak4678 register cache
 */
static inline void ak4678_write_reg_cache(struct snd_soc_codec *codec, u32 reg,
					  u32 value)
{
	u8 *cache = codec->reg_cache;
	BUG_ON(reg > ARRAY_SIZE(ak4678_reg));
	cache[reg] = (u8) value;
}

/*
 * Write to ak4678 register space
 */
static int ak4678_write(struct snd_soc_codec *codec, unsigned int reg,
			unsigned int value)
{
	u8 data[2];
	BUG_ON(reg > ARRAY_SIZE(ak4678_reg));

	if (value == ak4678_read_reg_cache(codec, reg)) {
		gprintk("write:: Reg already having same value\n");
		return 0;
	}

	data[0] = reg;
	data[1] = value;

	if (codec->hw_write(codec->control_data, data, 2) == 2) {
		gprintk("ak4678_write reg(%x) val(%x)\n", reg, (u8) value);
		ak4678_write_reg_cache(codec, reg, value);
		return 0;
	} else {
		return -EIO;
	}
}

/*
 * Write with Mask to  AK4678 register space
 */
static int ak4678_masked_write(struct snd_soc_codec *codec, u32 reg, u32 mask,
			       u32 value)
{
	u32 olddata;
	u32 newdata;
	int ret = 0;

	if ((mask == 0) || (mask == 0xFF)) {
		newdata = value;
	} else {
		olddata = ak4678_read_reg_cache(codec, reg);
		newdata = (olddata & ~(mask)) | value;
	}

	ret = ak4678_write(codec, reg, newdata);

	gprintk("(addr,data)=(%x, %x)\n", reg, newdata);

	return ret;
}

/*
 *  AK4678's sequnce is executed.
 */
static int ak4678_sequece_exec(struct snd_soc_codec *codec,
			       struct ak4678seq_t *akseq, u32 seqcount)
{
	u32 i;

	gprintk("%d\n", seqcount);
	for (i = 0; i < seqcount; i++) {
		ak4678_masked_write(codec, akseq[i].addr, akseq[i].mask,
				    akseq[i].data);
	}

	return (0);
}

static int get_stereo_ef(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ak4678_priv *ak4678 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak4678->stereo_ef_on;

	return 0;

}

static int set_stereo_ef(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ak4678_priv *ak4678 = snd_soc_codec_get_drvdata(codec);

	ak4678->stereo_ef_on = ucontrol->value.enumerated.item[0];

	if (ak4678->stereo_ef_on)
		ak4678_sequece_exec(codec, akseq_stereoef_on,
				    ARRAY_SIZE(akseq_stereoef_on));
	else
		ak4678_sequece_exec(codec, akseq_stereoef_off,
				    ARRAY_SIZE(akseq_stereoef_off));

	return 0;
}

static int get_dvlc_drc(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ak4678_priv *ak4678 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak4678->dvlc_drc_on;

	return 0;
}

static int set_dvlc_drc(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ak4678_priv *ak4678 = snd_soc_codec_get_drvdata(codec);

	ak4678->dvlc_drc_on = ucontrol->value.enumerated.item[0];
	if (ak4678->dvlc_drc_on)
		ak4678_sequece_exec(codec, akseq_dvlc_drc_on,
				    ARRAY_SIZE(akseq_dvlc_drc_on));
	else
		ak4678_sequece_exec(codec, akseq_dvlc_drc_off,
				    ARRAY_SIZE(akseq_dvlc_drc_off));

	return 0;
}

static int ak4678_set_port_dai_fmt(struct snd_soc_codec *codec, unsigned int id,
				   unsigned int fmt)
{
	u8 format = 0;
	u8 reg;

	gprintk("\n");

	switch (id) {
	case AK4678_PORTA:
		reg = AK4678_20_PCM_IF_CONTROL0;
		break;
	case AK4678_PORTB:
		reg = AK4678_21_PCM_IF_CONTROL1;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		gprintk("Slave\n");
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
	default:
		dev_err(codec->dev, "Master mode unsupported");
		return -EINVAL;
	}

	format = snd_soc_read(codec, reg);
	format &= ~(AK4678_DIF | 0x30);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		format |= AK4678_DIF_I2S_MODE;
		break;

	case SND_SOC_DAIFMT_LEFT_J:
		format |= AK4678_DIF_MSB_MODE;
		break;

	case SND_SOC_DAIFMT_DSP_A:
		format |= AK4678_DIF_DSP_MODE;	//short frame
		format |= 0x10;	//BCKPA :1 , MSBSA :0
		if ((fmt & SND_SOC_DAIFMT_INV_MASK) == SND_SOC_DAIFMT_IB_NF)
			format &= ~0x10;
		break;

	case SND_SOC_DAIFMT_DSP_B:
		format |= 0x1;	// long frame
		format |= 0x10;	//BCKPA :1 , MSBSA :0
		if ((fmt & SND_SOC_DAIFMT_INV_MASK) == SND_SOC_DAIFMT_IB_NF)
			format &= ~0x10;
		break;

	default:
		return -EINVAL;
	}

	/* set mode and format */
	snd_soc_write(codec, reg, format);

	return 0;
}

static const char *stereo_ef_on[] = {
	"Stereo Enphasis Filter OFF",
	"Stereo Enphasis Filter ON",
};

static const char *dvlc_on[] = {
	"DVLC_DRC OFF",
	"DVLC_DRC ON",
};

static const struct soc_enum ak4678_enum[] = {
	/* Note: Dont change the order, then you need to change
	 * in mixer control also */
	/* arg1: number of elements arg2: elements name array */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(stereo_ef_on), stereo_ef_on),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dvlc_on), dvlc_on),
};

/*
 * Input Digital volume control:
 * from -54.375 to 36 dB in 0.375 dB steps mute instead of -54.375 dB)
 */
static DECLARE_TLV_DB_SCALE(ivol_tlv, -5437, 37, 0);

/*
 * Line output volume control:
 * from -9 to 6 dB in 3 dB steps
 */
static DECLARE_TLV_DB_SCALE(lineout_tlv, -900, 300, 0);

/*
 * Headphone output volume control:
 * from -70 to 6 dB in 2 dB steps ( -64dB,-66dB,-68dB and -70dB are mute)
 */
static DECLARE_TLV_DB_SCALE(hpout_tlv, -7000, 200, 1);

/*
 * Speaker output volume control:
 * from -33 to 12 dB in 3 dB steps (mute instead of -33 dB)
 */
static DECLARE_TLV_DB_SCALE(spkout_tlv, -3300, 300, 1);

/*
 * Reciever output volume control:
 * from -33 to 12 dB in 3 dB steps (mute instead of -33 dB)
 */
static DECLARE_TLV_DB_SCALE(rcvout_tlv, -3300, 300, 1);

/*
 * Output Digital volume control: (DATT-A)
 * (This can be used as Bluetooth I/F output volume)
 * from -57.5 to 6 dB in 0.5 dB steps (mute instead of -57.5 dB)
 */
static DECLARE_TLV_DB_SCALE(datta_tlv, -5750, 50, 1);

/*
 * PCM IF A input volume control: (DATT-B for SDTIA)
 * from -57.5 to 6 dB in 0.5 dB steps (mute instead of -57.5 dB)
 */
static DECLARE_TLV_DB_SCALE(dattb_tlv, -5750, 50, 1);

/*
 * PCM IF A input to PCM IF B Output  volume control: (DATT-C for SDTIA to SDTIOB)
 * from -57.5 to 6 dB in 0.5 dB steps (mute instead of -57.5 dB)
 */
static DECLARE_TLV_DB_SCALE(dattc_tlv, -5750, 50, 1);

/*
 * PCM IF B input volume control: (BIVOL for SDTIB)
 * from -24 to 0 dB in 6 dB steps
 */
static DECLARE_TLV_DB_SCALE(bivol_tlv, -2400, 600, 1);

/*
 * 5 Band EQ Gain control
 * from -12 to 12 dB in 6 dB steps 0.5
 */
static DECLARE_TLV_DB_SCALE(eqgain_tlv, -1200, 50, 1);

/*
 * SVOLA Gain control
 * from -24 to 0 dB steps 6
 */
static DECLARE_TLV_DB_SCALE(svolal_tlv, -2400, 50, 1);
static DECLARE_TLV_DB_SCALE(svolar_tlv, -2400, 50, 1);
static DECLARE_TLV_DB_SCALE(svolb_tlv, -2400, 50, 1);

static const struct snd_kcontrol_new ak4678_snd_controls[] = {
	SOC_SINGLE_TLV("Input Digital Volume",
		       AK4678_11_LIN_VOLUME, 0, 0xF1, 0, ivol_tlv),
	SOC_SINGLE_TLV("Headphone Output Volume",
		       AK4678_0F_HP_VOLUME, 0, 0x26, 0, hpout_tlv),
	SOC_SINGLE_TLV("Speaker Output Volume",
		       AK4678_10_SPRC_VOLUME, 0, 0xF, 0, spkout_tlv),
	SOC_SINGLE_TLV("Receiver Output Volume",
		       AK4678_10_SPRC_VOLUME, 4, 0xF, 0, rcvout_tlv),
	SOC_SINGLE_TLV("Line Output Volume",
		       AK4678_0E_LINEOUT_VOLUME, 0, 0x5, 0, lineout_tlv),
	SOC_SINGLE_TLV("Digital Output Volume : DATT-A",
		       AK4678_1D_LOUT_VOLUME, 0, 0x7F, 1, datta_tlv),
	SOC_SINGLE_TLV("PCM IF A  Input  Volume : DATT-B",
		       AK4678_23_DVOLB_CONTROL, 0, 0x7F, 1, dattb_tlv),
	SOC_SINGLE_TLV("PCM IF A to B  Volume : DATT-C",
		       AK4678_24_DVOLC_CONTROL, 0, 0x7F, 1, dattc_tlv),
	SOC_SINGLE_TLV("PCM IF B input Volume : BIVOL",
		       AK4678_18_MODE1_CONTROL, 3, 4, 1, bivol_tlv),
	SOC_SINGLE_TLV("SVOLAL Volume",
		       AK4678_1C_SIDETONE_VOLUMEA, 0, 4, 1, svolal_tlv),
	SOC_SINGLE_TLV("SVOLAR Volume",
		       AK4678_1C_SIDETONE_VOLUMEA, 4, 4, 1, svolar_tlv),
	SOC_SINGLE_TLV("SVOLB Volume",
		       AK4678_22_SIDETONE_VOLUMEB, 0, 4, 1, svolb_tlv),

	SOC_SINGLE("Mic1 Bias Level Up", AK4678_02_POWER_MANAGEMENT2, 1, 1, 0),
	SOC_SINGLE("Mic2 Bias Level Up", AK4678_02_POWER_MANAGEMENT2, 3, 1, 0),
	SOC_SINGLE("Left Mic Gain", AK4678_07_MIC_AMP_GAIN, 0, 13, 0),
	SOC_SINGLE("Right Mic Gain", AK4678_07_MIC_AMP_GAIN, 4, 13, 0),
	SOC_SINGLE("High Path Filter 1", AK4678_19_FILTER_SELECT0, 5, 3, 0),
	SOC_SINGLE("High Path Filter 2", AK4678_1A_FILTER_SELECT1, 4, 1, 0),
	SOC_SINGLE("Low Path Filter", AK4678_1A_FILTER_SELECT1, 5, 1, 0),
	SOC_ENUM_EXT("Stereo Enphasis Filter Control", ak4678_enum[0],
		     get_stereo_ef, set_stereo_ef),
	SOC_SINGLE("5 Band Equalizer", AK4678_17_MODE0_CONTROL, 3, 1, 0),
	SOC_SINGLE_TLV("5band EQ1 Gain", AK4678_6A_5BAND_E1_GAIN, 0, 48, 1,
		       eqgain_tlv),
	SOC_SINGLE_TLV("5band EQ2 Gain", AK4678_6B_5BAND_E2_GAIN, 0, 48, 1,
		       eqgain_tlv),
	SOC_SINGLE_TLV("5band EQ3 Gain", AK4678_6C_5BAND_E3_GAIN, 0, 48, 1,
		       eqgain_tlv),
	SOC_SINGLE_TLV("5band EQ4 Gain", AK4678_6D_5BAND_E4_GAIN, 0, 48, 1,
		       eqgain_tlv),
	SOC_SINGLE_TLV("5band EQ5 Gain", AK4678_6E_5BAND_E5_GAIN, 0, 48, 1,
		       eqgain_tlv),
	SOC_SINGLE("Auto Level Control", AK4678_17_MODE0_CONTROL, 0, 1, 0),
	SOC_ENUM_EXT("DVLC_DRC Control", ak4678_enum[1], get_dvlc_drc,
		     set_dvlc_drc),

	SOC_SINGLE("Headphone L+R", AK4678_0B_HP_MANAGEMENT, 3, 1, 0),
	SOC_SINGLE("Lineout L+R", AK4678_0A_LINE_MANAGEMENT, 3, 1, 0),
};

/* input selection MUX */
static const char *ak4678_right_line_texts[] = {
	"Stereo Single-end LIN1", "Stereo Single-end LIN2",
	    "Stereo Single-end LIN3", "Stereo Single-end LIN4",
	"Mono DIF IN3+/-", "Mono DIF IN2+/-", "Mono DIF IN1+/-"
};

static const unsigned int ak4678_rline_values[] = {
	0x0, 0x05, 0xa, 0xf, 0x42, 0x24, 0x34
};

static const struct soc_enum ak4678_rline_enum =
SOC_VALUE_ENUM_SINGLE(AK4678_06_MIC_SIGNAL_SELECT, 0, 0x7f,
		      ARRAY_SIZE(ak4678_right_line_texts),
		      ak4678_right_line_texts,
		      ak4678_rline_values);
static const struct snd_kcontrol_new ak4678_input_select_controls =
SOC_DAPM_VALUE_ENUM("Route", ak4678_rline_enum);

/* ADC virtual MUX */
static const char *adc_mux_text[] = {
	"OFF",
	"ON",
};

static const struct soc_enum adc_enum = SOC_ENUM_SINGLE(0, 0, 2, adc_mux_text);

static const struct snd_kcontrol_new adcl_mux =
SOC_DAPM_ENUM_VIRT("ADCL Mux", adc_enum);

static const struct snd_kcontrol_new adcr_mux =
SOC_DAPM_ENUM_VIRT("ADCR Mux", adc_enum);

/*
 * Headphone virtual MUX
 */
static const char *HP_mux_text[] = {
	"OFF",
	"ON",
};

static const struct soc_enum HP_enum = SOC_ENUM_SINGLE(0, 0, 2, HP_mux_text);

static const struct snd_kcontrol_new HPL_mux =
SOC_DAPM_ENUM_VIRT("HPL Mux", HP_enum);

static const struct snd_kcontrol_new HPR_mux =
SOC_DAPM_ENUM_VIRT("HPR Mux", HP_enum);

/*
 * PFSEL MUX
 */
static const char *ak4678_PFSEL_texts[] = {
	"ADC", "STDI",
};

static const struct soc_enum ak4678_PFSEL_mux_enum =
SOC_ENUM_SINGLE(AK4678_19_FILTER_SELECT0, 0,
		ARRAY_SIZE(ak4678_PFSEL_texts),
		ak4678_PFSEL_texts);
static const struct snd_kcontrol_new ak4678_PFSEL_mux_control =
SOC_DAPM_ENUM("Route", ak4678_PFSEL_mux_enum);

/*
 * PFSDO MUX
 */
static const char *ak4678_PFSDO_texts[] = {
	"ADC", "PFSEL",
};

static const struct soc_enum ak4678_PFSDO_mux_enum =
SOC_ENUM_SINGLE(AK4678_19_FILTER_SELECT0, 1,
		ARRAY_SIZE(ak4678_PFSDO_texts),
		ak4678_PFSDO_texts);
static const struct snd_kcontrol_new ak4678_PFSDO_mux_control =
SOC_DAPM_ENUM("Route", ak4678_PFSDO_mux_enum);

/*
 * SDOL  MUX
 */
static const char *ak4678_SDOL_texts[] = {
	"PFSDO Lch", "MIX1L", "PFSDO + MIX1L", "(PFSDO + MIX1L)/2"
};

static const struct soc_enum ak4678_SDOL_mux_enum =
SOC_ENUM_SINGLE(AK4678_28_DMIX_CONTROL3, 4,
		ARRAY_SIZE(ak4678_SDOL_texts),
		ak4678_SDOL_texts);
static const struct snd_kcontrol_new ak4678_SDOL_mux_control =
SOC_DAPM_ENUM("Route", ak4678_SDOL_mux_enum);

/*
 * SDOR  MUX
 */
static const char *ak4678_SDOR_texts[] = {
	"PFSDO Rch", "MIX1R", "PFSDO + MIX1R", "(PFSDO + MIX1R)/2"
};

static const struct soc_enum ak4678_SDOR_mux_enum =
SOC_ENUM_SINGLE(AK4678_28_DMIX_CONTROL3, 6,
		ARRAY_SIZE(ak4678_SDOR_texts),
		ak4678_SDOR_texts);
static const struct snd_kcontrol_new ak4678_SDOR_mux_control =
SOC_DAPM_ENUM("Route", ak4678_SDOR_mux_enum);

/*
 * SDOD   MUX :SDTO Capture Switch
 */

static const char *ak4678_SDTO_texts[] = {
	"Enable", "Disable"
};

static const struct soc_enum ak4678_SDTO_mux_enum =
SOC_ENUM_SINGLE(AK4678_05_FORMAT_SELECT, 4,
		ARRAY_SIZE(ak4678_SDTO_texts),
		ak4678_SDTO_texts);
static const struct snd_kcontrol_new ak4678_SDTO_mux_control =
SOC_DAPM_ENUM("Route", ak4678_SDTO_mux_enum);

/*
 * PFMXL   MUX
 */
static const char *ak4678_PFMXL_texts[] = {
	"STDI Lch", "SVOLA Lch", "STDI + SVOLA"
};

static const struct soc_enum ak4678_PFMXL_mux_enum =
SOC_ENUM_SINGLE(AK4678_14_DIGMIX_CONTROL, 0,
		ARRAY_SIZE(ak4678_PFMXL_texts),
		ak4678_PFMXL_texts);
static const struct snd_kcontrol_new ak4678_PFMXL_mux_control =
SOC_DAPM_ENUM("Route", ak4678_PFMXL_mux_enum);
/*
 * PFMXR   MUX
 */
static const char *ak4678_PFMXR_texts[] = {
	"STDI Rch", "SVOLA Rch", "STDI + SVOLA"
};

static const struct soc_enum ak4678_PFMXR_mux_enum =
SOC_ENUM_SINGLE(AK4678_14_DIGMIX_CONTROL, 2,
		ARRAY_SIZE(ak4678_PFMXR_texts),
		ak4678_PFMXR_texts);
static const struct snd_kcontrol_new ak4678_PFMXR_mux_control =
SOC_DAPM_ENUM("Route", ak4678_PFMXR_mux_enum);

/*
 * SRMXL   MUX
 */
static const char *ak4678_SRMXL_texts[] = {
	"PFMXL Lch", "MIX1L", "PFMXL + MIX1L"
};

static const struct soc_enum ak4678_SRMXL_mux_enum =
SOC_ENUM_SINGLE(AK4678_14_DIGMIX_CONTROL, 4,
		ARRAY_SIZE(ak4678_SRMXL_texts),
		ak4678_SRMXL_texts);
static const struct snd_kcontrol_new ak4678_SRMXL_mux_control =
SOC_DAPM_ENUM("Route", ak4678_SRMXL_mux_enum);
/*
 * SRMXR   MUX
 */
static const char *ak4678_SRMXR_texts[] = {
	"PFMXR Rch", "MIX1R", "PFMXR + MIX1R"
};

static const struct soc_enum ak4678_SRMXR_mux_enum =
SOC_ENUM_SINGLE(AK4678_14_DIGMIX_CONTROL, 6,
		ARRAY_SIZE(ak4678_SRMXR_texts),
		ak4678_SRMXR_texts);
static const struct snd_kcontrol_new ak4678_SRMXR_mux_control =
SOC_DAPM_ENUM("Route", ak4678_SRMXR_mux_enum);

/*
 * DRC virtual   MUX
 */
static const char *DRC_mux_text[] = {
	"OFF",
	"ON",
};

static const struct soc_enum DRC_enum = SOC_ENUM_SINGLE(0, 0, 2, DRC_mux_text);

static const struct snd_kcontrol_new DRC_mux =
SOC_DAPM_ENUM_VIRT("DRC Mux", DRC_enum);

/*
 * EQ virtual   MUX
 */
static const char *EQ_mux_text[] = {
	"ON",
	"OFF",
};

static const struct soc_enum EQ_enum = SOC_ENUM_SINGLE(0, 0, 2, EQ_mux_text);

static const struct snd_kcontrol_new EQ_virt_mux =
SOC_DAPM_ENUM_VIRT("EQ Mux", EQ_enum);

/*
 * DASEL   MUX
 */
static const char *ak4678_DASEL_texts[] = {
	"DATT-A", "DRC", "STDI"
};

static const struct soc_enum ak4678_DASEL_mux_enum =
SOC_ENUM_SINGLE(AK4678_19_FILTER_SELECT0, 2,
		ARRAY_SIZE(ak4678_DASEL_texts),
		ak4678_DASEL_texts);
static const struct snd_kcontrol_new ak4678_DASEL_mux_control =
SOC_DAPM_ENUM("Route", ak4678_DASEL_mux_enum);

/* Output Mixers */
/*
 * LINEOUT
 */
static int ak4678_lineout_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMD:
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, AK4678_0A_LINE_MANAGEMENT, 0x4, 0x4);
		break;
	case SND_SOC_DAPM_POST_PMU:
	case SND_SOC_DAPM_POST_PMD:
		mdelay(300);
		snd_soc_update_bits(codec, AK4678_0A_LINE_MANAGEMENT, 0x4, 0x0);
		break;
	}

	return 0;
}

static const struct snd_kcontrol_new ak4678_lout1_mixer_controls[] = {
	SOC_DAPM_SINGLE("DACL", AK4678_09_DAC_PATH_SELECT, 0, 1, 0),
};

static const struct snd_kcontrol_new ak4678_rout1_mixer_controls[] = {
	SOC_DAPM_SINGLE("DACR", AK4678_09_DAC_PATH_SELECT, 1, 1, 0),
};

/*
 * RCV
 */
static int ak4678_RCV_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMD:
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, AK4678_0D_SPK_MANAGEMENT, 0x2, 0x2);
		mdelay(1);
		break;
	case SND_SOC_DAPM_POST_PMU:
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, AK4678_0D_SPK_MANAGEMENT, 0x2, 0x0);
		break;
	}

	return 0;
}

static const struct snd_kcontrol_new ak4678_RCVL_mixer_controls[] = {
	SOC_DAPM_SINGLE("DACRL", AK4678_09_DAC_PATH_SELECT, 4, 1, 0),
};

static const struct snd_kcontrol_new ak4678_RCVR_mixer_controls[] = {
	SOC_DAPM_SINGLE("DACRR", AK4678_09_DAC_PATH_SELECT, 5, 1, 0),
};

/*
 * SPK
 */
static const struct snd_kcontrol_new ak4678_SPKL_mixer_controls[] = {
	SOC_DAPM_SINGLE("DACSPKL", AK4678_09_DAC_PATH_SELECT, 6, 1, 0),
};

static const struct snd_kcontrol_new ak4678_SPKR_mixer_controls[] = {
	SOC_DAPM_SINGLE("DACSPKR", AK4678_09_DAC_PATH_SELECT, 7, 1, 0),
};

/*
 * Headphone
 */
static const struct snd_kcontrol_new ak4678_HPL_mixer_controls[] = {
	SOC_DAPM_SINGLE("DACHPL", AK4678_0B_HP_MANAGEMENT, 0, 1, 0),
};

static const struct snd_kcontrol_new ak4678_HPR_mixer_controls[] = {
	SOC_DAPM_SINGLE("DACHPR", AK4678_0B_HP_MANAGEMENT, 1, 1, 0),
};

/*
 * MIX1L   MUX
 */
static const char *ak4678_MIX1L_texts[] = {
	"DATT-B", "BIVOL Lch", "BIVOL Rch", "BIVOL Lch + BIVOL Rch",
	    "DATT-B + BIVOL Lch",
	"DATT-B + BIVOL Rch", "DATT-B + BIVOL Lch + BIVOL Rch"
};

static const struct soc_enum ak4678_MIX1L_mux_enum =
SOC_ENUM_SINGLE(AK4678_25_DMIX_CONTROL0, 0,
		ARRAY_SIZE(ak4678_MIX1L_texts),
		ak4678_MIX1L_texts);
static const struct snd_kcontrol_new ak4678_MIX1L_mux_control =
SOC_DAPM_ENUM("Route", ak4678_MIX1L_mux_enum);

/*
 * MIX1R   MUX
 */
static const struct soc_enum ak4678_MIX1R_mux_enum =
SOC_ENUM_SINGLE(AK4678_25_DMIX_CONTROL0, 3,
		ARRAY_SIZE(ak4678_MIX1L_texts),
		ak4678_MIX1L_texts);
static const struct snd_kcontrol_new ak4678_MIX1R_mux_control =
SOC_DAPM_ENUM("Route", ak4678_MIX1R_mux_enum);

/*
 * MIX2A   MUX
 */
static const char *ak4678_MIX2A_texts[] = {
	"BIVOL Lch", "BIVOL Rch", "BIVOL Lch + BIVOL Rch",
	    "(BIVOL Lch + BIVOL Rch)/2"
};

static const struct soc_enum ak4678_MIX2A_mux_enum =
SOC_ENUM_SINGLE(AK4678_26_DMIX_CONTROL1, 0,
		ARRAY_SIZE(ak4678_MIX2A_texts),
		ak4678_MIX2A_texts);
static const struct snd_kcontrol_new ak4678_MIX2A_mux_control =
SOC_DAPM_ENUM("Route", ak4678_MIX2A_mux_enum);

/*
 * MIX2B   MUX
 */
static const char *ak4678_MIX2B_texts[] = {
	"DATT-A Lch", "DATT-A Rch", "DATT-A Lch + DATT-A Rch",
	    "(DATT-A Lch + DATT-A Rch)/2"
};

static const struct soc_enum ak4678_MIX2B_mux_enum =
SOC_ENUM_SINGLE(AK4678_26_DMIX_CONTROL1, 2,
		ARRAY_SIZE(ak4678_MIX2B_texts),
		ak4678_MIX2B_texts);
static const struct snd_kcontrol_new ak4678_MIX2B_mux_control =
SOC_DAPM_ENUM("Route", ak4678_MIX2B_mux_enum);

/*
 * MIX2C   MUX
 */
static const char *ak4678_MIX2C_texts[] = {
	"MIX2A", "MIX2B", "MIX2A + MIX2B", "(MIX2A + MIX2B)/2"
};

static const struct soc_enum ak4678_MIX2C_mux_enum =
SOC_ENUM_SINGLE(AK4678_26_DMIX_CONTROL1, 4,
		ARRAY_SIZE(ak4678_MIX2C_texts),
		ak4678_MIX2C_texts);
static const struct snd_kcontrol_new ak4678_MIX2C_mux_control =
SOC_DAPM_ENUM("Route", ak4678_MIX2C_mux_enum);

/*
 * MIX3   MUX
 */
static const char *ak4678_MIX3_texts[] = {
	"DATT-A stereo", "DATT-A Mono Lch", "DATT-A Mono Rch",
	    "DATT-A Lch + DATT-A Rch",
	"(DATT-A Lch+ DATT-A Rch)/2", "DATT-A Lch + DATT-A Rch + DATT-C",
	    "PFSDO", "DATT-C"
};

static const struct soc_enum ak4678_MIX3_mux_enum =
SOC_ENUM_SINGLE(AK4678_27_DMIX_CONTROL2, 0,
		ARRAY_SIZE(ak4678_MIX3_texts),
		ak4678_MIX3_texts);
static const struct snd_kcontrol_new ak4678_MIX3_mux_control =
SOC_DAPM_ENUM("Route", ak4678_MIX3_mux_enum);

/*
 * SBMX   MUX
 */
static const char *ak4678_SBMX_texts[] = {
	"STDIA", "SVOLB", "STDIA + SVOLB"
};

static const struct soc_enum ak4678_SBMX_mux_enum =
SOC_ENUM_SINGLE(AK4678_28_DMIX_CONTROL3, 0,
		ARRAY_SIZE(ak4678_SBMX_texts),
		ak4678_SBMX_texts);
static const struct snd_kcontrol_new ak4678_SBMX_mux_control =
SOC_DAPM_ENUM("Route", ak4678_SBMX_mux_enum);

/* PCM port A output  SDOAD :Enable : 0, disable :1 */
static const struct snd_kcontrol_new ak4678_SDTOA_switch_controls =
SOC_DAPM_SINGLE("Switch", AK4678_20_PCM_IF_CONTROL0, 7, 1, 1);

/*  PCM port B output  SDOBD :Enable : 0, disable :1 */
static const struct snd_kcontrol_new ak4678_SDTOB_switch_controls =
SOC_DAPM_SINGLE("Switch", AK4678_21_PCM_IF_CONTROL1, 7, 1, 1);

int ak4678_porta_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	gprintk("BT Auido fmt 0x20 :0x%x\n", snd_soc_read(codec, AK4678_20_PCM_IF_CONTROL0));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ak4678_set_port_dai_fmt(codec, AK4678_PORTA,
					SND_SOC_DAIFMT_DSP_A |
					SND_SOC_DAIFMT_IB_NF |
					SND_SOC_DAIFMT_CBS_CFS);
		gprintk("BT Auido fmt 0x20 :0x%x\n", snd_soc_read(codec, AK4678_20_PCM_IF_CONTROL0));
		break;
	case SND_SOC_DAPM_POST_PMD:
	case SND_SOC_DAPM_PRE_PMD:
		break;
	}
	return 0;
}

int ak4678_portb_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
	case SND_SOC_DAPM_PRE_PMU:
		gprintk("DSP startup\n");
		break;
	case SND_SOC_DAPM_POST_PMD:
	case SND_SOC_DAPM_PRE_PMD:
		gprintk("power down DSP\n");
		break;
	}
	return 0;
}

/*
 * DAPM Widget
 */
static const struct snd_soc_dapm_widget ak4678_dapm_widgets[] = {
	SND_SOC_DAPM_MICBIAS("Mic1 Bias", AK4678_02_POWER_MANAGEMENT2, 0, 0),
	SND_SOC_DAPM_MICBIAS("Mic2 Bias", AK4678_02_POWER_MANAGEMENT2, 2, 0),

	SND_SOC_DAPM_MUX("Input Select Mux", SND_SOC_NOPM, 0, 0,
			 &ak4678_input_select_controls),

	SND_SOC_DAPM_VIRT_MUX("ADCL Mux", SND_SOC_NOPM, 0, 0,
			      &adcl_mux),
	SND_SOC_DAPM_VIRT_MUX("ADCR Mux", SND_SOC_NOPM, 0, 0,
			      &adcr_mux),

	SND_SOC_DAPM_MUX("PFSEL", AK4678_00_POWER_MANAGEMENT0, 1, 0,
			 &ak4678_PFSEL_mux_control),
	SND_SOC_DAPM_MUX("PFSDO", SND_SOC_NOPM, 0, 0,
			 &ak4678_PFSDO_mux_control),

	SND_SOC_DAPM_MUX("SDOL", SND_SOC_NOPM, 0, 0,
			 &ak4678_SDOL_mux_control),
	SND_SOC_DAPM_MUX("SDOR", SND_SOC_NOPM, 0, 0,
			 &ak4678_SDOR_mux_control),

	SND_SOC_DAPM_MUX("SDTO Capture Switch", SND_SOC_NOPM, 0, 0,
			 &ak4678_SDTO_mux_control),

	SND_SOC_DAPM_ADC("Left ADC", "Left Capture",
			 AK4678_00_POWER_MANAGEMENT0, 4, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Right Capture",
			 AK4678_00_POWER_MANAGEMENT0, 5, 0),

	SND_SOC_DAPM_AIF_IN("STDI Lch", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("STDI Rch", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SDTO Lch", "Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SDTO Rch", "Capture", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_MUX("PFMXL", SND_SOC_NOPM, 0, 0,
			 &ak4678_PFMXL_mux_control),
	SND_SOC_DAPM_MUX("PFMXR", SND_SOC_NOPM, 0, 0,
			 &ak4678_PFMXR_mux_control),

	SND_SOC_DAPM_MUX("SRMXL", SND_SOC_NOPM, 0, 0,
			 &ak4678_SRMXL_mux_control),
	SND_SOC_DAPM_MUX("SRMXR", SND_SOC_NOPM, 0, 0,
			 &ak4678_SRMXR_mux_control),

	SND_SOC_DAPM_VIRT_MUX("DRC", AK4678_01_POWER_MANAGEMENT1, 1, 0,
			      &DRC_mux),

	SND_SOC_DAPM_VIRT_MUX("EQ", AK4678_01_POWER_MANAGEMENT1, 0, 0,
			      &EQ_virt_mux),

	SND_SOC_DAPM_MUX("DASEL MUX", SND_SOC_NOPM, 0, 0,
			 &ak4678_DASEL_mux_control),

	SND_SOC_DAPM_DAC("DAC Right", "Right Playback",
			 AK4678_01_POWER_MANAGEMENT1, 3, 0),
	SND_SOC_DAPM_DAC("DAC Left", "Left Playback",
			 AK4678_01_POWER_MANAGEMENT1, 2, 0),

	SND_SOC_DAPM_MIXER_E("LOUT1 Mixer", AK4678_0A_LINE_MANAGEMENT, 0, 0,
			     &ak4678_lout1_mixer_controls[0],
			     ARRAY_SIZE(ak4678_lout1_mixer_controls),
			     ak4678_lineout_event,
			     (SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD)),	//PMLO
	SND_SOC_DAPM_MIXER_E("ROUT1 Mixer", AK4678_0A_LINE_MANAGEMENT, 1, 0,
			     &ak4678_rout1_mixer_controls[0],
			     ARRAY_SIZE(ak4678_rout1_mixer_controls),
			     ak4678_lineout_event,
			     (SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD)),	//PMRO
	SND_SOC_DAPM_MIXER_E("RCVL Mixer", AK4678_0D_SPK_MANAGEMENT,
			     0, 0, &ak4678_RCVL_mixer_controls[0],
			     ARRAY_SIZE(ak4678_RCVL_mixer_controls),
			     ak4678_RCV_event,
			     SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
			     SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RCVR Mixer", AK4678_0D_SPK_MANAGEMENT,
			     0, 0, &ak4678_RCVR_mixer_controls[0],
			     ARRAY_SIZE(ak4678_RCVR_mixer_controls),
			     ak4678_RCV_event,
			     SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),	//CONFIG_LINF

	SND_SOC_DAPM_MIXER("SPP Mixer", AK4678_0D_SPK_MANAGEMENT, 4, 0,
			   &ak4678_SPKL_mixer_controls[0],
			   ARRAY_SIZE(ak4678_SPKL_mixer_controls)),
	SND_SOC_DAPM_MIXER("SPN Mixer", AK4678_0D_SPK_MANAGEMENT, 4, 0,
			   &ak4678_SPKR_mixer_controls[0],
			   ARRAY_SIZE(ak4678_SPKR_mixer_controls)),

	SND_SOC_DAPM_MIXER("HPL Mixer", AK4678_0B_HP_MANAGEMENT, 0, 0,
			   NULL,
			   0),
	SND_SOC_DAPM_MIXER("HPR Mixer", AK4678_0B_HP_MANAGEMENT, 1, 0,
			   NULL,
			   0),

	SND_SOC_DAPM_VIRT_MUX("HPL Mux", SND_SOC_NOPM, 0, 0,
			      &HPL_mux),
	SND_SOC_DAPM_VIRT_MUX("HPR Mux", SND_SOC_NOPM, 0, 0,
			      &HPR_mux),

	SND_SOC_DAPM_MUX("MIX1L Mixer", AK4678_1F_PCM_IF_MANAGEMENT, 7, 0,
			 &ak4678_MIX1L_mux_control),
	SND_SOC_DAPM_MUX("MIX1R Mixer", AK4678_1F_PCM_IF_MANAGEMENT, 7, 0,
			 &ak4678_MIX1R_mux_control),

	SND_SOC_DAPM_MUX("MIX2A Mixer", SND_SOC_NOPM, 0, 0,
			 &ak4678_MIX2A_mux_control),

	SND_SOC_DAPM_MUX("MIX2B Mixer", SND_SOC_NOPM, 0, 0,
			 &ak4678_MIX2B_mux_control),

	SND_SOC_DAPM_MUX("MIX2C Mixer", SND_SOC_NOPM, 0, 0,
			 &ak4678_MIX2C_mux_control),

	SND_SOC_DAPM_MUX("MIX3 Mixer", SND_SOC_NOPM, 0, 0,
			 &ak4678_MIX3_mux_control),

	SND_SOC_DAPM_MUX("SBMX", SND_SOC_NOPM, 0, 0,
			 &ak4678_SBMX_mux_control),

	SND_SOC_DAPM_SWITCH("SDTOA Capture", SND_SOC_NOPM, 0, 0,
			    &ak4678_SDTOA_switch_controls),
	SND_SOC_DAPM_SWITCH("SDTOB Capture", SND_SOC_NOPM, 0, 0,
			    &ak4678_SDTOB_switch_controls),

	/* Supply :PMOSC */
	SND_SOC_DAPM_SUPPLY("PMOSC", AK4678_1F_PCM_IF_MANAGEMENT, 3, 0, NULL,
			    0),
	/* Supply :PMPCMA */
	SND_SOC_DAPM_SUPPLY("PMPCMA", AK4678_1F_PCM_IF_MANAGEMENT, 0, 0, NULL,
			    0),
	/* Supply :PMPCMB */
	SND_SOC_DAPM_SUPPLY("PMPCMB", AK4678_1F_PCM_IF_MANAGEMENT, 4, 0, NULL,
			    0),

	SND_SOC_DAPM_AIF_IN_E("STDIA", "Playback", 0,
			      AK4678_1F_PCM_IF_MANAGEMENT, 1, 0,
			      ak4678_porta_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT("SDTOA", "Capture", 0, AK4678_1F_PCM_IF_MANAGEMENT,
			     2, 0),
	SND_SOC_DAPM_AIF_IN_E("STDIB Lch", "Playback", 0,
			      AK4678_1F_PCM_IF_MANAGEMENT, 5, 0,
			      ak4678_portb_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN("STDIB Rch", "Playback", 0,
			    AK4678_1F_PCM_IF_MANAGEMENT, 5, 0),

	SND_SOC_DAPM_AIF_OUT("SDTOB Lch", "Capture", 0,
			     AK4678_1F_PCM_IF_MANAGEMENT, 6, 0),
	SND_SOC_DAPM_AIF_OUT("SDTOB Rch", "Capture", 0,
			     AK4678_1F_PCM_IF_MANAGEMENT, 6, 0),

	/* Outputs */
	SND_SOC_DAPM_OUTPUT("LOUT1"),
	SND_SOC_DAPM_OUTPUT("ROUT1"),
	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("HPR"),
	SND_SOC_DAPM_OUTPUT("RCVP"),
	SND_SOC_DAPM_OUTPUT("RCVN"),
	SND_SOC_DAPM_OUTPUT("SPP"),
	SND_SOC_DAPM_OUTPUT("SPN"),

	/* Inputs */
	SND_SOC_DAPM_INPUT("LIN1"),
	SND_SOC_DAPM_INPUT("RIN1"),
	SND_SOC_DAPM_INPUT("LIN2"),
	SND_SOC_DAPM_INPUT("RIN2"),
	SND_SOC_DAPM_INPUT("LIN3"),
	SND_SOC_DAPM_INPUT("RIN3"),
	SND_SOC_DAPM_INPUT("LIN4"),
	SND_SOC_DAPM_INPUT("RIN4"),
};

static const struct snd_soc_dapm_route audio_route[] = {
	/* Inputs */
	{"Mic1 Bias", NULL, "LIN1"},
	{"Mic1 Bias", NULL, "RIN1"},
	{"Mic2 Bias", NULL, "LIN3"},
	{"Mic2 Bias", NULL, "RIN3"},

	{"Input Select Mux", "Stereo Single-end LIN1", "Mic1 Bias"},

	{"Mic1 Bias", NULL, "LIN2"},
	{"Mic1 Bias", NULL, "RIN2"},
	{"Input Select Mux", "Stereo Single-end LIN2", "Mic1 Bias"},
	{"Input Select Mux", "Stereo Single-end LIN3", "Mic2 Bias"},

	{"Input Select Mux", "Stereo Single-end LIN4", "LIN4"},
	{"Input Select Mux", "Stereo Single-end LIN4", "RIN4"},
	{"Input Select Mux", "Mono DIF IN3+/-", "LIN3"},
	{"Input Select Mux", "Mono DIF IN3+/-", "RIN3"},

	{"Input Select Mux", "Mono DIF IN2+/-", "Mic2 Bias"},

	{"Input Select Mux", "Mono DIF IN1+/-", "Mic1 Bias"},
	{"Input Select Mux", "Mono DIF IN1+/-", "Mic2 Bias"},

	{"ADCL Mux", "ON", "Input Select Mux"},
	{"ADCR Mux", "ON", "Input Select Mux"},

	{"Left ADC", NULL, "ADCL Mux"},
	{"Right ADC", NULL, "ADCR Mux"},

	{"PFSEL", "ADC", "Left ADC"},
	{"PFSEL", "ADC", "Right ADC"},

	{"PFSEL", "STDI", "STDI Rch"},
	{"PFSEL", "STDI", "STDI Lch"},

	{"PFSDO", "ADC", "Left ADC"},
	{"PFSDO", "ADC", "Right ADC"},

	{"PFSDO", "PFSEL", "PFSEL"},

	{"SDOL", "PFSDO Lch", "PFSDO"},
	{"SDOL", "MIX1L", "MIX1L Mixer"},
	{"SDOL", "PFSDO + MIX1L", "PFSDO"},
	{"SDOL", "PFSDO + MIX1L", "MIX1L Mixer"},
	{"SDOL", "(PFSDO + MIX1L)/2", "PFSDO"},
	{"SDOL", "(PFSDO + MIX1L)/2", "MIX1L Mixer"},

	{"SDOR", "PFSDO Rch", "PFSDO"},
	{"SDOR", "MIX1R", "MIX1R Mixer"},
	{"SDOR", "PFSDO + MIX1R", "PFSDO"},
	{"SDOR", "PFSDO + MIX1R", "MIX1R Mixer"},
	{"SDOR", "(PFSDO + MIX1R)/2", "PFSDO"},
	{"SDOR", "(PFSDO + MIX1R)/2", "MIX1R Mixer"},

	{"SDTO Capture Switch", "Enable", "SDOR"},
	{"SDTO Capture Switch", "Enable", "SDOL"},
	{"SDTO Lch", NULL, "SDTO Capture Switch"},
	{"SDTO Rch", NULL, "SDTO Capture Switch"},

	{"LOUT1", NULL, "LOUT1 Mixer"},
	{"ROUT1", NULL, "ROUT1 Mixer"},
	{"HPL", NULL, "HPL Mixer"},
	{"HPR", NULL, "HPR Mixer"},
	{"RCVP", NULL, "RCVL Mixer"},
	{"RCVN", NULL, "RCVR Mixer"},
	{"SPP", NULL, "SPP Mixer"},
	{"SPN", NULL, "SPN Mixer"},

	{"LOUT1 Mixer", "DACL", "DAC Left"},
	{"ROUT1 Mixer", "DACR", "DAC Right"},

	{"HPL Mixer", NULL, "HPL Mux"},
	{"HPR Mixer", NULL, "HPR Mux"},
	{"HPL Mux", "ON", "DAC Left"},
	{"HPR Mux", "ON", "DAC Right"},

	{"RCVL Mixer", "DACRL", "DAC Left"},
	{"RCVR Mixer", "DACRR", "DAC Right"},
	{"SPP Mixer", "DACSPKL", "DAC Left"},
	{"SPN Mixer", "DACSPKR", "DAC Right"},

	{"DAC Left", NULL, "DASEL MUX"},
	{"DAC Right", NULL, "DASEL MUX"},

	{"DASEL MUX", "DRC", "DRC"},
	{"DASEL MUX", "DATT-A", "EQ"},

	{"DASEL MUX", "STDI", "STDI Lch"},
	{"DASEL MUX", "STDI", "STDI Rch"},

	{"DRC", "ON", "EQ"},
	{"EQ", "ON", "SRMXL"},
	{"EQ", "ON", "SRMXR"},

	{"SRMXL", "PFMXL Lch", "PFMXL"},
	{"SRMXL", "MIX1L", "MIX1L Mixer"},
	{"SRMXL", "PFMXL + MIX1L", "PFMXL"},
	{"SRMXL", "PFMXL + MIX1L", "MIX1L Mixer"},

	{"SRMXR", "PFMXR Rch", "PFMXR"},
	{"SRMXR", "MIX1R", "MIX1R Mixer"},
	{"SRMXR", "PFMXR + MIX1R", "PFMXR"},
	{"SRMXR", "PFMXR + MIX1R", "MIX1R Mixer"},

	{"PFMXL", "STDI Lch", "STDI Lch"},
	{"PFMXL", "SVOLA Lch", "PFSDO"},
	{"PFMXL", "STDI + SVOLA", "STDI Lch"},
	{"PFMXL", "STDI + SVOLA", "PFSDO"},

	{"PFMXR", "STDI Rch", "STDI Rch"},
	{"PFMXR", "SVOLA Rch", "PFSDO"},
	{"PFMXR", "STDI + SVOLA", "STDI Rch"},
	{"PFMXR", "STDI + SVOLA", "PFSDO"},

	{"MIX1L Mixer", "DATT-B", "STDIA"},
	{"MIX1L Mixer", "BIVOL Lch", "STDIB Lch"},
	{"MIX1L Mixer", "BIVOL Rch", "STDIB Rch"},
	{"MIX1L Mixer", "BIVOL Lch + BIVOL Rch", "STDIB Lch"},
	{"MIX1L Mixer", "BIVOL Lch + BIVOL Rch", "STDIB Rch"},
	{"MIX1L Mixer", "DATT-B + BIVOL Lch", "STDIA"},
	{"MIX1L Mixer", "DATT-B + BIVOL Lch", "STDIB Lch"},
	{"MIX1L Mixer", "DATT-B + BIVOL Rch", "STDIA"},
	{"MIX1L Mixer", "DATT-B + BIVOL Rch", "STDIB Rch"},
	{"MIX1L Mixer", "DATT-B + BIVOL Lch + BIVOL Rch", "STDIA"},
	{"MIX1L Mixer", "DATT-B + BIVOL Lch + BIVOL Rch", "STDIB Rch"},
	{"MIX1L Mixer", "DATT-B + BIVOL Lch + BIVOL Rch", "STDIB Lch"},

	{"MIX1R Mixer", "DATT-B", "STDIA"},
	{"MIX1R Mixer", "BIVOL Lch", "STDIB Lch"},
	{"MIX1R Mixer", "BIVOL Rch", "STDIB Rch"},
	{"MIX1R Mixer", "BIVOL Lch + BIVOL Rch", "STDIB Lch"},
	{"MIX1R Mixer", "BIVOL Lch + BIVOL Rch", "STDIB Rch"},
	{"MIX1R Mixer", "DATT-B + BIVOL Lch", "STDIA"},
	{"MIX1R Mixer", "DATT-B + BIVOL Lch", "STDIB Lch"},
	{"MIX1R Mixer", "DATT-B + BIVOL Rch", "STDIA"},
	{"MIX1R Mixer", "DATT-B + BIVOL Rch", "STDIB Rch"},
	{"MIX1R Mixer", "DATT-B + BIVOL Lch + BIVOL Rch", "STDIA"},
	{"MIX1R Mixer", "DATT-B + BIVOL Lch + BIVOL Rch", "STDIB Rch"},
	{"MIX1R Mixer", "DATT-B + BIVOL Lch + BIVOL Rch", "STDIB Lch"},

	{"MIX2A Mixer", "BIVOL Lch", "STDIB Lch"},
	{"MIX2A Mixer", "BIVOL Rch", "STDIB Rch"},
	{"MIX2A Mixer", "BIVOL Lch + BIVOL Rch", "STDIB Lch"},
	{"MIX2A Mixer", "BIVOL Lch + BIVOL Rch", "STDIB Rch"},
	{"MIX2A Mixer", "(BIVOL Lch + BIVOL Rch)/2", "STDIB Lch"},
	{"MIX2A Mixer", "(BIVOL Lch + BIVOL Rch)/2", "STDIB Rch"},

	{"MIX2B Mixer", "DATT-A Lch", "EQ"},
	{"MIX2B Mixer", "DATT-A Rch", "EQ"},
	{"MIX2B Mixer", "DATT-A Lch + DATT-A Rch", "EQ"},
	{"MIX2B Mixer", "(DATT-A Lch + DATT-A Rch)/2", "EQ"},

	{"MIX2C Mixer", "MIX2A", "MIX2A Mixer"},
	{"MIX2C Mixer", "MIX2B", "MIX2B Mixer"},
	{"MIX2C Mixer", "MIX2A + MIX2B", "MIX2A Mixer"},
	{"MIX2C Mixer", "MIX2A + MIX2B", "MIX2B Mixer"},
	{"MIX2C Mixer", "(MIX2A + MIX2B)/2", "MIX2A Mixer"},
	{"MIX2C Mixer", "(MIX2A + MIX2B)/2", "MIX2B Mixer"},

	{"MIX3 Mixer", "DATT-A stereo", "EQ"},
	{"MIX3 Mixer", "DATT-A Mono Lch", "EQ"},
	{"MIX3 Mixer", "DATT-A Mono Rch", "EQ"},
	{"MIX3 Mixer", "DATT-A Lch + DATT-A Rch", "EQ"},
	{"MIX3 Mixer", "(DATT-A Lch+ DATT-A Rch)/2", "EQ"},
	{"MIX3 Mixer", "DATT-A Lch + DATT-A Rch + DATT-C", "EQ"},
	{"MIX3 Mixer", "DATT-A Lch + DATT-A Rch + DATT-C", "SBMX"},
	{"MIX3 Mixer", "PFSDO", "PFSDO"},
	{"MIX3 Mixer", "DATT-C", "SBMX"},

	{"SBMX", "STDIA", "STDIA"},
	{"SBMX", "SVOLB", "MIX2C Mixer"},
	{"SBMX", "STDIA + SVOLB", "STDIA"},
	{"SBMX", "STDIA + SVOLB", "MIX2C Mixer"},

	{"SDTOB Capture", "Switch", "MIX3 Mixer"},

	{"SDTOA Capture", "Switch", "MIX2C Mixer"},
	{"SDTOA", NULL, "SDTOA Capture"},

	{"SDTOB Lch", NULL, "SDTOB Capture"},
	{"SDTOB Rch", NULL, "SDTOB Capture"},

	{"STDIB Lch", NULL, "PMPCMB"},
	{"STDIB Rch", NULL, "PMPCMB"},
	{"SDTOB Lch", NULL, "PMPCMB"},
	{"SDTOB Rch", NULL, "PMPCMB"},

	{"PMPCMB", NULL, "PMOSC"},

	{"STDIA", NULL, "PMPCMA"},
	{"SDTOA", NULL, "PMPCMA"},

	{"PMPCMA", NULL, "PMOSC"},
};

static int set_fschgpara(struct snd_soc_codec *codec, int fsno)
{
	u16 i, addr, nWtm;

	gprintk("\n");

	addr = AK4678_29_FIL1_COEFFICIENT0;
	for (i = 0; i < 4; i++) {
		snd_soc_write(codec, addr, hpf2[fsno][i]);
		addr++;
	}

	addr = AK4678_A0_DVLCL_LPF_CO_EFFICIENT_0;
	for (i = 0; i < 16; i++) {
		snd_soc_write(codec, addr, fil3band[fsno][i]);
		addr++;
	}

	addr = AK4678_76_NS_LPF_CO_EFFICIENT_0;
	for (i = 0; i < 8; i++) {
		snd_soc_write(codec, addr, fil2ns[fsno][i]);
		addr++;
	}

	switch (fsno) {
	case 0:
		nWtm = AK4678_WTM_FS11;
		break;
	case 1:
		nWtm = AK4678_WTM_FS22;
		break;
	case 2:
	default:
		nWtm = AK4678_WTM_FS44;
		break;
	}

	snd_soc_update_bits(codec, AK4678_15_ALCTIMER_SELECT, AK4678_WTM, nWtm);

	return (0);
}

static int ak4678_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ak4678_priv *ak4678 = snd_soc_codec_get_drvdata(codec);
	u8 fs;
	u8 mode = 0;
	u16 fsno2 = ak4678->fsno;

	fs = snd_soc_read(codec, AK4678_03_PLL_MODE_SELECT0);
	fs &= ~AK4678_FS;


	gprintk("rate = %u\n", params_rate(params) );
	switch (params_rate(params)) {
	case 8000:
		fs |= AK4678_FS_8KHZ;
		break;
	case 12000:
		fs |= AK4678_FS_12KHZ;
		break;
	case 16000:
		fs |= AK4678_FS_16KHZ;
		break;
	case 24000:
		fs |= AK4678_FS_24KHZ;
		break;
	case 11025:
		fs |= AK4678_FS_11_025KHZ;
		break;
	case 22050:
		fs |= AK4678_FS_22_05KHZ;
		break;
	case 32000:
		fs |= AK4678_FS_32KHZ;
		break;
	case 44100:
		fs |= AK4678_FS_44_1KHZ;
		break;
	case 48000:
		fs |= AK4678_FS_48KHZ;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, AK4678_03_PLL_MODE_SELECT0, fs);

	if (ak4678->ext_clk_mode == 1) {
		gprintk("ext_clk_mode = 1\n");
		mode = snd_soc_read(codec, AK4678_04_PLL_MODE_SELECT1);
		mode &= ~AK4678_PLL_MODE_SELECT_1_CM;

		if (params_rate(params) <= AK4678_FS_MIDDLE) {
			fsno2 = 1;
			mode |= AK4678_PLL_MODE_SELECT_1_CM;
		} else {
			fsno2 = 2;
			mode |= AK4678_PLL_MODE_SELECT_1_CM0;
		}

		snd_soc_write(codec, AK4678_04_PLL_MODE_SELECT1, mode);
	}

	if (fsno2 != ak4678->fsno) {
		ak4678->fsno = fsno2;
		set_fschgpara(codec, fsno2);
	}

	return 0;
}

static int ak4678_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
				 unsigned int freq, int dir)
{
	gprintk("\n");
	return 0;
}

static int ak4678_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 mode;
	u8 format;


	/* set master/slave audio interface */
	mode = snd_soc_read(codec, AK4678_04_PLL_MODE_SELECT1);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		gprintk("Slave\n");
		mode &= ~(AK4678_M_S);
		mode &= ~(AK4678_BCKO);
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		gprintk("Master\n");
		mode |= (AK4678_M_S);
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
	default:
		dev_err(codec->dev, "Clock mode unsupported");
		return -EINVAL;
	}

	format = snd_soc_read(codec, AK4678_05_FORMAT_SELECT);
	format &= ~AK4678_DIF;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		gprintk("I2S mode\n");
		format |= AK4678_DIF_I2S_MODE;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		gprintk("Left J mode\n");
		format |= AK4678_DIF_MSB_MODE;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		gprintk("DSP A mode\n");
		format |= AK4678_DIF_DSP_MODE;
		format |= AK4678_BCKP;
		format |= AK4678_MSBS;
		if ((fmt & SND_SOC_DAIFMT_INV_MASK) == SND_SOC_DAIFMT_IB_NF)
			format &= ~AK4678_BCKP;
		break;
	default:
		return -EINVAL;
	}

	/* set mode and format */
	snd_soc_write(codec, AK4678_05_FORMAT_SELECT, format);
	snd_soc_write(codec, AK4678_04_PLL_MODE_SELECT1, mode);

	return 0;
}

static int ak4678_set_dai_pll(struct snd_soc_dai *dai, int pll_id, int source,
			      unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ak4678_priv *ak4678 = snd_soc_codec_get_drvdata(codec);
	u8 ret = 0;
	u8 pll = 0;
	u8 delay = 0;
	u8 mode = 0;


	if (freq_out == AK4678_BICKO_64FS)
		mode |= AK4678_PLL_MODE_SELECT_1_BCKO;

	if (pll_id == AK4678_PLL_SLAVE || pll_id == AK4678_PLL_MASTER) {
		mode |= AK4678_PLL_MODE_SELECT_1_PMPLL;
		ak4678->pllmode = 1;
		gprintk("pll_id = %s, pll_mode\n",
			(pll_id ==
			 AK4678_PLL_SLAVE) ? "AK4678_PLL_SLAVE" :
			"AK4678_PLL_MASTER");
	} else if (pll_id == AK4678_EXT_SLAVE || pll_id == AK4678_EXT_MASTER) {
		mode &= ~AK4678_PLL_MODE_SELECT_1_PMPLL;
		ak4678->ext_clk_mode = 1;
		snd_soc_update_bits(codec, AK4678_04_PLL_MODE_SELECT1,
				    AK4678_PLL_MODE_SELECT_1_PMPLL |
				    AK4678_PLL_MODE_SELECT_1_BCKO, mode);
		gprintk("pll_id = %s, ext_mode\n",
			(pll_id == AK4678_EXT_SLAVE) ? "AK4678_EXT_SLAVE" : "AK4678_EXT_MASTER");
		return 0;
	} else {
		return -EINVAL;
	}

	switch (source) {
	case AK4678_PLL_BICK32:
		pll |= AK4678_PLL_BICK32;
		delay = 2;
		break;
	case AK4678_PLL_BICK64:
		pll |= AK4678_PLL_BICK64;
		delay = 2;
		break;
	case AK4678_PLL_11_2896MHZ:
		pll |= AK4678_PLL_11_2896MHZ;
		delay = 10;
		break;
	case AK4678_PLL_12_288MHZ:
		pll |= AK4678_PLL_12_288MHZ;
		delay = 10;
		break;
	case AK4678_PLL_12MHZ:
		pll |= AK4678_PLL_12MHZ;
		delay = 10;
		break;
	case AK4678_PLL_24MHZ:
		gprintk("24Mhz mode.....\n");
		pll |= AK4678_PLL_24MHZ;
		delay = 10;
		break;
	case AK4678_PLL_19_2MHZ:
		pll |= AK4678_PLL_19_2MHZ;
		delay = 10;
		break;
	case AK4678_PLL_13_5MHZ:
		pll |= AK4678_PLL_13_5MHZ;
		delay = 10;
		break;
	case AK4678_PLL_27MHZ:
		pll |= AK4678_PLL_27MHZ;
		delay = 10;
		break;
	case AK4678_PLL_13MHZ:
		pll |= AK4678_PLL_13MHZ;
		delay = 10;
		break;
	case AK4678_PLL_26MHZ:
		pll |= AK4678_PLL_26MHZ;
		delay = 10;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, AK4678_04_PLL_MODE_SELECT1,
			    AK4678_PLL_MODE_SELECT_1_PMPLL |
			    AK4678_PLL_MODE_SELECT_1_BCKO, mode);
	snd_soc_update_bits(codec, AK4678_03_PLL_MODE_SELECT0, AK4678_PLL, pll);
	mdelay(delay);

	return ret;
}

static int ak4678_set_dai_clkdiv(struct snd_soc_dai *dai, int div_id, int div)
{
	u8 ret = 0;

	gprintk("\n");

	return ret;
}

static int ak4678_hw_free(struct snd_pcm_substream *substream,
			  struct snd_soc_dai *dai)
{
	u8 ret = 0;
	gprintk("\n");
	return ret;
}

void ak4678_set_dai_shutdown(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	gprintk("\n");
}

static int ak4678_set_prepare(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	int ret = 0;
	gprintk("\n");
	return ret;
}

static int ak4678_set_dai_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	int ret = 0;
	struct snd_soc_codec *codec = dai->codec;

	ret = snd_soc_read(codec, AK4678_00_POWER_MANAGEMENT0);
	gprintk("reg(0x00) = value(0x%x)\n", ret);

	ret = snd_soc_write(codec, AK4678_00_POWER_MANAGEMENT0, AK4678_PMVCM);

	gprintk("ret = %d\n", ret);

	return ret;
}

static int ak4678_trigger(struct snd_pcm_substream *substream, int cmd,
			  struct snd_soc_dai *dai)
{
	int ret = 0;

	gprintk("cmd = %d\n", cmd);
	return ret;
}

static int ak4678_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	u8 reg;
	struct ak4678_priv *ak4678 = snd_soc_codec_get_drvdata(codec);

	reg = snd_soc_read(codec, AK4678_00_POWER_MANAGEMENT0);

	gprintk("BIAS LEVLE =%d\n", level);

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (ak4678->pllmode == 1)
			snd_soc_update_bits(codec, AK4678_04_PLL_MODE_SELECT1,
					    0x01, 0x0);

		snd_soc_write(codec, AK4678_00_POWER_MANAGEMENT0, 0x01);
		break;
	case SND_SOC_BIAS_OFF:
		if (ak4678->pllmode == 1)
			snd_soc_update_bits(codec, AK4678_04_PLL_MODE_SELECT1,
					    0x01, 0x0);

		snd_soc_write(codec, AK4678_00_POWER_MANAGEMENT0, 0x00);
		break;
	}

	//gprintk("BIAS  reg(0x00) =0x%x\n", snd_soc_read(codec, AK4678_00_POWER_MANAGEMENT0));
	//gprintk("BIAS  reg(0x04) =0x%x\n", snd_soc_read(codec, AK4678_04_PLL_MODE_SELECT1));

	codec->dapm.bias_level = level;

	return 0;
}

static int ak4678_set_dai_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret = 0;
	u32 mute_reg;

	gprintk("mute[%s]\n", mute ? "ON" : "OFF");

	mute_reg = snd_soc_read(codec, AK4678_18_MODE1_CONTROL) & 0x4;

	if (mute_reg == mute)
		return 0;

	if (mute)
		ret |= snd_soc_update_bits(codec, AK4678_18_MODE1_CONTROL, 0x4,	0x4);
	else
		ret |= snd_soc_update_bits(codec, AK4678_18_MODE1_CONTROL, 0x4, 0);

	return ret;
}

#define AK4678_RATES		(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
		SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |\
		SNDRV_PCM_RATE_48000)

#define AK4678_FORMATS		SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE

static struct snd_soc_dai_ops ak4678_dai_ops = {
	.hw_params = ak4678_hw_params,
	.set_sysclk = ak4678_set_dai_sysclk,
	.set_fmt = ak4678_set_dai_fmt,
	.trigger = ak4678_trigger,
	.set_clkdiv = ak4678_set_dai_clkdiv,
	.set_pll = ak4678_set_dai_pll,
	.hw_free = ak4678_hw_free,
	.shutdown = ak4678_set_dai_shutdown,
	.startup = ak4678_set_dai_startup,
	.digital_mute = ak4678_set_dai_mute,
	.prepare = ak4678_set_prepare,
};

static int ak4678_set_portA_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret = 0;

	gprintk("\n");

	ret = ak4678_set_port_dai_fmt(codec, dai->id, fmt);

	return ret;
}

static int ak4678_portA_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 mode;
	u8 reg;

	gprintk("\n");

	switch (dai->id) {
	case 1:
		reg = AK4678_20_PCM_IF_CONTROL0;
		break;
	case 2:
		reg = AK4678_21_PCM_IF_CONTROL1;
		break;
	default:
		return -EINVAL;
	}

	mode = snd_soc_read(codec, reg);
	mode &= ~0xc;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_MU_LAW:
		mode |= 0xc;
		break;
	case SNDRV_PCM_FORMAT_A_LAW:
		mode |= 0x8;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, reg, mode);

	snd_soc_update_bits(codec, 0x05, 0x3, 0x3);
	snd_soc_update_bits(codec, 0x03, 0xF0, 0xB0);

	return 0;
}

static struct snd_soc_dai_ops ak4678_portA_dai_ops = {
	.hw_params = ak4678_portA_hw_params,
	.set_fmt = ak4678_set_portA_dai_fmt,
	.set_pll = ak4678_set_dai_pll,
};

struct snd_soc_dai_driver ak4678_dai[] = {
	{
	 .name = "ak4678-hifi",
	 .id = AK4678_PORTIIS,
	 .playback = {
		      .stream_name = "Playback",
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = AK4678_RATES,
		      .formats = AK4678_FORMATS,
		      },
	 .capture = {
		     .stream_name = "Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = AK4678_RATES,
		     .formats = AK4678_FORMATS,
		     },
	 .ops = &ak4678_dai_ops,
	 .symmetric_rates = 1,
	 },
	{
	 .name = "ak4678-pcm-a",
	 .id = AK4678_PORTA,
	 .playback = {
		      .stream_name = "Playback",
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		      .formats =
		      SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_MU_LAW |
		      SNDRV_PCM_FMTBIT_A_LAW,
		      },
	 .capture = {
		     .stream_name = "Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		     .formats =
		     SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_MU_LAW |
		     SNDRV_PCM_FMTBIT_A_LAW,
		     },
	 .ops = &ak4678_portA_dai_ops,
	 },
	{
	 .name = "ak4678-pcm-b",
	 .id = AK4678_PORTB,
	 .playback = {
		      .stream_name = "Playback",
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_48000,
		      .formats =
		      SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_MU_LAW |
		      SNDRV_PCM_FMTBIT_A_LAW,
		      },
	 .capture = {
		     .stream_name = "Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_48000,
		     .formats =
		     SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_MU_LAW |
		     SNDRV_PCM_FMTBIT_A_LAW,
		     },
	 .ops = &ak4678_portA_dai_ops,
	 },
};

static int ak4678_write_cache_reg(struct snd_soc_codec *codec, u16 regs,
				  u16 rege)
{
	u32 reg, cache_data;
	reg = regs;

	do {
		cache_data = ak4678_read_reg_cache(codec, reg);
		snd_soc_write(codec, reg, cache_data);
		reg++;
	} while (reg <= rege);

	return (0);
}

static int ak4678_set_reg_digital_effect(struct snd_soc_codec *codec)
{
	ak4678_write_cache_reg(codec, AK4678_13_ALCREF_SELECT,
			       AK4678_16_ALCMODE_CONTROL);
	ak4678_write_cache_reg(codec, AK4678_29_FIL1_COEFFICIENT0,
			       AK4678_3A_EQ_COEFFICIENT5);
	ak4678_write_cache_reg(codec, AK4678_50_5BAND_E1_COEF0,
			       AK4678_69_5BAND_E5_COEF3);
	ak4678_write_cache_reg(codec, AK4678_76_NS_LPF_CO_EFFICIENT_0,
			       AK4678_7D_NS_HPF_CO_EFFICIENT_3);
	ak4678_write_cache_reg(codec, AK4678_82_DVLCL_CURVE_X1,
			       AK4678_AF_DVLCH_HPF_CO_EFFICIENT_3);

	return (0);
}

int ak4678_reset(struct ak4678_priv *ak4678)
{
#ifdef GHC_USE_PDN
	int ret = 0;

	gprintk("p->pdn = %u\n", ak4678->pdn);

	ret = gpio_request(ak4678->pdn, "PDNA");
	if (ret) {
		printk("%s: failed to request reset gpio PDNA\n", __func__);
		return -EBUSY;
	}

	ret = gpio_direction_output(ak4678->pdn, 1);
	if (ret < 0) {
		printk("Unable to set PDNA Pin output\n");
		gpio_free(ak4678->pdn);
		return -ENODEV;
	}

	gpio_set_value(ak4678->pdn, 0);
	/* Wait for at least t>1.5 us */
	udelay(10);

	/* Drive PDNA pin High. Note: PDNA shud be kept high
	   during CODEC normal operation */
	gpio_set_value(ak4678->pdn, 1);
#endif

	return 0;
}

static int ak4678_probe(struct snd_soc_codec *codec)
{
	struct ak4678_priv *ak4678 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	codec->cache_only = 0;

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_I2C);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	ret = ak4678_reset(ak4678);
	if (ret < 0) {
		printk("ak4678 reset fail\n");
		return ret;
	}


	ak4678_codec = codec;

	/*  Dummy command */
	ak4678_write(codec, AK4678_00_POWER_MANAGEMENT0, 0x0);
	ak4678_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* Set SPK VOLUME 3db , default 0db(0x10 == 0xB) */
	snd_soc_write(codec, AK4678_10_SPRC_VOLUME, 0xc);

	/* set mic gain 12 dB */
	snd_soc_write(codec, AK4678_07_MIC_AMP_GAIN, 0x99);

	ak4678_set_reg_digital_effect(codec);

	ak4678->ext_clk_mode = 0;
	ak4678->stereo_ef_on = 0;
	ak4678->dvlc_drc_on = 0;
	ak4678->fsno = 2;
	ak4678->pllmode = 0;

	return ret;
}

static int ak4678_remove(struct snd_soc_codec *codec)
{
#ifdef GHC_USE_PDN
	struct ak4678_priv *ak4678 = snd_soc_codec_get_drvdata(codec);
#endif
	gprintk("\n");

	ak4678_set_bias_level(codec, SND_SOC_BIAS_OFF);
#ifdef GHC_USE_PDN
	gpio_set_value(ak4678->pdn, 0);
	gpio_free(ak4678->pdn);
#endif

	return 0;
}


#ifdef CONFIG_PM
static int ak4678_suspend(struct device *dev)
{
	struct ak4678_priv *ak4678 = dev_get_drvdata(dev);
	struct snd_soc_codec *codec = ak4678_codec;
	struct snd_soc_card *card = codec->card;
	struct snd_soc_pcm_runtime *rtd = card->rtd;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	gprintk("enter\n");

	if (ak4678->suspended) {
		printk("Already in suspend state\n");
		return 0;
	}
	ret = ak4678_set_dai_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_CBS_CFS);
	if(ret <0)
		gprintk("CBS_CFS mode change is failed: %d\n", ret);

#ifdef GHC_USE_PDN
	gpio_set_value(ak4678->pdn, 0);
#endif

	ak4678->suspended = true;

	return 0;
}

static int ak4678_resume(struct device *dev)
{
	struct snd_soc_codec *codec = ak4678_codec;
	struct ak4678_priv *ak4678 = snd_soc_codec_get_drvdata(codec);

#ifdef GHC_USE_PDN

	gpio_set_value(ak4678->pdn, 1);
#endif
	gprintk("reg %x = value%x\n", AK4678_0C_CP_CONTROL, snd_soc_read(codec, AK4678_0C_CP_CONTROL));

	ak4678_set_reg_digital_effect(codec);

	codec->cache_sync = 1;
	snd_soc_cache_sync(codec);
	gprintk("reg %x = value%x\n", AK4678_0C_CP_CONTROL, snd_soc_read(codec, AK4678_0C_CP_CONTROL));
	ak4678_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	ak4678->suspended = false;
	gprintk("leave\n");

	return 0;
}


static const struct dev_pm_ops ak4678_pm_ops = {
	.suspend = ak4678_suspend,
	.resume = ak4678_resume,
};
#else
static int ak4678_suspend(struct device *dev)
{
	return 0;
}

static int ak4678_resume(struct device *dev)
{
	return 0;
}
#endif

static struct snd_soc_codec_driver soc_codec_dev_ak4678 = {
	.probe = ak4678_probe,
	.remove = ak4678_remove,
	.set_bias_level = ak4678_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(ak4678_reg),
	.reg_word_size = sizeof(u8),
	.reg_cache_default = ak4678_reg,
	.controls = ak4678_snd_controls,
	.num_controls = ARRAY_SIZE(ak4678_snd_controls),
	.dapm_widgets = ak4678_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(ak4678_dapm_widgets),
	.dapm_routes = audio_route,
	.num_dapm_routes = ARRAY_SIZE(audio_route),
};


#ifdef CONFIG_OF
static int ak4678_i2c_parse_dt(struct i2c_client *i2c, struct ak4678_priv *p)
{
	struct device *dev = &i2c->dev;
	struct device_node *np = dev->of_node;

	if (!np)
		return -1;

	p->pdn = of_get_named_gpio(np, "ak4678,pdn-gpio", 0);
	if (p->pdn < 0) {
		printk("Looking up %s property in node %s failed %d\n",
				"ak4678,pdn-gpio", dev->of_node->full_name,
				p->pdn);
		p->pdn = -1;

		return -1;
	}

	if( !gpio_is_valid(p->pdn) ) {
		printk(KERN_ERR "ak4678 pdn pin(%u) is invalid\n", p->pdn);
		return -1;
	}

	gprintk("p->pdn = %u\n", p->pdn);

	return 0;
}
#else
static int ak4678_i2c_parse_dt(struct i2c_client *i2c, struct ak4678_priv *p)
{
	return NULL;
}
#endif


static int ak4678_i2c_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	struct ak4678_priv *ak4678;
	int ret;

	gprintk("\n");

	ak4678 = kzalloc(sizeof(struct ak4678_priv), GFP_KERNEL);
	if (ak4678 == NULL)
		return -ENOMEM;


	if (client->dev.of_node) {
		gprintk("Read PDN pin from device tree\n");
		ret = ak4678_i2c_parse_dt(client, ak4678);
		if( ret < 0 ) {
			printk("ak4678 PDN pin error\n");
		}
	}

	ak4678->suspended = false;


	i2c_set_clientdata(client, ak4678);

	ret = snd_soc_register_codec(&client->dev,
				     &soc_codec_dev_ak4678, ak4678_dai,
				     ARRAY_SIZE(ak4678_dai));
	if (ret < 0) {
		kfree(ak4678);
		printk("%s: fail (%d)\n", __FUNCTION__, __LINE__);
	}

	gprintk("success\n");

	return ret;
}

static int ak4678_i2c_remove(struct i2c_client *client)
{
	struct ak4678_priv *ak4678 = i2c_get_clientdata(client);

	gprintk("\n");

	snd_soc_unregister_codec(&client->dev);
	kfree(ak4678);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id ak4678_i2c_dt_ids[] = {
	      { .compatible = "akm,ak4678"},
		        { }
};
#endif

static const struct i2c_device_id ak4678_i2c_id[] = {
	{"ak4678", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ak4678_i2c_id);

static struct i2c_driver ak4678_i2c_driver = {
	.driver = {
		.name = "ak4678-codec",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(ak4678_i2c_dt_ids),
#endif


#ifdef CONFIG_PM
	   .pm = &ak4678_pm_ops,
#endif

		},
	.probe = ak4678_i2c_probe,
	.remove = ak4678_i2c_remove,
	.id_table = ak4678_i2c_id,
};

static int __init ak4678_modinit(void)
{
	int ret;

	gprintk("\n");
	ret = i2c_add_driver(&ak4678_i2c_driver);
	if (ret)
		pr_err("Failed to register ak4678 I2C driver: %d\n", ret);

	return ret;
}

module_init(ak4678_modinit);

static void __exit ak4678_exit(void)
{
	i2c_del_driver(&ak4678_i2c_driver);
}

module_exit(ak4678_exit);

MODULE_DESCRIPTION("ASoC ak4678 codec driver");
MODULE_AUTHOR("AKM Corporation");
MODULE_LICENSE("GPL");
