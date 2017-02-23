/*
 *  shiri_ak4678.c
 *
 *  Copyright (c) 2012, Insignal Co., Ltd.
 *
 *  Author: Claude <claude@insginal.co.kr>
 *          Nermy  <nermy@insignal.co.kr>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/firmware.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/control.h>

#include <plat/gpio-cfg.h>
#include <mach/regs-pmu.h>
#include <mach/regs-clock-exynos3.h>

#include "i2s.h"
#include "i2s-regs.h"


#if 0
	#define gprintk(fmt, x... ) printk( "%s: " fmt, __FUNCTION__ , ## x)
#else
	#define gprintk(x...) do { } while (0)
#endif

#define GPIO_CODEC_RESET    EXYNOS3_GPC0(3)

#define AK4678_EXT_SLAVE			 0

static bool xclkout_enabled;

static void shiri_xclkout_enable(bool on)
{
	gprintk("xclkout = %s\n", on ? "on" : "off");

	xclkout_enabled = on;
	/* XUSBXTI 24MHz via XCLKOUT */
	writel(on ? 0x0900 : 0x0901, EXYNOS_PMU_DEBUG);
}

static int set_aud_pll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		pr_err("%s: failed to get fout_epll\n", __func__);
		return PTR_ERR(fout_epll);
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_set_rate(fout_epll, rate);
	gprintk("EPLL rate = %ld\n", clk_get_rate(fout_epll));
out:
	clk_put(fout_epll);

	return 0;
}


#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER

static int set_aud_sclk(struct snd_soc_card *card, unsigned long epll, unsigned long rate)
{
	struct clk *fout_epll, *mout_epll, *mout_audio, *dout_audio, *dout_i2s, *sclk_i2s;
	int ret = 0;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		pr_err("%s: failed to get fout_epll\n", __func__);
		ret = PTR_ERR(fout_epll);
		goto out0;
	}

	mout_epll = clk_get(card->dev, "mout_epll");
	if (IS_ERR(mout_epll)) {
		pr_err("%s: failed to get mout_epll\n", __func__);
		ret = PTR_ERR(mout_epll);
		goto out1;
	}
	clk_set_parent(mout_epll, fout_epll);

	writel(readl(EXYNOS3_MIF_L_REG(0x1120)) | 0x00000010,
			EXYNOS3_MIF_L_REG(0x1120));

	mout_audio = clk_get(card->dev, "mout_audio");
	if (IS_ERR(mout_audio)) {
		pr_err("%s: failed to get mout_audio\n", __func__);
		ret = PTR_ERR(mout_audio);
		goto out2;
	}
	clk_set_parent(mout_audio, mout_epll);
	gprintk("mout_audio = %ld\n", clk_get_rate(mout_audio) );

	dout_audio = clk_get(card->dev, "dout_audio");
	if (IS_ERR(dout_audio)) {
		pr_err("%s: failed to get dout_audio\n", __func__);
		ret = PTR_ERR(dout_audio);
		goto out3;
	}
	clk_set_parent(dout_audio, mout_audio);
	clk_set_rate(dout_audio, rate);
	gprintk("dout_audio rate = %ld\n", clk_get_rate(dout_audio));

	dout_i2s = clk_get(card->dev, "dout_i2s");
	if (IS_ERR(dout_i2s)) {
		pr_err("%s: failed to get dout_i2s\n", __func__);
		ret = PTR_ERR(dout_i2s);
		goto out4;
	}
	clk_set_parent(dout_i2s, dout_audio);
	clk_set_rate(dout_i2s, rate);
	gprintk("dout_i2s rate = %ld\n", clk_get_rate(dout_i2s));

	sclk_i2s = clk_get(card->dev, "sclk_i2s");
	if (IS_ERR(sclk_i2s)) {
		pr_err("%s: failed to get sclk_i2s\n", __func__);
		ret = PTR_ERR(sclk_i2s);
		goto out5;
	}
	gprintk("sclk_i2s rate = %ld\n", clk_get_rate(sclk_i2s));
	gprintk("clk_prepare_enable......sclk_i2s\n");


	writel(0x10008, EXYNOS3_CLK_BUS_TOP_REG(0xCA00));
	writel(0x0100, EXYNOS_PMU_DEBUG);

	clk_put(sclk_i2s);
out5:
	clk_put(dout_i2s);
out4:
	clk_put(dout_audio);
out3:
	clk_put(mout_audio);
out2:
	clk_put(mout_epll);
out1:
	clk_put(fout_epll);
out0:
	return ret;
}

/*
 * Shiri hw params. (AP I2S Master with ak4678 - codec slave mode)
 */
static int shiri_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret, bfs;
	unsigned int fs, epll;

	/* AIF1CLK should be >=3MHz for optimal performance */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	fs = params_rate(params);

	gprintk("sampling rate = %u\n", fs);
	switch (fs) {
	case 24000:
	case 48000:
	case 96000:
	case 192000:
		epll = 49152000;
		ret = set_aud_pll_rate(epll);
		if (ret) {
			pr_err("%s: set_aud_pll_rate return error %d\n", __func__, ret);
			return ret;
		}
		break;
	case 44100:
		epll = 45158400;
		ret = set_aud_pll_rate(epll);
		if (ret) {
			pr_err("%s: set_aud_pll_rate return error %d\n", __func__, ret);
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}

	gprintk("epll = %u, sclk = fs*256 = %u\n", epll, fs*256);
	ret = set_aud_sclk(card, epll, fs*256);
	if (ret) {
		pr_err("%s: set_aud_sclk return error %d\n", __func__, ret);
		return ret;
	}


	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_pll(codec_dai, AK4678_EXT_SLAVE, 0, 0, 0);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set ext slave mode codec fmt: %d\n", ret);
		return ret;
	}


	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	gprintk("cdclk out\n");
	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					256, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	gprintk("opclk out\n");
	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	gprintk("divbclk out = %d\n", bfs);
	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	return 0;
}
#else

