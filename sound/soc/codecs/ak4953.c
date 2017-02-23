/*
 * ak4953.c  --  audio driver for AK4953A
 *
 * Copyright (C) 2013 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                      13/05/27	    2.0
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/of_gpio.h>

#include "ak4953.h"

#define PLL_32BICK_MODE

#ifdef AK4953A_DEBUG
#define akdbgprt printk
#else
#define akdbgprt(format, arg...) do {} while (0)
#endif

#define LINEIN1_MIC_BIAS_CONNECT
#define LINEIN2_MIC_BIAS_CONNECT

/* AK4953A Codec Private Data */
struct ak4953_priv {
	struct snd_soc_codec codec;
	u8 reg_cache[AK4953A_MAX_REGISTERS];
	int onDrc;
	int onStereo;
	int pdn;
};

static struct snd_soc_codec *ak4953_codec;
static struct ak4953_priv *ak4953_data;

/* ak4953 register cache & default register settings */
static const u8 ak4953_reg[AK4953A_MAX_REGISTERS] = {
	0x00,	/*	0x00	AK4953A_00_POWER_MANAGEMENT1	*/
	0x00,	/*	0x01	AK4953A_01_POWER_MANAGEMENT2	*/
	0x03,	/*	0x02	AK4953A_02_SIGNAL_SELECT1	*/
	0x00,	/*	0x03	AK4953A_03_SIGNAL_SELECT2	*/
	0x10,	/*	0x04	AK4953A_04_SIGNAL_SELECT3	*/
	0x02,	/*	0x05	AK4953A_05_MODE_CONTROL1	*/
	0x00,	/*	0x06	AK4953A_06_MODE_CONTROL2	*/
	0x1D,	/*	0x07	AK4953A_07_MODE_CONTROL3		*/
	0x00,	/*	0x08	AK4953A_08_DIGITL_MIC	*/
	0x01,	/*	0x09	AK4953A_09_TIMER_SELECT	*/
	0x30,	/*	0x0A	AK4953A_0A_ALC_TIMER_SELECT	*/
	0x81,	/*	0x0B	AK4953A_0B_ALC_MODE_CONTROL1	*/
	0xE1,	/*	0x0C	AK4953A_0C_ALC_MODE_CONTROL2	*/
	0x28,	/*	0x0D	AK4953A_0D_ALC_MODE_CONTROL3	*/
	0x91,	/*	0x0E	AK4953A_0E_ALC_VOLUME	*/
	0x91,	/*	0x0F	AK4953A_0F_LCH_INPUT_VOLUME_CONTROL	*/
	0x91,	/*	0x10	AK4953A_10_RCH_INPUT_VOLUME_CONTROL	*/
	0x91,	/*	0x11	AK4953A_11_LCH_OUTPUT_VOLUME_CONTROL	*/
	0x91,	/*	0x12	AK4953A_12_RCH_OUTPUT_VOLUME_CONTROL	*/
	0x18,	/*	0x13	AK4953A_13_LCH_DIGITAL_VOLUME_CONTROL	*/
	0x18,	/*	0x14	AK4953A_14_RCH_DIGITAL_VOLUME_CONTROL	*/
	0x00,	/*	0x15	AK4953A_15_BEEP_FREQUENCY	*/
	0x00,	/*	0x16	AK4953A_16_BEEP_ON_TIMEL	*/
	0x00,	/*	0x17	AK4953A_17_BEEP_OFF_TIME	*/
	0x00,	/*	0x18	AK4953A_18_BEEP_REPEAT_COUNT	*/
	0x00,	/*	0x19	AK4953A_19_BEEP_VOLUME_CONTROL	*/
	0x00,	/*	0x1A	AK4953A_1A_RESERVED	*/
	0x00,	/*	0x1B	AK4953A_1B_RESERVED	*/
	0x01,	/*	0x1C	AK4953A_1C_DIGITAL_FILTER_SELECT1	*/
	0x03,	/*	0x1D	AK4953A_1D_DIGITAL_FILTER_MODE	*/
	0xA9,	/*	0x1E	AK4953A_1E_HPF2_COEFFICIENT0	*/
	0x1F,	/*	0x1F	AK4953A_1F_HPF2_COEFFICIENT1	*/
	0xAD,	/*	0x20	AK4953A_20_HPF2_COEFFICIENT2	*/
	0x20,	/*	0x21	AK4953A_21_HPF2_COEFFICIENT3	*/
	0x7F,	/*	0x22	AK4953A_22_LPF_COEFFICIENT0	*/
	0x0C,	/*	0x23	AK4953A_23_LPF_COEFFICIENT1	*/
	0xFF,	/*	0x24	AK4953A_24_LPF_COEFFICIENT2	*/
	0x38,	/*	0x25	AK4953A_25_LPF_COEFFICIENT3	*/
	0x00,	/*	0x26	AK4953A_26_RESERVED	*/
	0x00,	/*	0x27	AK4953A_27_RESERVED	*/
	0x00,	/*	0x28	AK4953A_28_RESERVED	*/
	0x00,	/*	0x29	AK4953A_29_RESERVED	*/
	0x00,	/*	0x2A	AK4953A_2A_RESERVED	*/
	0x00,	/*	0x2B	AK4953A_2B_RESERVED	*/
	0x00,	/*	0x2C	AK4953A_2C_RESERVED	*/
	0x00,	/*	0x2D	AK4953A_2D_RESERVED	*/
	0x00,	/*	0x2E	AK4953A_2E_RESERVED	*/
	0x00,	/*	0x2F	AK4953A_2F_RESERVED	*/
	0x00,	/*	0x30	AK4953A_30_DIGITAL_FILTER_SELECT2	*/
	0x00,	/*	0x31	AK4953A_31_RESERVED	*/
	0x68,	/*	0x32	AK4953A_32_E1_COEFFICIENT0	*/
	0x00,	/*	0x33	AK4953A_33_E1_COEFFICIENT1	*/
	0x4F,	/*	0x34	AK4953A_34_E1_COEFFICIENT2	*/
	0x3F,	/*	0x35	AK4953A_35_E1_COEFFICIENT3	*/
	0xAD,	/*	0x36	AK4953A_36_E1_COEFFICIENT4	*/
	0xE0,	/*	0x37	AK4953A_37_E1_COEFFICIENT5	*/
	0x73,	/*	0x38	AK4953A_38_E2_COEFFICIENT0	*/
	0x00,	/*	0x39	AK4953A_39_E2_COEFFICIENT1	*/
	0x0B,	/*	0x3A	AK4953A_3A_E2_COEFFICIENT2	*/
	0x3F,	/*	0x3B	AK4953A_3B_E2_COEFFICIENT3	*/
	0xE6,	/*	0x3C	AK4953A_3C_E2_COEFFICIENT4	*/
	0xE0,	/*	0x3D	AK4953A_3D_E2_COEFFICIENT5	*/
	0x07,	/*	0x3E	AK4953A_3E_E3_COEFFICIENT0	*/
	0x08,	/*	0x3F	AK4953A_3F_E3_COEFFICIENT1	*/
	0x67,	/*	0x40	AK4953A_40_E3_COEFFICIENT2	*/
	0x37,	/*	0x41	AK4953A_41_E3_COEFFICIENT3	*/
	0x07,	/*	0x42	AK4953A_42_E3_COEFFICIENT4	*/
	0xE8,	/*	0x43	AK4953A_43_E3_COEFFICIENT5	*/
	0xE0,	/*	0x44	AK4953A_44_E4_COEFFICIENT0	*/
	0x0A,	/*	0x45	AK4953A_45_E4_COEFFICIENT1	*/
	0xAD,	/*	0x46	AK4953A_46_E4_COEFFICIENT2	*/
	0x29,	/*	0x47	AK4953A_47_E4_COEFFICIENT3	*/
	0x80,	/*	0x48	AK4953A_48_E4_COEFFICIENT4	*/
	0xEE,	/*	0x49	AK4953A_49_E4_COEFFICIENT5	*/
	0x04,	/*	0x4A	AK4953A_4A_E5_COEFFICIENT0	*/
	0x0C,	/*	0x4B	AK4953A_4B_E5_COEFFICIENT1	*/
	0x2B,	/*	0x4C	AK4953A_4C_E5_COEFFICIENT2	*/
	0x15,	/*	0x4D	AK4953A_4D_E5_COEFFICIENT3	*/
	0x07,	/*	0x4E	AK4953A_4E_E5_COEFFICIENT4	*/
	0xF4,	/*	0x4F	AK4953A_4F_E5_COEFFICIENT5	*/
};

