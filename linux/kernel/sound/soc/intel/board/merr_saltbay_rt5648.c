/*
 *  merr_saltbay_rt5648.c - ASoc Machine driver for Intel Merrifield MID platform
 *  for the rt5648 codec
 *
 *  Copyright (C) 2012 Intel Corp
 *  Author: Vinod Koul <vinod.koul@linux.intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
// #define DEBUG 1

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/async.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/rpmsg.h>
#include <asm/intel_scu_pmic.h>
#include <asm/intel_mid_rpmsg.h>
#include <linux/platform_data/intel_mid_remoteproc.h>
#include <asm/platform_mrfld_audio.h>
#include <asm/intel_sst_mrfld.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/input.h>
#include "../../codecs/rt5648.h"


#define DEFAULT_MCLK       19200000
static unsigned int codec_clk_rate = DEFAULT_MCLK;

static int mrfld_set_clk_fmt(struct snd_soc_dai *codec_dai)
{
	unsigned int fmt;
	int ret;

	pr_debug("Enter in %s\n", __func__);

#if 0
	/* ALC5648  Slave Mode`*/
	fmt =   SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_IB_NF
		| SND_SOC_DAIFMT_CBS_CFS;
#else
	fmt =   SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF
		| SND_SOC_DAIFMT_CBS_CFS;

#endif
	/* Set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, fmt);

	if (ret < 0) {
		pr_err("can't set codec DAI configuration %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_pll(codec_dai, 0, RT5648_PLL1_S_MCLK,
				  DEFAULT_MCLK, 48000 * 512);
	if (ret < 0) {
		pr_err("can't set codec pll: %d\n", ret);
		return ret;
	}
	ret = snd_soc_dai_set_sysclk(codec_dai, RT5648_SCLK_S_PLL1,
					codec_clk_rate, SND_SOC_CLOCK_IN);

	if (ret < 0) {
		pr_err("can't set codec clock %d\n", ret);
		return ret;
	}
	return 0;
}

static int mrfld_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	codec_clk_rate = params_rate(params) * 512;
	return mrfld_set_clk_fmt(codec_dai);
}

static int mrfld_compr_set_params(struct snd_compr_stream *cstream)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	return mrfld_set_clk_fmt(codec_dai);
}

struct mrfld_mc_private {
	struct snd_soc_jack jack;
	int jack_retry;
};

enum gpios {
	MRFLD_HSDET,
	MRFLD_HOOKDET,
	NUM_HS_GPIOS,
};

#ifdef CONFIG_HEADSET_FUNCTION
extern struct snd_soc_jack_gpio hs_gpio[];
#else
static int mrfld_jack_gpio_detect(void);
static int mrfld_jack_gpio_detect_bp(void);

static struct snd_soc_jack_gpio hs_gpio[] = {
	[MRFLD_HSDET] = {
		.name			= "JACK_DET_FILTR",
		.report			= SND_JACK_HEADSET,
		.debounce_time		= 50,
		.jack_status_check	= mrfld_jack_gpio_detect,
		.irq_flags		= IRQF_TRIGGER_FALLING |
					  IRQF_TRIGGER_RISING,
		.invert			= 1,
	},
	[MRFLD_HOOKDET] = {
		.name			= "HOOK_DET",
		.report			= SND_JACK_HEADSET | SND_JACK_BTN_0,
		.debounce_time		= 100,
		.jack_status_check	= mrfld_jack_gpio_detect_bp,
		.irq_flags		= IRQF_TRIGGER_FALLING |
					  IRQF_TRIGGER_RISING,
	},
};

static int mrfld_jack_gpio_detect_bp(void)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio[MRFLD_HOOKDET];
	struct snd_soc_jack *jack = gpio->jack;
	//struct snd_soc_codec *codec = jack->codec;

	int enable, hs_status, status = 0;

	pr_debug("enter %s\n", __func__);
	status = jack->status;
	enable = gpio_get_value(gpio->gpio);
	if (gpio->invert)
		enable = !enable;

	/* Check for headset status before processing interrupt */
	gpio = &hs_gpio[MRFLD_HSDET];
	hs_status = gpio_get_value(gpio->gpio);
	if (gpio->invert)
		hs_status = !hs_status;
	pr_debug("in %s hook detect = 0x%x, headset detect = 0x%x\n",
			__func__, enable, hs_status);
	pr_debug("in %s jack status = 0x%x\n", __func__, jack->status);
	if (((jack->status & SND_JACK_HEADSET) == SND_JACK_HEADSET)
						&& (hs_status)) {
		if (enable) {
			status = SND_JACK_HEADSET | SND_JACK_BTN_0;
		} else {
			status = SND_JACK_HEADSET;
		}
	} else {
		pr_debug("%s:Spurious BP interrupt : jack_status 0x%x, HS_status 0x%x\n",
				__func__, jack->status, hs_status);
	}
	pr_debug("leave %s: status 0x%x\n", __func__, status);

	return status;
}

