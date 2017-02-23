/*
 *  espresso3250_wm5110.c
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
#include <linux/mfd/arizona/pdata.h>
#include <linux/mfd/arizona/registers.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/control.h>

#include <plat/gpio-cfg.h>
#include <mach/regs-pmu.h>

#include "i2s.h"
#include "i2s-regs.h"
#include "../codecs/florida.h"
#include "../codecs/arizona.h"

#define ESPRESSO_DEFAULT_MCLK1	24000000
#define ESPRESSO_DEFAULT_MCLK2	32768

static bool clkout_enabled;

static void espresso_enable_mclk(bool on)
{
	pr_debug("%s: %s\n", __func__, on ? "on" : "off");

	clkout_enabled = on;
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
	pr_debug("%s: EPLL rate = %ld\n",
			__func__, clk_get_rate(fout_epll));
out:
	clk_put(fout_epll);

	return 0;
}

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
	pr_debug("%s: SCLK_I2S rate = %ld\n",
		__func__, clk_get_rate(sclk_i2s));

	clk_put(sclk_i2s);
out3:
	clk_put(mout_audio);
out2:
	clk_put(mout_epll);
out1:
	clk_put(fout_epll);

	return ret;
}

#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
/*
 * ESPRESSO I2S DAI operations. (AP Master with dummy codec)
 */
static int espresso_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int pll, sclk, bfs, div, rfs, ret;
	unsigned long rclk;

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

	switch (params_rate(params)) {
	case 48000:
	case 96000:
	case 192000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 12288000:
	case 18432000:
		div = 4;
		break;
	case 24576000:
	case 36864000:
		div = 2;
		break;
	case 49152000:
	case 73728000:
		div = 1;
		break;
	default:
		pr_err("Not yet supported!\n");
		return -EINVAL;
	}

	/* Set AUD_PLL frequency */
	sclk = rclk;
	pll = sclk * div;
	set_aud_pll_rate(pll);

	/* Set SCLK */
	ret = set_aud_sclk(sclk);
	if (ret < 0)
		return ret;

	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, 0, 0);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	return 0;
}
#else
/*
 * ESPRESSO I2S DAI operations. (Codec Master)
 */
static int espresso_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret, aif1rate, fs;

	/* Set Codec FLL */
	aif1rate = params_rate(params);
	fs = (aif1rate >= 192000) ? 256 : 1024;

	set_aud_pll_rate(49152000);

	set_aud_sclk(card, 12288000);

	ret = snd_soc_codec_set_pll(codec, FLORIDA_FLL1_REFCLK,
				    ARIZONA_FLL_SRC_NONE, 0, 0);
	if (ret != 0) {
		dev_err(card->dev, "Failed to start FLL1 REF: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_pll(codec,
				    FLORIDA_FLL1,
				    ARIZONA_CLK_SRC_MCLK1,
				    ESPRESSO_DEFAULT_MCLK1,
				    aif1rate * fs);
	if (ret != 0) {
		dev_err(card->dev, "Failed to start FLL1: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_sysclk(codec,
				       ARIZONA_CLK_SYSCLK,
				       ARIZONA_CLK_SRC_FLL1,
				       aif1rate * fs,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev, "Failed to set SYSCLK to FLL1: %d\n", ret);

	ret = snd_soc_codec_set_sysclk(codec,
				       ARIZONA_CLK_ASYNCCLK,
				       ARIZONA_CLK_SRC_FLL2,
				       48000 * 1024,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev,
				 "Unable to set ASYNCCLK to FLL2: %d\n", ret);

	/* Set Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif1 codec fmt: %d\n", ret);
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

static const struct snd_kcontrol_new espresso_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
};

const struct snd_soc_dapm_widget espresso_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
	SND_SOC_DAPM_MIC("Main Mic", NULL),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),
};

const struct snd_soc_dapm_route espresso_dapm_routes[] = {
	{ "HP", NULL, "HPOUT1L" },
	{ "HP", NULL, "HPOUT1R" },
	{ "SPK", NULL, "SPKOUTLN" },
	{ "SPK", NULL, "SPKOUTLP" },
	{ "SPK", NULL, "SPKOUTRN" },
	{ "SPK", NULL, "SPKOUTRP" },
	{ "Main Mic", NULL, "MICBIAS1" },
	{ "Sub Mic", NULL, "MICBIAS2" },
	{ "IN1L", NULL, "Main Mic" },
	{ "IN2L", NULL, "Sub Mic" },
};

static struct snd_soc_ops espresso_ops = {
	.hw_params = espresso_hw_params,
};

#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER	/* for dummy codec */
static struct snd_soc_dai_link espresso_dai[] = {
	{
		.name = "WM5110 PRI",
		.stream_name = "i2s",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "dummy-aif1",
		.platform_name = "samsung-i2s.0",
		.codec_name = "dummy-codec",
		.ops = &espresso_ops,
	}
};
#else
static struct snd_soc_dai_link espresso_dai[] = {
	{
		.name = "WM5110 PRI",
		.stream_name = "i2s",
		.codec_dai_name = "florida-aif1",
		.codec_name = "florida-codec",
		.ops = &espresso_ops,
	}
};
#endif

static int espresso_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	int ret;

	snd_soc_dapm_ignore_suspend(&card->dapm, "HP");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SPK");
	snd_soc_dapm_sync(&card->dapm);

	ret = snd_soc_codec_set_sysclk(codec,
				       ARIZONA_CLK_SYSCLK,
				       ARIZONA_CLK_SRC_FLL1,
				       48000 * 1024,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev, "Failed to set SYSCLK to FLL1: %d\n", ret);

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_ASYNCCLK,
				       ARIZONA_CLK_SRC_FLL2,
				       48000 * 1024,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev, "Failed to set SYSCLK to FLL2: %d\n", ret);

	return 0;
}