static const struct {
	int readable;   /* Mask of readable bits */
	int writable;   /* Mask of writable bits */
} ak4953_access_masks[] = {
	{ 0xFF, 0xFF },
	{ 0x3B, 0x3B },
	{ 0xBF, 0xBF },
	{ 0xCF, 0xCF },
	{ 0x3C, 0x3C },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0x3B, 0x3B },
	{ 0xC3, 0xC3 },
	{ 0x7F, 0x7F },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0x00 },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0x83, 0x83 },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0x7F, 0x7F },
	{ 0x9F, 0x9F },
	{ 0x00, 0x00 },
	{ 0x00, 0x00 },
	{ 0x37, 0x37 },
	{ 0x07, 0x07 },
	{ 0xFF, 0xFF },
	{ 0x3F, 0x3F },
	{ 0xFF, 0xFF },
	{ 0x3F, 0x3F },
	{ 0xFF, 0xFF },
	{ 0x3F, 0x3F },
	{ 0xFF, 0xFF },
	{ 0x3F, 0x3F },
	{ 0x00, 0x00 },
	{ 0x00, 0x00 },
	{ 0x00, 0x00 },
	{ 0x00, 0x00 },
	{ 0x00, 0x00 },
	{ 0x00, 0x00 },
	{ 0x00, 0x00 },
	{ 0x00, 0x00 },
	{ 0x00, 0x00 },
	{ 0x00, 0x00 },
	{ 0x1F, 0x1F },
	{ 0x00, 0x00 },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF },
	{ 0xFF, 0xFF }
};

/*
 *  MIC Gain control:
* from 11 to 29 dB (dB scale specified with min/max values instead of step)
 */
static DECLARE_TLV_DB_MINMAX(mgain_tlv, 1100, 2900);

/*
 * Input Digital volume L control:
 * from -54.375 to 36 dB in 0.375 dB steps mute instead of -54.375 dB)
 */
static DECLARE_TLV_DB_SCALE(ivoll_tlv, -5437, 37, 1);

/*
 * Input Digital volume R control:
 * from -54.375 to 36 dB in 0.375 dB steps mute instead of -54.375 dB)
 */
static DECLARE_TLV_DB_SCALE(ivolr_tlv, -5437, 37, 1);

/*
* Output Digital volume L control (Manual mode):
 * (This can be used as Bluetooth I/F output volume)
 * from -57.5 to 6 dB in 0.5 dB steps (mute instead of -57.5 dB)
 */
static DECLARE_TLV_DB_SCALE(dvoll1_tlv, -5437, 37, 1);

/*
* Output Digital volume R control (Manual mode):
 * (This can be used as Bluetooth I/F output volume)
 * from -57.5 to 6 dB in 0.5 dB steps (mute instead of -57.5 dB)
 */
static DECLARE_TLV_DB_SCALE(dvolr1_tlv, -5437, 37, 1);

/*
 * Output Digital volume L control:
 * (This can be used as Bluetooth I/F output volume)
 * from -57.5 to 6 dB in 0.5 dB steps (mute instead of -57.5 dB)
 */
static DECLARE_TLV_DB_SCALE(dvoll2_tlv, -11550, 50, 1);

/*
 * Output Digital volume R control:
 * (This can be used as Bluetooth I/F output volume)
 * from -57.5 to 6 dB in 0.5 dB steps (mute instead of -57.5 dB)
 */
static DECLARE_TLV_DB_SCALE(dvolr2_tlv, -11550, 50, 1);

/*
 * Speaker output volume control:
 * from -5.3 to 11.3 dB in 2 dB steps (mute instead of -33 dB)
 */
static DECLARE_TLV_DB_SCALE(spkout_tlv, 530, 200, 0);

static const char *ak4953_dem_select_texts[] =
		{"44.1kHz", "OFF", "48kHz", "32kHz"};