static int mrfld_jack_gpio_detect(void)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio[MRFLD_HSDET];
	struct snd_soc_jack *jack = gpio->jack;
	struct snd_soc_codec *codec = jack->codec;

	int enable, status = 0;

	enable = gpio_get_value(gpio->gpio);
	if (gpio->invert)
		enable = !enable;
	pr_debug("%s:gpio->%d=0x%d\n", __func__, gpio->gpio, enable);

	if (enable) {
		status = rt5648_headset_detect(codec, 1);
		pr_debug("%s:headset insert, status %d\n", __func__, status);
	} else {
		status = rt5648_headset_detect(codec, 0);
		pr_debug("%s:headset removal, status %d\n", __func__, status);
	}

	return status;
}
#endif

static int mrfld_set_bias_level(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_STANDBY:
		card->dapm.bias_level = level;
		break;
	case SND_SOC_BIAS_OFF:
		break;
	default:
		pr_err("%s: Invalid bias level=%d\n", __func__, level);
		return -EINVAL;
		break;
	}
	pr_debug("card(%s)->bias_level %u\n", card->name,
			card->dapm.bias_level);
	return 0;
}

static inline struct snd_soc_codec *mrfld_get_codec(struct snd_soc_card *card)
{
	bool found = false;
	struct snd_soc_codec *codec;

	list_for_each_entry(codec, &card->codec_dev_list, card_list) {
		if (!strstr(codec->name, "rt5648.1-001a")) {
			pr_debug("codec was %s", codec->name);
			continue;
		} else {
			pr_debug("found codec %s\n", codec->name);
			found = true;
			break;
		}
	}
	if (found == false) {
		pr_err("%s: cant find codec", __func__);
		return NULL;
	}
	return codec;
}

static int mrfld_set_bias_level_post(struct snd_soc_card *card,
				     struct snd_soc_dapm_context *dapm,
				     enum snd_soc_bias_level level)
{
	struct snd_soc_codec *codec;

	codec = mrfld_get_codec(card);
	if (!codec)
		return -EIO;

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_STANDBY:
		/* Processing already done during set_bias_level()
		 * callback. No action required here.
		 */
		break;
	case SND_SOC_BIAS_OFF:
		if (codec->dapm.bias_level != SND_SOC_BIAS_OFF)
			break;
		card->dapm.bias_level = level;
		break;
	default:
		return -EINVAL;
	}
	pr_debug("%s:card(%s)->bias_level %u\n", __func__, card->name,
			card->dapm.bias_level);
	return 0;
}

/* ALC5648 widgets */
static const struct snd_soc_dapm_widget widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Int Mic", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
};

/* ALC5648 Audio Map */
static const struct snd_soc_dapm_route map[] = {
	/* {"micbias1", NULL, "Headset Mic"},
	{"IN1P", NULL, "micbias1"}, */
	{"IN1P", NULL, "Headset Mic"},
	{"DMIC1", NULL, "Int Mic"},
	{"Headphone", NULL, "HPOL"},
	{"Headphone", NULL, "HPOR"},
	{"Ext Spk", NULL, "SPOL"},
	{"Ext Spk", NULL, "SPOR"},
};

static const struct snd_kcontrol_new ssp_comms_controls[] = {
		SOC_DAPM_PIN_SWITCH("Headphone"),
		SOC_DAPM_PIN_SWITCH("Headset Mic"),
		SOC_DAPM_PIN_SWITCH("Ext Spk"),
		SOC_DAPM_PIN_SWITCH("Int Mic"),
};