/*
 * set_aud_sclk - codec master mode
 */
static int set_aud_sclk(struct snd_soc_card *card, unsigned long rate)
{
	struct clk *mout_epll;
	struct clk *mout_audio;
	struct clk *sclk_i2s;
	struct clk *fout_epll;
	int ret = 0;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		pr_err("%s: failed to get fout_epll\n", __func__);
		return PTR_ERR(fout_epll);
	}

	mout_epll = clk_get(card->dev, "mout_epll");
	if (IS_ERR(mout_epll)) {
		pr_err("%s: failed to get mout_epll\n", __func__);
		ret = -EINVAL;
		goto out1;
	}
	clk_set_parent(mout_epll, fout_epll);

	mout_audio = clk_get(card->dev, "mout_audio");
	if (IS_ERR(mout_audio)) {
		pr_err("%s: failed to get mout_audio\n", __func__);
		ret = -EINVAL;
		goto out2;
	}
	clk_set_parent(mout_audio, mout_epll);

	sclk_i2s = clk_get(card->dev, "sclk_i2s");
	if (IS_ERR(sclk_i2s)) {
		pr_err("%s: failed to get sclk_i2s\n", __func__);
		ret = -EINVAL;
		goto out3;
	}

	clk_set_rate(sclk_i2s, rate);
	gprintk("SCLK_I2S rate = %ld\n", clk_get_rate(sclk_i2s));

	clk_put(sclk_i2s);
out3:
	clk_put(mout_audio);
out2:
	clk_put(mout_epll);
out1:
	clk_put(fout_epll);

	return ret;
}

/*
 * Shiri hw param setup - codec master mode
 */
#define AK4678_PLL_MASTER            3
#define AK4678_PLL_24MHZ            (7 << 0)
static int shiri_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret;

	set_aud_pll_rate(49152000);
	set_aud_sclk(card, 12288000);

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set codec mode fmt: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_pll(codec_dai, AK4678_PLL_MASTER, AK4678_PLL_24MHZ, 0, 0);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set PLL master mode codec fmt: %d\n", ret);
		return ret;
	}


	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif1 cpu fmt: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set SAMSUNG_I2S_CDCL: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set SAMSUNG_I2S_OPCL: %d\n", ret);
		return ret;
	}


	return ret;
}
#endif

static struct snd_soc_ops shiri_ops = {
	.hw_params = shiri_hw_params,
};

static int shiri_ak4678_init_paiftx(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
	shiri_xclkout_enable(false);
#else
	shiri_xclkout_enable(true);
#endif
	snd_soc_dapm_sync(dapm);

	return 0;
}

static struct snd_soc_dai_link shiri_dai[] = {
	{
		.name = "AK4678 HiFi",
		.stream_name = "Playback",
		.codec_dai_name = "ak4678-hifi",
		.codec_name = "ak4678-codec.1-0012",
		.init = shiri_ak4678_init_paiftx,
		.ops = &shiri_ops,
		.cpu_dai_name = "13970000.i2s",
		.platform_name = "13970000.i2s",
	},
};

static struct snd_soc_card shiri = {
	.name = "SHIRI-AK4678",
	.dai_link = shiri_dai,
	.num_links = ARRAY_SIZE(shiri_dai),
};

static int shiri_ak4678_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &shiri;

	gprintk("start \n");
	/* register card */
	card->dev = &pdev->dev;


	if (np) {

		if (!shiri_dai[0].cpu_dai_name) {
			shiri_dai[0].cpu_of_node = of_parse_phandle(np,
							"samsung,audio-cpu", 0);

			if (!shiri_dai[0].cpu_of_node) {
				dev_err(&pdev->dev,
				"Property 'samsung,audio-cpu' missing or invalid\n");
				ret = -EINVAL;
			}
		}

		if (!shiri_dai[0].platform_name) {
			shiri_dai[0].platform_of_node = shiri_dai[0].cpu_of_node;
		}
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);
		return ret;
	}

#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
{
	struct clk *sclk_i2s = clk_get(card->dev, "sclk_i2s");
	if (IS_ERR(sclk_i2s)) {
		pr_err("%s: failed to get sclk_i2s\n", __func__);
		ret = PTR_ERR(sclk_i2s);
	} else {
		clk_prepare_enable(sclk_i2s);
	}
}
#endif
	gprintk("ak4678-dai: register card %s -> %s\n", card->dai_link->codec_dai_name, card->dai_link->cpu_dai_name);

	return ret;

}
static int shiri_ak4678_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&shiri);
#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
{
	struct clk *sclk_i2s = clk_get(shiri.dev, "sclk_i2s");
	if (IS_ERR(sclk_i2s)) {
		pr_err("%s: failed to get sclk_i2s\n", __func__);
	} else {
		clk_disable_unprepare(sclk_i2s);
	}
}
#endif
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id akm_ak4678_of_match[] = {
	{ .compatible = "samsung,shiri-ak4678", },
	{},
};
MODULE_DEVICE_TABLE(of, akm_ak4678_of_match);
#endif /* CONFIG_OF */

static struct platform_driver origen_ak4678_driver = {
	.driver = {
		.name = "SHIRI-I2S",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(akm_ak4678_of_match),
	},
	.probe = shiri_ak4678_probe,
	.remove = shiri_ak4678_remove,
};
module_platform_driver(origen_ak4678_driver);


MODULE_AUTHOR("nermy <nermy@insignal.co.kr>");
MODULE_DESCRIPTION("AK4678 ALSA SoC Driver for Shiri Board");
MODULE_LICENSE("GPL");