static const struct soc_enum ak4953_enum[] =
{
	SOC_ENUM_SINGLE(AK4953A_07_MODE_CONTROL3, 0,
			ARRAY_SIZE(ak4953_dem_select_texts),
			ak4953_dem_select_texts),
};

#ifdef AK4953A_DEBUG
static const char *test_reg_select[] =
{
	"read AK4953A Reg 00:2F",
	"read AK4953A Reg 30:4F",
};

static const struct soc_enum ak4953_enum2[] =
{
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(test_reg_select), test_reg_select),
};

static int nTestRegNo = 0;

static int get_test_reg(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = nTestRegNo;

	return 0;
}

static int set_test_reg(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    i, value;
	int	   regs, rege;

	nTestRegNo = currMode;

	switch(nTestRegNo) {
		case 1:
			regs = 0x30;
			rege = 0x4F;
			break;
		default:
			regs = 0x00;
			rege = 0x2F;
			break;
	}

	for ( i = regs ; i <= rege ; i++ ){
		value = snd_soc_read(codec, i);
		printk("***AK4953A Addr,Reg=(%x, %x)\n", i, value);
	}

	return 0;
}
#endif /* End of AK4953A_DEBUG */

static int ak4953_writeMask(struct snd_soc_codec *, u16, u16, u16);
static inline u32 ak4953_read_reg_cache(struct snd_soc_codec *, u16);

static const struct snd_kcontrol_new ak4953_snd_controls[] = {
	SOC_SINGLE_TLV("Mic Gain Control",
			AK4953A_02_SIGNAL_SELECT1, 0, 0x06, 0, mgain_tlv),
	SOC_SINGLE_TLV("Input Digital Volume L",
			AK4953A_0F_LCH_INPUT_VOLUME_CONTROL, 0, 0xF1, 0,
			ivoll_tlv),
	SOC_SINGLE_TLV("Input Digital Volume R",
			AK4953A_10_RCH_INPUT_VOLUME_CONTROL, 0, 0xF1, 0,
			ivolr_tlv),
	SOC_SINGLE_TLV("Digital Output Volume1 L (Manual Mode)",
			AK4953A_11_LCH_OUTPUT_VOLUME_CONTROL, 0, 0xF1, 0,
			dvoll1_tlv),
	SOC_SINGLE_TLV("Digital Output Volume1 R (Manual Mode)",
			AK4953A_12_RCH_OUTPUT_VOLUME_CONTROL, 0, 0xF1, 0,
			dvolr1_tlv),
	SOC_SINGLE_TLV("Digital Output Volume2 L",
			AK4953A_13_LCH_DIGITAL_VOLUME_CONTROL, 0, 0xFF, 1,
			dvoll2_tlv),
	SOC_SINGLE_TLV("Digital Output Volume2 R",
			AK4953A_14_RCH_DIGITAL_VOLUME_CONTROL, 0, 0xFF, 1,
			dvolr2_tlv),
	SOC_SINGLE_TLV("Speaker Output Volume",
			AK4953A_03_SIGNAL_SELECT2, 6, 0x03, 0, spkout_tlv),

	SOC_SINGLE("Input Volume Dependent", AK4953A_07_MODE_CONTROL3,
			2, 1, 0),
	SOC_SINGLE("Digital Output Volume1 Dependent",
			AK4953A_07_MODE_CONTROL3, 3, 1, 0),
	SOC_SINGLE("Digital Output Volume2 Dependent",
			AK4953A_07_MODE_CONTROL3, 4, 1, 0),
	SOC_SINGLE("Headphone Monaural Output",
			AK4953A_04_SIGNAL_SELECT3, 2, 1, 0),
	SOC_SINGLE("High Path Filter 1", AK4953A_1C_DIGITAL_FILTER_SELECT1,
			1, 3, 0),
	SOC_SINGLE("High Path Filter 2", AK4953A_1C_DIGITAL_FILTER_SELECT1,
			4, 1, 0),
	SOC_SINGLE("Low Path Filter",    AK4953A_1C_DIGITAL_FILTER_SELECT1, 5,
			1, 0),
	SOC_SINGLE("5 Band Equalizer 1", AK4953A_30_DIGITAL_FILTER_SELECT2, 0,
			1, 0),
	SOC_SINGLE("5 Band Equalizer 2", AK4953A_30_DIGITAL_FILTER_SELECT2, 1,
			1, 0),
	SOC_SINGLE("5 Band Equalizer 3", AK4953A_30_DIGITAL_FILTER_SELECT2, 2,
			1, 0),
	SOC_SINGLE("5 Band Equalizer 4", AK4953A_30_DIGITAL_FILTER_SELECT2, 3,
			1, 0),
	SOC_SINGLE("5 Band Equalizer 5", AK4953A_30_DIGITAL_FILTER_SELECT2, 4,
			1, 0),
	SOC_SINGLE("Auto Level Control 1", AK4953A_0B_ALC_MODE_CONTROL1, 5, 1,
			0),
	SOC_SINGLE("Auto Level Control 2", AK4953A_0B_ALC_MODE_CONTROL1, 6, 1,
			0),
	SOC_ENUM("De-emphasis Control", ak4953_enum[0]),
	SOC_SINGLE("Soft Mute Control",  AK4953A_07_MODE_CONTROL3, 5, 1, 0),

	/* mic power supply control */
	SOC_SINGLE("MIC Power Supply", AK4953A_02_SIGNAL_SELECT1, 4, 1, 0),
	SOC_SINGLE("MIC Power Pin Control", AK4953A_02_SIGNAL_SELECT1, 5, 1,
			0),

#ifdef AK4953A_DEBUG
	SOC_ENUM_EXT("Reg Read", ak4953_enum2[0], get_test_reg, set_test_reg),
#endif

};

static const char *ak4953_adc1_select_texts[] =
		{"Stereo", "Mono"};

static const struct soc_enum ak4953_adc1_mux_enum =
	SOC_ENUM_SINGLE(0, 0, ARRAY_SIZE(ak4953_adc1_select_texts),
			ak4953_adc1_select_texts);

static const struct snd_kcontrol_new ak4953_adc1_mux_control =
	SOC_DAPM_ENUM_VIRT("ADC Switch1", ak4953_adc1_mux_enum);