static int mrfld_init(struct snd_soc_pcm_runtime *runtime)
{
	int ret;
	struct snd_soc_codec *codec = runtime->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_card *card = runtime->card;
	//struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;
	struct mrfld_mc_private *ctx = snd_soc_card_get_drvdata(card);

	pr_debug("Entry %s\n", __func__);

#if 0
	/* TODO: switch back to I2S mode once LPE ready */
	ret = snd_soc_dai_set_tdm_slot(aif1_dai, 1, 1, 4, 24);
	if (ret < 0) {
		pr_err("%s: can't set codec pcm format %d\n", __func__, ret);
		return ret;
	}
#endif
	mrfld_set_bias_level(card, dapm, SND_SOC_BIAS_OFF);
	card->dapm.idle_bias_off = true;

	/* FIXME
	 * set all the nc_pins, set all the init control
	 * and add any machine controls here
	 */

	printk("%s: card name: %s, snd_card: %s, codec: %p\n",
			__func__, codec->card->name, codec->card->snd_card->longname, codec);
	ctx->jack_retry = 0;
	ret = snd_soc_jack_new(codec, "Intel MID Audio Jack",
			       SND_JACK_HEADSET | SND_JACK_HEADPHONE |
			       SND_JACK_BTN_0,
			       &ctx->jack);
	if (ret) {
		pr_err("jack creation failed\n");
		return ret;
	}

	pr_info("%s: hs_gpio array size %d\n", __func__, NUM_HS_GPIOS);
	ret = snd_soc_jack_add_gpios(&ctx->jack, NUM_HS_GPIOS, hs_gpio);
	if (ret) {
		pr_err("adding jack GPIO failed\n");
		return ret;
	}

	snd_jack_set_key(ctx->jack.jack, SND_JACK_BTN_0, KEY_MEDIA);

	ret = snd_soc_add_card_controls(card, ssp_comms_controls,
				ARRAY_SIZE(ssp_comms_controls));
	if (ret) {
		pr_err("Add Comms Controls failed %d",
				ret);
		return ret;
	}

	/* Keep the voice call paths active during
	suspend. Mark the end points ignore_suspend */
	snd_soc_dapm_ignore_suspend(dapm, "HPOL");
	snd_soc_dapm_ignore_suspend(dapm, "HPOR");

	snd_soc_dapm_ignore_suspend(dapm, "SPOL");
	snd_soc_dapm_ignore_suspend(dapm, "SPOR");

	snd_soc_dapm_enable_pin(dapm, "Headset Mic");
	snd_soc_dapm_enable_pin(dapm, "Headphone");
	snd_soc_dapm_enable_pin(dapm, "Ext Spk");
	snd_soc_dapm_enable_pin(dapm, "Int Mic");

	mutex_lock(&codec->mutex);
	snd_soc_dapm_sync(dapm);
	mutex_unlock(&codec->mutex);

	return 0;
}

static unsigned int rates_8000_16000[] = {
	8000,
	16000,
};

static struct snd_pcm_hw_constraint_list constraints_8000_16000 = {
	.count = ARRAY_SIZE(rates_8000_16000),
	.list  = rates_8000_16000,
};

static unsigned int rates_48000[] = {
	48000,
};

static struct snd_pcm_hw_constraint_list constraints_48000 = {
	.count = ARRAY_SIZE(rates_48000),
	.list  = rates_48000,
};

static int mrfld_startup(struct snd_pcm_substream *substream)
{
	return snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			&constraints_48000);
}

static struct snd_soc_ops mrfld_ops = {
	.startup = mrfld_startup,
	.hw_params = mrfld_hw_params,
};

static int mrfld_8k_16k_startup(struct snd_pcm_substream *substream)
{
	return snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			&constraints_8000_16000);
}

static struct snd_soc_ops mrfld_8k_16k_ops = {
	.startup = mrfld_8k_16k_startup,
	.hw_params = mrfld_hw_params,
};

static struct snd_soc_compr_ops mrfld_compr_ops = {
	.set_params = mrfld_compr_set_params,
};