static int espresso_suspend_post(struct snd_soc_card *card)
{
	return 0;
}

static int espresso_resume_pre(struct snd_soc_card *card)
{
	return 0;
}

static int espresso_set_sysclk(struct snd_soc_card *card, bool on)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	int ret;

	if (on) {

		ret = snd_soc_codec_set_pll(codec, FLORIDA_FLL1_REFCLK,
				ARIZONA_FLL_SRC_NONE, 0, 0);
		if (ret != 0) {
			dev_err(card->dev, "Failed to start FLL1 REF\n");
			return ret;
		}

		ret = snd_soc_codec_set_pll(codec,
				FLORIDA_FLL1,
				ARIZONA_CLK_SRC_MCLK1,
				ESPRESSO_DEFAULT_MCLK1,
				48000 * 1024);
		if (ret != 0) {
			dev_err(card->dev, "Failed to start FLL1\n");
			return ret;
		}
	} else {
		ret = snd_soc_codec_set_pll(codec,
				FLORIDA_FLL1,
				ARIZONA_CLK_SRC_MCLK1,
				ESPRESSO_DEFAULT_MCLK1,
				0);
		if (ret != 0) {
			dev_err(card->dev, "Failed to stop FLL1: %d\n", ret);
			return ret;
		}

		ret = snd_soc_codec_set_pll(codec, FLORIDA_FLL1_REFCLK,
				ARIZONA_FLL_SRC_NONE, 0, 0);
		if (ret != 0) {
			dev_err(card->dev, "Failed to start FLL1 REF\n");
			return ret;
		}

		ret = snd_soc_codec_set_pll(codec, FLORIDA_FLL1, 0, 0, 0);
		if (ret != 0) {
			dev_err(card->dev, "Failed to start FLL1\n");
			return ret;
		}
	}

	return 0;
}

static int espresso_set_mic_cp(struct snd_soc_card *card, bool on)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	int ret;

	ret = snd_soc_update_bits(codec, ARIZONA_MIC_CHARGE_PUMP_1,
			ARIZONA_CPMIC_ENA_MASK,
			on ? ARIZONA_CPMIC_ENA_MASK : 0);
	return ret;
}

static int espresso_set_bias_level(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;

	if (dapm->dev != aif1_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		if (card->dapm.bias_level == SND_SOC_BIAS_OFF) {
			espresso_enable_mclk(true);
			espresso_set_sysclk(card, true);
			espresso_set_mic_cp(card, true);

		}
		break;
	case SND_SOC_BIAS_OFF:
		espresso_set_sysclk(card, false);
		espresso_enable_mclk(false);
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	default:
		break;
	}

	card->dapm.bias_level = level;

	return 0;
}

static struct snd_soc_card espresso = {
	.name = "ESPRESSO-I2S",
	.owner = THIS_MODULE,

	.late_probe = espresso_late_probe,
	.suspend_post = espresso_suspend_post,
	.resume_pre = espresso_resume_pre,
	.set_bias_level = espresso_set_bias_level,

	.dai_link = espresso_dai,
	.num_links = ARRAY_SIZE(espresso_dai),

	.controls = espresso_controls,
	.num_controls = ARRAY_SIZE(espresso_controls),
	.dapm_widgets = espresso_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(espresso_dapm_widgets),
	.dapm_routes = espresso_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(espresso_dapm_routes),
};

static int espresso_audio_probe(struct platform_device *pdev)
{

	int ret;
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &espresso;

	card->dev = &pdev->dev;

	if (np) {

		if (!espresso_dai[0].cpu_dai_name) {
			espresso_dai[0].cpu_of_node = of_parse_phandle(np,
							"samsung,audio-cpu", 0);

			if (!espresso_dai[0].cpu_of_node) {
				dev_err(&pdev->dev,
				"Property 'samsung,audio-cpu' missing or invalid\n");
				ret = -EINVAL;
			}
		}

		if (!espresso_dai[0].platform_name)
			espresso_dai[0].platform_of_node = espresso_dai[0].cpu_of_node;
	}

	ret = snd_soc_register_card(card);

	if (ret)
		dev_err(&pdev->dev, "snd_soc_register_card() failed:%d\n", ret);

	return ret;
}

static int espresso_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id samsung_wm5110_of_match[] = {
	{ .compatible = "samsung,espresso-wm5110", },
	{},
};
MODULE_DEVICE_TABLE(of, samsung_wm5110_of_match);
#endif /* CONFIG_OF */

static struct platform_driver espresso_audio_driver = {
		.driver		= {
		.name	= "espresso-audio",
		.owner  = THIS_MODULE,
		.pm	= &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(samsung_wm5110_of_match),
	},
	.probe		= espresso_audio_probe,
	.remove		= espresso_audio_remove,
};

module_platform_driver(espresso_audio_driver);

MODULE_DESCRIPTION("ALSA SoC ESPRESSO WM5110");
MODULE_LICENSE("GPL");