static const char *ak4953_adc2_select_texts[] =
		{"Stereo", "Mono"};

static const struct soc_enum ak4953_adc2_mux_enum =
	SOC_ENUM_SINGLE(0, 0, ARRAY_SIZE(ak4953_adc2_select_texts),
			ak4953_adc2_select_texts);

static const struct snd_kcontrol_new ak4953_adc2_mux_control =
	SOC_DAPM_ENUM_VIRT("ADC Switch2", ak4953_adc2_mux_enum);

static const char * ak4953_line_texts[] = {
	"LIN1/RIN1", "LIN2/RIN2", "LIN3/RIN3"};

static const unsigned int ak4953_select_values[] = {
	0x00, 0x03, 0x0c }; /* CONFIG_LINF_DAPM */

static const struct soc_enum ak4953_input_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4953A_03_SIGNAL_SELECT2, 0, 0x0f,
			      ARRAY_SIZE(ak4953_line_texts),
			      ak4953_line_texts,
			      ak4953_select_values);

static const struct snd_kcontrol_new ak4953_input_select_controls =
	SOC_DAPM_VALUE_ENUM("Input Select", ak4953_input_mux_enum);

static const char *ak4953_in1_select_texts[] = {"IN1", "Mic Bias"};

static const struct soc_enum ak4953_in1_mux_enum =
	SOC_ENUM_SINGLE(0, 0, ARRAY_SIZE(ak4953_in1_select_texts),
			ak4953_in1_select_texts);

static const struct snd_kcontrol_new ak4953_in1_mux_control =
	SOC_DAPM_ENUM_VIRT("IN1 Switch", ak4953_in1_mux_enum);

static const char *ak4953_in2_select_texts[] = {"IN2", "Mic Bias"};

static const struct soc_enum ak4953_in2_mux_enum =
	SOC_ENUM_SINGLE(0, 0, ARRAY_SIZE(ak4953_in2_select_texts),
			ak4953_in2_select_texts);

static const struct snd_kcontrol_new ak4953_in2_mux_control =
	SOC_DAPM_ENUM_VIRT("IN2 Switch", ak4953_in2_mux_enum);

static const char *ak4953_micbias_select_texts[] = {"IN1", "IN2"};

static const struct soc_enum ak4953_micbias_mux_enum =
	SOC_ENUM_SINGLE(AK4953A_02_SIGNAL_SELECT1, 4,
			ARRAY_SIZE(ak4953_micbias_select_texts),
			ak4953_micbias_select_texts);

static const struct snd_kcontrol_new ak4953_micbias_mux_control =
	SOC_DAPM_ENUM("MIC bias Select", ak4953_micbias_mux_enum);

static const char *ak4953_adcpf_select_texts[] = {"SDTI", "ADC"};

static const struct soc_enum ak4953_adcpf_mux_enum =
	SOC_ENUM_SINGLE(AK4953A_1D_DIGITAL_FILTER_MODE, 1,
			ARRAY_SIZE(ak4953_adcpf_select_texts),
			ak4953_adcpf_select_texts);

static const struct snd_kcontrol_new ak4953_adcpf_mux_control =
	SOC_DAPM_ENUM("ADCPF Select", ak4953_adcpf_mux_enum);

static const char *ak4953_pfsdo_select_texts[] = {"ADC", "PFIL"};

static const struct soc_enum ak4953_pfsdo_mux_enum =
	SOC_ENUM_SINGLE(AK4953A_1D_DIGITAL_FILTER_MODE, 0,
			ARRAY_SIZE(ak4953_pfsdo_select_texts),
			ak4953_pfsdo_select_texts);

static const struct snd_kcontrol_new ak4953_pfsdo_mux_control =
	SOC_DAPM_ENUM("PFSDO Select", ak4953_pfsdo_mux_enum);

static const char *ak4953_pfdac_select_texts[] = {"SDTI", "PFIL"};

static const struct soc_enum ak4953_pfdac_mux_enum =
	SOC_ENUM_SINGLE(AK4953A_1D_DIGITAL_FILTER_MODE, 2,
			ARRAY_SIZE(ak4953_pfdac_select_texts),
			ak4953_pfdac_select_texts);

static const struct snd_kcontrol_new ak4953_pfdac_mux_control =
	SOC_DAPM_ENUM("PFDAC Select", ak4953_pfdac_mux_enum);

static const char *ak4953_mic_select_texts[] =
		{"AMIC", "DMIC"};

static const struct soc_enum ak4953_mic_mux_enum =
	SOC_ENUM_SINGLE(AK4953A_08_DIGITL_MIC, 0,
			ARRAY_SIZE(ak4953_mic_select_texts),
			ak4953_mic_select_texts);

static const struct snd_kcontrol_new ak4953_mic_mux_control =
	SOC_DAPM_ENUM("MIC Select", ak4953_mic_mux_enum);

static const char *ak4953_hpsw_select_texts[] = {"OFF", "ON"};

static const struct soc_enum ak4953_hpsw_mux_enum =
	SOC_ENUM_SINGLE(0, 0, ARRAY_SIZE(ak4953_hpsw_select_texts),
			ak4953_hpsw_select_texts);

static const struct snd_kcontrol_new ak4953_hpsw_mux_control =
	SOC_DAPM_ENUM_VIRT("HP Switch", ak4953_hpsw_mux_enum);

static const struct snd_kcontrol_new ak4953_dacs_mixer_controls[] = {
	SOC_DAPM_SINGLE("DACS", AK4953A_02_SIGNAL_SELECT1, 5, 1, 0),
};

static int ak4953_spko_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	akdbgprt("\t[AK4953A] %s(%d)\n",__FUNCTION__,__LINE__);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:	/* before widget power up */
		break;
	case SND_SOC_DAPM_POST_PMU:	/* after widget power up */
		akdbgprt("\t[AK4953A] %s wait=1msec\n",__FUNCTION__);
		mdelay(1);
		ak4953_writeMask(codec, AK4953A_02_SIGNAL_SELECT1,
				 0x80, 0x80);
		break;
	case SND_SOC_DAPM_PRE_PMD:	/* before widget power down */
		ak4953_writeMask(codec, AK4953A_02_SIGNAL_SELECT1,
				 0x80, 0x00);
		mdelay(1);
		break;
	case SND_SOC_DAPM_POST_PMD:	/* after widget power down */
		break;
	}

	return 0;
}