struct snd_soc_dai_link mrfld_rt5648_msic_dailink[] = {
	[MERR_SALTBAY_AUDIO] = {
		.name = "Merrifield Audio Port",
		.stream_name = "Audio",
		.cpu_dai_name = "Headset-cpu-dai",
		.codec_dai_name = "rt5648-aif1",
		.codec_name = "rt5648.1-001a",
		.platform_name = "sst-platform",
		.init = mrfld_init,
		.ignore_suspend = 1,
		.ops = &mrfld_ops,
		.playback_count = 3,
	},
	[MERR_SALTBAY_COMPR] = {
		.name = "Merrifield Compress Port",
		.stream_name = "Compress",
		.cpu_dai_name = "Compress-cpu-dai",
		.codec_dai_name = "rt5648-aif1",
		.codec_name = "rt5648.1-001a",
		.platform_name = "sst-platform",
		.init = NULL,
		.ignore_suspend = 1,
		.compr_ops = &mrfld_compr_ops,
	},
	[MERR_SALTBAY_VOIP] = {
		.name = "Merrifield VOIP Port",
		.stream_name = "Voip",
		.cpu_dai_name = "Voip-cpu-dai",
		.codec_dai_name = "rt5648-aif1",
		.codec_name = "rt5648.1-001a",
		.platform_name = "sst-platform",
		.init = NULL,
		.ignore_suspend = 1,
		.ops = &mrfld_8k_16k_ops,
	},
	[MERR_SALTBAY_PROBE] = {
		.name = "Merrifield Probe Port",
		.stream_name = "Probe",
		.cpu_dai_name = "Probe-cpu-dai",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.platform_name = "sst-platform",
		.playback_count = 8,
		.capture_count = 8,
	},
	[MERR_SALTBAY_AWARE] = {
		.name = "Merrifield Aware Port",
		.stream_name = "Aware",
		.cpu_dai_name = "Loopback-cpu-dai",
		.codec_dai_name = "rt5648-aif1",
		.codec_name = "rt5648.1-001a",
		.platform_name = "sst-platform",
		.init = NULL,
		.ignore_suspend = 1,
		.ops = &mrfld_8k_16k_ops,
	},
	[MERR_SALTBAY_VAD] = {
		.name = "Merrifield VAD Port",
		.stream_name = "Vad",
		.cpu_dai_name = "Loopback-cpu-dai",
		.codec_dai_name = "rt5648-aif1",
		.codec_name = "rt5648.1-001a",
		.platform_name = "sst-platform",
		.init = NULL,
		.ignore_suspend = 1,
		.ops = &mrfld_8k_16k_ops,
	},
	[MERR_SALTBAY_POWER] = {
		.name = "Virtual Power Port",
		.stream_name = "Power",
		.cpu_dai_name = "Power-cpu-dai",
		.platform_name = "sst-platform",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
};

#ifdef CONFIG_PM_SLEEP
static int snd_mrfld_prepare(struct device *dev)
{
	pr_debug("In %s device name\n", __func__);
	snd_soc_suspend(dev);
	return 0;
}

static void snd_mrfld_complete(struct device *dev)
{
	pr_debug("In %s\n", __func__);
	snd_soc_resume(dev);
}

static int snd_mrfld_poweroff(struct device *dev)
{
	pr_debug("In %s\n", __func__);
	snd_soc_poweroff(dev);
	return 0;
}
#else
#define snd_mrfld_prepare NULL
#define snd_mrfld_complete NULL
#define snd_mrfld_poweroff NULL
#endif

/* SoC card */
static struct snd_soc_card snd_soc_card_mrfld = {
	.name = "rt5648-audio",
	.dai_link = mrfld_rt5648_msic_dailink,
	.num_links = ARRAY_SIZE(mrfld_rt5648_msic_dailink),
	.set_bias_level_post = mrfld_set_bias_level_post,
	.dapm_widgets = widgets,
	.num_dapm_widgets = ARRAY_SIZE(widgets),
	.dapm_routes = map,
	.num_dapm_routes = ARRAY_SIZE(map),
};

static int snd_mrfld_mc_probe(struct platform_device *pdev)
{
	int ret_val = 0;
	struct mrfld_mc_private *drv;
	struct mrfld_audio_platform_data *pdata;

	pr_debug("Entry %s\n", __func__);

	drv = kzalloc(sizeof(*drv), GFP_ATOMIC);
	if (!drv) {
		pr_err("allocation failed\n");
		return -ENOMEM;
	}
	pdata = pdev->dev.platform_data;
	/*TODO: remove hardcode before IFWI ready */
#if 0 /* Mofd VV board */
	hs_gpio[MRFLD_HSDET].gpio = 48;
	hs_gpio[MRFLD_HOOKDET].gpio = 49;
#else /* EVB */
	hs_gpio[MRFLD_HSDET].gpio = 58;
	hs_gpio[MRFLD_HOOKDET].gpio = 57;
#endif
	/* register the soc card */
	snd_soc_card_mrfld.dev = &pdev->dev;
	snd_soc_card_set_drvdata(&snd_soc_card_mrfld, drv);
	ret_val = snd_soc_register_card(&snd_soc_card_mrfld);
	if (ret_val) {
		pr_err("snd_soc_register_card failed %d\n", ret_val);
		goto unalloc;
	}
	platform_set_drvdata(pdev, &snd_soc_card_mrfld);
	return ret_val;

unalloc:
	kfree(drv);
	return ret_val;
}

static int snd_mrfld_mc_remove(struct platform_device *pdev)
{
	struct snd_soc_card *soc_card = platform_get_drvdata(pdev);
	struct mrfld_mc_private *drv = snd_soc_card_get_drvdata(soc_card);

	snd_soc_jack_free_gpios(&drv->jack, NUM_HS_GPIOS, hs_gpio);
	kfree(drv);
	pr_debug("In %s\n", __func__);
	snd_soc_card_set_drvdata(soc_card, NULL);
	snd_soc_unregister_card(soc_card);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static void snd_mrfld_mc_shutdown(struct platform_device *pdev)
{
	struct snd_soc_card *soc_card = platform_get_drvdata(pdev);
	struct mrfld_mc_private *drv = snd_soc_card_get_drvdata(soc_card);

	snd_soc_jack_free_gpios(&drv->jack, NUM_HS_GPIOS, hs_gpio);
	pr_debug("In %s\n", __func__);
}

const struct dev_pm_ops snd_mrfld_rt5648_mc_pm_ops = {
	.prepare = snd_mrfld_prepare,
	.complete = snd_mrfld_complete,
	.poweroff = snd_mrfld_poweroff,
};

static struct platform_driver snd_mrfld_mc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "mrfld_rt5648",
		.pm = &snd_mrfld_rt5648_mc_pm_ops,
	},
	.probe = snd_mrfld_mc_probe,
	.remove = snd_mrfld_mc_remove,
	.shutdown = snd_mrfld_mc_shutdown,
};

