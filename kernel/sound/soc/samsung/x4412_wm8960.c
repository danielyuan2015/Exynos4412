#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <asm/mach-types.h>
#include <mach/map.h>
#include <mach/regs-audss.h>
#include "../codecs/wm8960.h"
#include "i2s.h"

/*
tinymix 6 127
tinymix 8 5
tinymix 9 5
tinymix 36 1
tinymix 39 1
tinyplay /sdcard/1.wav
*/
static int set_epll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return PTR_ERR(fout_epll);
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_set_rate(fout_epll, rate);
out:
	clk_put(fout_epll);

	return 0;
}

static int x4412_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int bfs, rfs, ret, psr;
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
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 4096000:
	case 5644800:
	case 6144000:
	case 8467200:
	case 9216000:
		psr = 8;
		break;
	case 8192000:
	case 11289600:
	case 12288000:
	case 16934400:
	case 18432000:
		psr = 4;
		break;
	case 22579200:
	case 24576000:
	case 33868800:
	case 36864000:
		psr = 2;
		break;
	case 67737600:
	case 73728000:
		psr = 1;
		break;
	default:
		printk(KERN_ERR "Not yet supported!\n");
		return -EINVAL;
	}

	set_epll_rate(rclk * psr);

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS); 
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_0, 0, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK, 0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * SMDK WM8580 DAI operations.
 */
static struct snd_soc_ops x4412_ops = {
	.hw_params = x4412_hw_params,
};

/* x4412v310 Playback widgets */
static const struct snd_soc_dapm_widget wm8960_dapm_widgets_pbk[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_SPK("Speaker_L", NULL),
	SND_SOC_DAPM_SPK("Speaker_R", NULL),
};

/* x4412v310 RX connections */
static const struct snd_soc_dapm_route audio_map_rx[] = {
	/* Headphone */
	{ "Headphone Jack", NULL, "HP_L" },
	{ "Headphone Jack", NULL, "HP_R" },

	/* Speaker */
	{ "Speaker_L", NULL, "SPK_LP" },
	{ "Speaker_L", NULL, "SPK_LN" },
	{ "Speaker_R", NULL, "SPK_RP" },
	{ "Speaker_R", NULL, "SPK_RN" },
};

/* x4412 Capture widgets */
static const struct snd_soc_dapm_widget wm8960_dapm_widgets_cpt[] = {
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_MIC("Mic main", NULL),
	SND_SOC_DAPM_LINE("Line Input 3 (FM)", NULL),
};

/* x4412 TX connections */
static const struct snd_soc_dapm_route audio_map_tx[] = {
	/* Line Input 3 (FM)*/
	{ "LINPUT3", NULL, "Line Input 3 (FM)" },
	{ "RINPUT3", NULL, "Line Input 3 (FM)" },

	{ "LINPUT2", NULL, "Mic main"},
	{ "RINPUT2", NULL, "Mic Jack"},
};

static const struct snd_soc_dapm_route audio_map_tx_htpc[] = {
	/* Line Input 3 (FM)*/
	{ "LINPUT3", NULL, "Line Input 3 (FM)" },
	{ "RINPUT3", NULL, "Line Input 3 (FM)" },

	{ "LINPUT1", NULL, "Mic main"},
	{ "LINPUT2", NULL, "Mic Jack"},
};

static int x4412_wm8960_init_paiftx(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	//snd_soc_dapm_new_controls(dapm, wm8960_dapm_widgets_cpt, ARRAY_SIZE(wm8960_dapm_widgets_cpt));
	snd_soc_dapm_add_routes(dapm, audio_map_tx_htpc, ARRAY_SIZE(audio_map_tx_htpc));
    snd_soc_dapm_add_routes(dapm, audio_map_tx, ARRAY_SIZE(audio_map_tx));
	snd_soc_dapm_sync(dapm);

	return 0;
}

static int x4412_wm8960_init_paifrx(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	snd_soc_dapm_new_controls(dapm, wm8960_dapm_widgets_pbk, ARRAY_SIZE(wm8960_dapm_widgets_pbk));
	snd_soc_dapm_add_routes(dapm, audio_map_rx, ARRAY_SIZE(audio_map_rx));
	snd_soc_dapm_sync(dapm);

	return 0;
}

enum {
	PRI_PLAYBACK = 0,
	PRI_CAPTURE,
	SEC_PLAYBACK,
};

static struct snd_soc_dai_link x4412_dai[] = {
	[PRI_PLAYBACK] = { /* Primary Playback i/f */
		.name = "WM8960 Playback",
		.stream_name = "Playback",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "wm8960-hifi",
#if defined(CONFIG_SND_SAMSUNG_NORMAL) || defined(CONFIG_SND_SAMSUNG_RP)
		.platform_name = "samsung-audio",		
#else
		.platform_name = "samsung-audio-idma",
#endif
		.codec_name = "wm8960-codec.0-001a",
		.init = x4412_wm8960_init_paifrx,
		.ops = &x4412_ops,
	},
	[PRI_CAPTURE] = { /* Primary Capture i/f */
		.name = "WM8960 PAIF TX",
		.stream_name = "Capture",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "wm8960-hifi",
#if defined(CONFIG_SND_SAMSUNG_NORMAL) || defined(CONFIG_SND_SAMSUNG_RP)
		.platform_name = "samsung-audio",		
#else
		.platform_name = "samsung-audio-idma",
#endif
		.codec_name = "wm8960-codec.0-001a",
		.init = x4412_wm8960_init_paiftx,
		.ops = &x4412_ops,
	},
};

static struct snd_soc_card x4412_card = {
	.name = "SMDK-I2S",
	.dai_link = x4412_dai,
	.num_links = ARRAY_SIZE(x4412_dai),
};

static struct platform_device * x4412_snd_device;

static int __init x4412_audio_init(void)
{
	int ret;

	x4412_snd_device = platform_device_alloc("soc-audio", -1);
	if (!x4412_snd_device)
		return -ENOMEM;

	platform_set_drvdata(x4412_snd_device, &x4412_card);

	ret = platform_device_add(x4412_snd_device);
	if (ret)
		platform_device_put(x4412_snd_device);

	return ret;
}
module_init(x4412_audio_init);

static void __exit x4412_audio_exit(void)
{
	platform_device_unregister(x4412_snd_device);
}
module_exit(x4412_audio_exit);

MODULE_AUTHOR("Jerryjianjun <jerryjianjun@gmail.com>");
MODULE_DESCRIPTION("ALSA SoC SMDK WM8960");
MODULE_LICENSE("GPL");