static const struct snd_soc_dapm_widget ak4953_dapm_widgets[] = {
#ifdef PLL_32BICK_MODE
	SND_SOC_DAPM_SUPPLY("PMPLL", AK4953A_01_POWER_MANAGEMENT2, 0, 0, NULL,
			0),
#else
#ifdef PLL_64BICK_MODE
	SND_SOC_DAPM_SUPPLY("PMPLL", AK4953A_01_POWER_MANAGEMENT2, 0, 0, NULL,
			0),
#endif
#endif

	/* DAC */
	SND_SOC_DAPM_DAC("DAC", "NULL", AK4953A_00_POWER_MANAGEMENT1, 2, 0),

	/* Analog Output */
	SND_SOC_DAPM_OUTPUT("SPK"),
	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("HPR"),

	SND_SOC_DAPM_PGA("SPK Amp", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_PGA("HPL Amp", AK4953A_01_POWER_MANAGEMENT2, 5, 0, NULL,
			0),
	SND_SOC_DAPM_PGA("HPR Amp", AK4953A_01_POWER_MANAGEMENT2, 4, 0, NULL,
			0),

	SND_SOC_DAPM_MIXER_E("SPKO Mixer", AK4953A_00_POWER_MANAGEMENT1, 4, 0,
			&ak4953_dacs_mixer_controls[0],
			ARRAY_SIZE(ak4953_dacs_mixer_controls),
			ak4953_spko_event,
			(SND_SOC_DAPM_POST_PMU |SND_SOC_DAPM_PRE_PMD |
			 SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD)),

	SND_SOC_DAPM_VIRT_MUX("DACHP", SND_SOC_NOPM, 0, 0,
			&ak4953_hpsw_mux_control),

	SND_SOC_DAPM_AIF_OUT("SDTO", "Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDTI", "Playback", 0, SND_SOC_NOPM, 0, 0),

	/* PFIL */
	SND_SOC_DAPM_MUX("PFDAC MUX", SND_SOC_NOPM, 0, 0,
			&ak4953_pfdac_mux_control),
	SND_SOC_DAPM_MUX("PFSDO MUX", SND_SOC_NOPM, 0, 0,
			&ak4953_pfsdo_mux_control),
	SND_SOC_DAPM_MUX("ADCPF MUX", SND_SOC_NOPM, 0, 0,
			&ak4953_adcpf_mux_control),

	/* Digital Mic */
	SND_SOC_DAPM_INPUT("DMICLIN"),
	SND_SOC_DAPM_INPUT("DMICRIN"),

	SND_SOC_DAPM_ADC("DMICL", "NULL", AK4953A_08_DIGITL_MIC, 4, 0),
	SND_SOC_DAPM_ADC("DMICR", "NULL", AK4953A_08_DIGITL_MIC, 5, 0),

	SND_SOC_DAPM_VIRT_MUX("MIC MUX", SND_SOC_NOPM, 0, 0,
			&ak4953_mic_mux_control),
	SND_SOC_DAPM_VIRT_MUX("ADC MUX2", SND_SOC_NOPM, 0, 0,
			&ak4953_adc2_mux_control),

	/* ADC */
	SND_SOC_DAPM_ADC("ADC Left", "NULL", AK4953A_00_POWER_MANAGEMENT1, 0,
			0),
	SND_SOC_DAPM_ADC("ADC Right", "NULL", AK4953A_00_POWER_MANAGEMENT1, 1,
			0),

	SND_SOC_DAPM_VIRT_MUX("ADC MUX1", SND_SOC_NOPM, 0, 0,
			&ak4953_adc1_mux_control),
	SND_SOC_DAPM_ADC("PFIL", "NULL", AK4953A_00_POWER_MANAGEMENT1, 7, 0),

	/* Analog Input  MIC Bias */
	SND_SOC_DAPM_INPUT("LIN1/RIN1"),
	SND_SOC_DAPM_INPUT("LIN2/RIN2"),
	SND_SOC_DAPM_INPUT("LIN3/RIN3"),

	SND_SOC_DAPM_MICBIAS("Mic Bias", AK4953A_02_SIGNAL_SELECT1, 3, 0),

	SND_SOC_DAPM_VIRT_MUX("IN2 MUX", SND_SOC_NOPM, 0, 0,
			&ak4953_in2_mux_control),
	SND_SOC_DAPM_VIRT_MUX("IN1 MUX", SND_SOC_NOPM, 0, 0,
			&ak4953_in1_mux_control),

	SND_SOC_DAPM_MUX("Input Select MUX", SND_SOC_NOPM, 0, 0,
		&ak4953_input_select_controls),

	SND_SOC_DAPM_MUX("Mic Bias MUX", SND_SOC_NOPM, 0, 0,
			&ak4953_micbias_mux_control),
};


static const struct snd_soc_dapm_route ak4953_intercon[] = {
#ifdef PLL_32BICK_MODE
	{"ADC Left", "NULL", "PMPLL"},
	{"ADC Right", "NULL", "PMPLL"},
	{"DAC", "NULL", "PMPLL"},
#else
#ifdef PLL_64BICK_MODE
	{"ADC Left", "NULL", "PMPLL"},
	{"ADC Right", "NULL", "PMPLL"},
	{"DAC", "NULL", "PMPLL"},
#endif	/* End of PLL_64BICK_MODE */
#endif	/* End of PLL_32BICK_MODE */
	{"Mic Bias MUX", "IN1", "LIN1/RIN1"},
	{"Mic Bias MUX", "IN2", "LIN2/RIN2"},

	{"Mic Bias", "NULL", "Mic Bias MUX"},

	{"IN1 MUX", "IN1", "LIN1/RIN1"},
	{"IN1 MUX", "Mic Bias", "Mic Bias"},
	{"IN2 MUX", "IN2", "LIN2/RIN2"},
	{"IN2 MUX", "Mic Bias", "Mic Bias"},

	{"Input Select MUX", "LIN1/RIN1", "IN1 MUX"},
	{"Input Select MUX", "LIN2/RIN2", "IN2 MUX"},
	{"Input Select MUX", "LIN3/RIN3", "LIN3/RIN3"},

	{"ADC Left", "NULL", "Input Select MUX"},
	{"ADC Right", "NULL", "Input Select MUX"},

	{"ADC MUX1", "Stereo", "ADC Left"},
	{"ADC MUX1", "Stereo", "ADC Right"},
	{"ADC MUX1", "Mono", "ADC Left"},

	{"DMICL", "NULL", "DMICLIN"},
	{"DMICR", "NULL", "DMICRIN"},

	{"ADC MUX2", "Stereo", "DMICL"},
	{"ADC MUX2", "Stereo", "DMICR"},
	{"ADC MUX2", "Mono", "DMICL"},

	{"MIC MUX", "DMIC", "ADC MUX2"},
	{"MIC MUX", "AMIC", "ADC MUX1"},

	{"ADCPF MUX", "ADC", "MIC MUX"},
	{"ADCPF MUX", "SDTI", "SDTI"},
	{"PFIL", "NULL", "ADCPF MUX"},

	{"PFSDO MUX", "PFIL", "PFIL"},
	{"PFSDO MUX", "ADC", "MIC MUX"},
	{"SDTO", "NULL", "PFSDO MUX"},

	{"PFDAC MUX", "PFIL", "PFIL"},
	{"PFDAC MUX", "SDTI", "SDTI"},
	{"DAC", "NULL", "PFDAC MUX"},

	{"DACHP", "ON", "DAC"},
	{"HPL Amp", "NULL", "DACHP"},
	{"HPR Amp", "NULL", "DACHP"},
	{"HPL", "NULL", "HPL Amp"},
	{"HPR", "NULL", "HPR Amp"},

	{"SPKO Mixer", "DACS", "DAC"},
	{"SPK Amp", "NULL", "SPKO Mixer"},
	{"SPK", "NULL", "SPK Amp"},
};


static int ak4953_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 	fs;

	akdbgprt("\t[AK4953A] %s(%d)\n",__FUNCTION__,__LINE__);

	fs = snd_soc_read(codec, AK4953A_06_MODE_CONTROL2);
	fs &= ~AK4953A_FS;

	switch (params_rate(params)) {
	case 8000:
		fs |= AK4953A_FS_8KHZ;
		break;
	case 11025:
		fs |= AK4953A_FS_11_025KHZ;
		break;
	case 12000:
		fs |= AK4953A_FS_12KHZ;
		break;
	case 16000:
		fs |= AK4953A_FS_16KHZ;
		break;
	case 22050:
		fs |= AK4953A_FS_22_05KHZ;
		break;
	case 24000:
		fs |= AK4953A_FS_24KHZ;
		break;
	case 32000:
		fs |= AK4953A_FS_32KHZ;
		break;
	case 44100:
		fs |= AK4953A_FS_44_1KHZ;
		break;
	case 48000:
		fs |= AK4953A_FS_48KHZ;
		break;
	case 96000:
		fs |= AK4953A_FS_96KHZ;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, AK4953A_06_MODE_CONTROL2, fs);

	return 0;
}