static int snd_mrfld_driver_init(void)
{
	pr_info("Merrifield Machine Driver mrfld_rt5648 registerd\n");
	return platform_driver_register(&snd_mrfld_mc_driver);
}

static void snd_mrfld_driver_exit(void)
{
	pr_debug("In %s\n", __func__);
	platform_driver_unregister(&snd_mrfld_mc_driver);
}

static int snd_mrfld_rpmsg_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;

	pr_info("In %s\n", __func__);

	if (rpdev == NULL) {
		pr_err("rpmsg channel not created\n");
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed snd_mrfld rpmsg device\n");

	ret = snd_mrfld_driver_init();

out:
	return ret;
}

static void snd_mrfld_rpmsg_remove(struct rpmsg_channel *rpdev)
{
	snd_mrfld_driver_exit();
	dev_info(&rpdev->dev, "Removed snd_mrfld rpmsg device\n");
}

static void snd_mrfld_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
				int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
			data, len,  true);
}

static struct rpmsg_device_id snd_mrfld_rpmsg_id_table[] = {
	{ .name = "rpmsg_mrfld_rt5648_audio" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, snd_mrfld_rpmsg_id_table);

static struct rpmsg_driver snd_mrfld_rpmsg = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= snd_mrfld_rpmsg_id_table,
	.probe		= snd_mrfld_rpmsg_probe,
	.callback	= snd_mrfld_rpmsg_cb,
	.remove		= snd_mrfld_rpmsg_remove,
};

static int __init snd_mrfld_rpmsg_init(void)
{
	return register_rpmsg_driver(&snd_mrfld_rpmsg);
}
late_initcall(snd_mrfld_rpmsg_init);

static void __exit snd_mrfld_rpmsg_exit(void)
{
	return unregister_rpmsg_driver(&snd_mrfld_rpmsg);
}
module_exit(snd_mrfld_rpmsg_exit);

MODULE_DESCRIPTION("ASoC Intel(R) Merrifield MID Machine driver");
MODULE_AUTHOR("Vinod Koul <vinod.koul@linux.intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:mrfld-audio");