static int ak4953_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
		unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 pll;

	akdbgprt("\t[AK4953A] %s(%d)\n",__FUNCTION__,__LINE__);

	pll = snd_soc_read(codec, AK4953A_05_MODE_CONTROL1);
	pll &= ~AK4953A_PLL;

#ifdef PLL_32BICK_MODE
	pll |= AK4953A_PLL_BICK32;
#else
#ifdef PLL_64BICK_MODE
	pll |= AK493A_PLL_BICK64;
#else
	pll |= AK4953A_EXT_SLAVE;
#endif
#endif

	snd_soc_write(codec, AK4953A_05_MODE_CONTROL1, pll);

	return 0;
}

static int ak4953_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{

	struct snd_soc_codec *codec = dai->codec;
	u8 mode;
	u8 format;

	akdbgprt("\t[AK4953A] %s(%d)\n",__FUNCTION__,__LINE__);

	/* set master/slave audio interface */
	mode = snd_soc_read(codec, AK4953A_01_POWER_MANAGEMENT2);
#ifdef PLL_32BICK_MODE
	mode |= AK4953A_PMPLL;
#else
#ifdef PLL_64BICK_MODE
	mode |= AK4953A_PMPLL;
#endif
#endif
	format = snd_soc_read(codec, AK4953A_05_MODE_CONTROL1);
	format &= ~AK4953A_DIF;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		akdbgprt("\t[AK4953A] %s(Slave)\n",__FUNCTION__);
		mode &= ~(AK4953A_M_S);
		format &= ~(AK4953A_BCKO);
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		akdbgprt("\t[AK4953A] %s(Master)\n",__FUNCTION__);
		mode |= (AK4953A_M_S | AK4953A_PMPLL | 0x02);
		/* 24Mhz master clock from AP */
		format |= (AK4953A_BCKO | 0x70);
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
	default:
		dev_err(codec->dev, "Clock mode unsupported");
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		format |= AK4953A_DIF_I2S_MODE;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		format |= AK4953A_DIF_24MSB_MODE;
		break;
	default:
		return -EINVAL;
	}

	/* set mode and format */
	snd_soc_write(codec, AK4953A_05_MODE_CONTROL1, format);
	mdelay(10); /* for pll lock */
	snd_soc_write(codec, AK4953A_01_POWER_MANAGEMENT2, mode);
	mdelay(10); /* for pll lock */

	return 0;
}

static int ak4953_volatile(struct snd_soc_codec *codec, unsigned int reg)
{
	return 0;
}

static int ak4953_readable(struct snd_soc_codec *codec, unsigned int reg)
{
	if (reg >= ARRAY_SIZE(ak4953_access_masks))
		return 0;
	return ak4953_access_masks[reg].readable != 0;
}

/*
 * Read ak4953 register cache
 */
static inline u32 ak4953_read_reg_cache(struct snd_soc_codec *codec, u16 reg)
{
    u8 *cache = codec->reg_cache;
    BUG_ON(reg > ARRAY_SIZE(ak4953_reg));
    return (u32)cache[reg];
}

#ifdef AK4953A_CONTIF_DEBUG
/*
 * Write ak4953 register cache
 */
static inline void ak4953_write_reg_cache(struct snd_soc_codec *codec,
	u16 reg, u16 value)
{
	u8 *cache = codec->reg_cache;
	BUG_ON(reg > ARRAY_SIZE(ak4953_reg));
	cache[reg] = (u8)value;
}

unsigned int ak4953_i2c_read(struct snd_soc_codec *codec, unsigned int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(codec->control_data, (u8)(reg & 0xFF));

	if (ret < 0)
		akdbgprt("\t[AK4953A] %s(%d)\n", __FUNCTION__, __LINE__);

	return ret;
}

static int ak4953_i2c_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	ak4953_write_reg_cache(codec, reg, value);

	akdbgprt("\t[ak4953] %s: (addr,data)=(%x, %x)\n", __FUNCTION__, reg,
			value);

	if (i2c_smbus_write_byte_data(codec->control_data, (u8)(reg & 0xFF),
				(u8)(value & 0xFF)) < 0) {
		akdbgprt("\t[AK4953A] %s(%d)\n", __FUNCTION__, __LINE__);
		return EIO;
	}

	return 0;
}
#endif

/*
 * Write with Mask to  AK4953A register space
 */
static int ak4953_writeMask(struct snd_soc_codec *codec, u16 reg, u16 mask,
			    u16 value)
{
	u16 olddata;
	u16 newdata;

	if ( (mask == 0) || (mask == 0xFF) )
		newdata = value;
	else {
		olddata = ak4953_read_reg_cache(codec, reg);
		newdata = (olddata & ~(mask)) | value;
	}

	snd_soc_write(codec, (unsigned int)reg, (unsigned int)newdata);

	akdbgprt("\t[ak4953_writeMask] %s(%d): (addr,data)=(%x, %x)\n",
			__FUNCTION__, __LINE__, reg, newdata);

	return 0;
}

/* for AK4953A */
static int ak4953_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *codec_dai)
{
	int 	ret = 0;

	akdbgprt("\t[AK4953A] %s(%d)\n", __FUNCTION__, __LINE__);

	return ret;
}

static int ak4953_set_bias_level(struct snd_soc_codec *codec,
		enum snd_soc_bias_level level)
{
	u8 reg;

	akdbgprt("\t[AK4953A] %s(%d)\n",__FUNCTION__,__LINE__);

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_STANDBY:
		/* For AK4953A */
		reg = snd_soc_read(codec, AK4953A_00_POWER_MANAGEMENT1);
		snd_soc_write(codec, AK4953A_00_POWER_MANAGEMENT1,
				reg | AK4953A_PMVCM);
		break;
	case SND_SOC_BIAS_OFF:
		/* For AK4953A */
		snd_soc_write(codec, AK4953A_00_POWER_MANAGEMENT1, 0x00);
		break;
	}
	codec->dapm.bias_level = level;
	return 0;
}

#define AK4953A_RATES		(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
				SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |\
				SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |\
				SNDRV_PCM_RATE_96000)

#define AK4953A_FORMATS		SNDRV_PCM_FMTBIT_S16_LE | \
				SNDRV_PCM_FMTBIT_S24_LE

static struct snd_soc_dai_ops ak4953_dai_ops = {
	.hw_params	= ak4953_hw_params,
	.set_sysclk	= ak4953_set_dai_sysclk,
	.set_fmt	= ak4953_set_dai_fmt,
	.trigger = ak4953_trigger,
};

struct snd_soc_dai_driver ak4953_dai[] = {
	{
		.name = "ak4953-AIF1",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AK4953A_RATES,
			.formats = AK4953A_FORMATS,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AK4953A_RATES,
			.formats = AK4953A_FORMATS,
		},
		.ops = &ak4953_dai_ops,
	},
};

static int ak4953_write_cache_reg(struct snd_soc_codec *codec, u16 regs,
	u16 rege)
{
	u32	reg, cache_data;

	reg = regs;
	do {
		cache_data = ak4953_read_reg_cache(codec, reg);
		snd_soc_write(codec, (unsigned int)reg,
				(unsigned int)cache_data);
		reg ++;
	} while (reg <= rege);

	return 0;
}

static int ak4953_set_reg_digital_effect(struct snd_soc_codec *codec)
{
	ak4953_write_cache_reg(codec, AK4953A_09_TIMER_SELECT,
			AK4953A_14_RCH_DIGITAL_VOLUME_CONTROL);
	ak4953_write_cache_reg(codec, AK4953A_1C_DIGITAL_FILTER_SELECT1,
			AK4953A_1C_DIGITAL_FILTER_SELECT1);
	ak4953_write_cache_reg(codec, AK4953A_1E_HPF2_COEFFICIENT0,
			AK4953A_25_LPF_COEFFICIENT3);
	ak4953_write_cache_reg(codec, AK4953A_32_E1_COEFFICIENT0,
			AK4953A_4F_E5_COEFFICIENT5);

	return 0;
}

static int ak4953_probe(struct snd_soc_codec *codec)
{
	struct ak4953_priv *ak4953 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	akdbgprt("\t[AK4953A] %s(%d)\n", __FUNCTION__, __LINE__);

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_I2C);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

#ifdef AK4616_CONTIF_DEBUG
	codec->write = ak4953_i2c_write;
	codec->read = ak4953_i2c_read;
#endif
	ak4953_codec = codec;

	akdbgprt("\t[AK4953A] %s(%d) ak4953=%x\n", __FUNCTION__, __LINE__,
			(int)ak4953);

	snd_soc_write(codec, AK4953A_00_POWER_MANAGEMENT1, 0x00);
	snd_soc_write(codec, AK4953A_00_POWER_MANAGEMENT1, 0x00);

	ak4953_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	akdbgprt("\t[AK4953A bias] %s(%d)\n", __FUNCTION__, __LINE__);

	ak4953_set_reg_digital_effect(codec);

	akdbgprt("\t[AK4953A Effect] %s(%d)\n", __FUNCTION__, __LINE__);

	ak4953->onDrc = 0;
	ak4953->onStereo = 0;

	return ret;
}

static int ak4953_remove(struct snd_soc_codec *codec)
{
	akdbgprt("\t[AK4953A] %s(%d)\n", __FUNCTION__, __LINE__);

	ak4953_set_bias_level(codec, SND_SOC_BIAS_OFF);

	gpio_set_value(ak4953_data->pdn, 0);

	return 0;
}

#ifdef CONFIG_PM
static int ak4953_suspend(struct device *dev)
{
	ak4953_set_bias_level(ak4953_codec, SND_SOC_BIAS_OFF);

	gpio_set_value(ak4953_data->pdn, 0);

	return 0;
}

static int ak4953_resume(struct device *dev)
{
	gpio_set_value(ak4953_data->pdn, 1);

	ak4953_set_bias_level(ak4953_codec, SND_SOC_BIAS_STANDBY);

	return 0;
}

static const struct dev_pm_ops ak4953_pm_ops = {
	.suspend = ak4953_suspend,
	.resume = ak4953_resume,
};
#else
static int ak4953_suspend(struct device *dev)
{
	return 0;
}

static int ak4953_resume(struct device *dev)
{
	return 0;
}
#endif

struct snd_soc_codec_driver soc_codec_dev_ak4953 = {
	.probe = ak4953_probe,
	.remove = ak4953_remove,

	.set_bias_level = ak4953_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(ak4953_reg),
	.reg_word_size = sizeof(u8),
	.reg_cache_default = ak4953_reg,
	.readable_register = ak4953_readable,
	.volatile_register = ak4953_volatile,
	.dapm_widgets = ak4953_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(ak4953_dapm_widgets),
	.dapm_routes = ak4953_intercon,
	.num_dapm_routes = ARRAY_SIZE(ak4953_intercon),
	.controls = ak4953_snd_controls,
	.num_controls = ARRAY_SIZE(ak4953_snd_controls),
};
EXPORT_SYMBOL_GPL(soc_codec_dev_ak4953);

#ifdef CONFIG_OF
static int ak4953_i2c_parse_dt(struct i2c_client *i2c, struct ak4953_priv *p)
{
	struct device *dev = &i2c->dev;
	struct device_node *np = dev->of_node;
	int ret;

	if (!np)
		return -1;

	p->pdn = of_get_named_gpio(np, "ak4953,pdn-gpio", 0);
	if (p->pdn < 0) {
		dev_err(dev, "Looking up %s property in node %s failed %d\n",
			"ak4953,pdn-gpio", dev->of_node->full_name,
			p->pdn);
		p->pdn = -1;

		return -1;
	}

	if( !gpio_is_valid(p->pdn) ) {
		dev_err(dev, "ak4953 pdn pin(%u) is invalid\n", p->pdn);
		return -1;
	}

	dev_dbg(dev, "p->pdn = %u\n", p->pdn);

	ret = devm_gpio_request(dev, p->pdn, "PDNA");
	if (ret) {
		dev_err(dev, "%s: failed to request reset gpio PDNA\n",
				__func__);
		return -EBUSY;
	}

	ret = gpio_direction_output(p->pdn, 1);
	if (ret < 0) {
		dev_err(dev, "Unable to set PDNA Pin output\n");
		gpio_free(p->pdn);
		return -ENODEV;
	}

	gpio_set_value(p->pdn, 0);
	/* Wait for at least t>1.5 us */
	udelay(10);

	/* Drive PDNA pin High. Note: PDNA shud be kept high
	   during CODEC normal operation */
	gpio_set_value(p->pdn, 1);

	return 0;
}
#else
static int ak4953_i2c_parse_dt(struct i2c_client *i2c, struct ak4953_priv *p)
{
	return NULL;
}
#endif

static int ak4953_i2c_probe(struct i2c_client *i2c,
                            const struct i2c_device_id *id)
{
	struct ak4953_priv *ak4953;
	struct snd_soc_codec *codec;
	struct device *dev = &i2c->dev;
	int ret = 0;

	akdbgprt("\t[AK4953A] %s(%d)\n", __FUNCTION__, __LINE__);

	ak4953 = devm_kzalloc(dev, sizeof(struct ak4953_priv), GFP_KERNEL);
	if (!ak4953)
		return -ENOMEM;

	if (i2c->dev.of_node) {
		dev_dbg(dev, "Read PDN pin from device tree\n");
		ret = ak4953_i2c_parse_dt(i2c, ak4953);
		if(ret < 0) {
			dev_err(dev, "ak4953 PDN pin error\n");
		}
	}

	codec = &ak4953->codec;
	i2c_set_clientdata(i2c, ak4953);
	codec->control_data = i2c;
	ak4953_data = ak4953;

	codec->dev = dev;
	snd_soc_codec_set_drvdata(codec, ak4953);

	ret = snd_soc_register_codec(dev, &soc_codec_dev_ak4953,
			&ak4953_dai[0], ARRAY_SIZE(ak4953_dai));
	if (ret < 0)
		akdbgprt("\t[AK4953A Error!] %s(%d)\n", __FUNCTION__,
				__LINE__);

	return ret;
}

static int ak4953_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id ak4953_i2c_dt_ids[] = {
	{ .compatible = "akm,ak4953"},
	{ },
};
#endif

static const struct i2c_device_id ak4953_i2c_id[] = {
	{ "ak4953", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ak4953_i2c_id);

static struct i2c_driver ak4953_i2c_driver = {
	.driver = {
		.name = "ak4953",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(ak4953_i2c_dt_ids),
#endif

#ifdef CONFIG_PM
		.pm = &ak4953_pm_ops,
#endif

	},
	.probe = ak4953_i2c_probe,
	.remove = ak4953_i2c_remove,
	.id_table = ak4953_i2c_id,
};

static int __init ak4953_modinit(void)
{
	akdbgprt("\t[AK4953A] %s(%d)\n", __FUNCTION__, __LINE__);

	return i2c_add_driver(&ak4953_i2c_driver);
}
module_init(ak4953_modinit);

static void __exit ak4953_exit(void)
{
	i2c_del_driver(&ak4953_i2c_driver);
}
module_exit(ak4953_exit);

MODULE_DESCRIPTION("ASoC ak4953 codec driver");
MODULE_LICENSE("GPL");
