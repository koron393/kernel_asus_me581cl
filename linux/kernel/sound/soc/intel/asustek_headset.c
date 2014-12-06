/*
 *  Headset device detection driver.
 *
 * Copyright (C) 2011 ASUSTek Corporation.
 *
 * Authors:
 *  Jason Cheng <jason4_cheng@asus.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/switch.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <asm/string.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/workqueue.h>
#include "../codecs/rt5648.h"

MODULE_DESCRIPTION("Headset detection driver");
MODULE_LICENSE("GPL");


/*----------------------------------------------------------------------------
 ** FUNCTION DECLARATION
 **----------------------------------------------------------------------------*/
struct button_work {
	struct delayed_work work;
	int key;
};
static int __init		headset_init(void);
static void __exit		headset_exit(void);
static int			detection_work(void);
static int			button_work(void);
static void			button_work_late(struct work_struct*);
static bool			is_btn_available(int status);
int				hs_micbias_power(int on);
int				hs_get_status(void);
static struct workqueue_struct *g_button_workqueue_late;
static struct button_work g_button_work_late[4];
/*----------------------------------------------------------------------------
 ** GLOBAL VARIABLES
 **----------------------------------------------------------------------------*/
enum {
	HS_JACK = 0,
	HS_HOOK,
};

struct snd_soc_jack_gpio hs_gpio[] = {
	[HS_JACK] = {
		.name			= "JACK_DET_FILTR",
		.gpio			= 58,
		.report			= SND_JACK_HEADSET |
			SND_JACK_HEADPHONE | SND_JACK_LINEOUT,
		.debounce_time		= 500,
		.jack_status_check	= detection_work,
		.irq_flags		= IRQF_TRIGGER_FALLING |
			IRQF_TRIGGER_RISING,
	},
	[HS_HOOK] = {
		.name			= "HOOK_DET",
		.gpio			= 57,
		.report			= SND_JACK_BTN_0,
		.debounce_time		= 50,
		.jack_status_check	= button_work,
		.irq_flags		= IRQF_TRIGGER_FALLING |
			IRQF_TRIGGER_RISING,
	},
};

EXPORT_SYMBOL(hs_gpio);

enum {
	OFF,
	ON,
};

struct headset_data {
	struct switch_dev sdev;
	struct wake_lock detection_lock;
	int next_bw_idx;
	bool ignore_btn;
};
static struct headset_data hs_data;


extern struct snd_soc_codec *audio_codec;

int hs_get_status(void) {
	const struct snd_soc_jack* jack = hs_gpio[HS_JACK].jack;
	// Report hs status to codec bias control.
	return jack?(jack->status & hs_gpio[HS_JACK].report):-1;
}

EXPORT_SYMBOL(hs_get_status);

static ssize_t headset_name_show(struct switch_dev *sdev, char *buf)
{
	switch (hs_get_status()){
		case 0:
			return sprintf(buf, "NO DEVICE\n");
		case SND_JACK_HEADSET:
			return sprintf(buf, "HEADSET\n");
		case SND_JACK_HEADPHONE:
			return sprintf(buf, "HEADPHONE\n");
	}
	return -EINVAL;
}

static ssize_t headset_state_show(struct switch_dev *sdev, char *buf)
{
	switch (hs_get_status()){
		case 0:
			return sprintf(buf, "%d\n", 0);
		case SND_JACK_HEADSET:
			return sprintf(buf, "%d\n", 1);
		case SND_JACK_HEADPHONE:
			return sprintf(buf, "%d\n", 2);
	}
	return -EINVAL;
}

static int insert_headset(void)
{
	if (gpio_get_value_cansleep(hs_gpio[HS_HOOK].gpio)) {
		printk("%s: headphone\n", __func__);
		return SND_JACK_HEADPHONE;
	} else {
		printk("%s: headset\n", __func__);
		return SND_JACK_HEADSET;
	}
}
// static void detection_work(struct work_struct *work)
static int detection_work(void)
{
	int jk_status, last_jack;
	int status = hs_get_status();
	int waited = 0, retries = 0;
	const int DEBOUNCING = 350;
	const int WAIT_CODEC_WAKE = 20;
	const int MAX_WAIT_TIME = 10 * WAIT_CODEC_WAKE;
	const int MAX_RETRIES = 10;

	printk("%s: detection\n", __func__);
	/* prevent system from falling asleep again as we sleep */
	wake_lock(&hs_data.detection_lock);

	while(audio_codec->suspended) {
		msleep(WAIT_CODEC_WAKE);
		if ((waited += WAIT_CODEC_WAKE) >= MAX_WAIT_TIME) {
			pr_err("HEADSET: codec didn't waked in time\n");
			break;
		}
	}
	// Hooks here are likely just bouncing.
	hs_data.ignore_btn = true;
	hs_micbias_power(ON);

	/* Delay for hook status stable. */
	jk_status = gpio_get_value_cansleep(hs_gpio[HS_JACK].gpio);
	do {
		msleep(10);
		waited += 10;
		last_jack = gpio_get_value_cansleep(hs_gpio[HS_JACK].gpio);
		if (jk_status != last_jack) {
			printk("%s: garbage bouncing.\n", __func__);
			waited = 0;
			jk_status = last_jack;
			++retries;
		}
	} while ((waited < DEBOUNCING) && (retries < MAX_RETRIES));

	if (retries == MAX_RETRIES)
		pr_debug("%s: Too musch debouncing.\n", __func__);

	pr_debug("hs_jack status %d\n", jk_status);
	if (jk_status != 0) {
		/* Headset not plugged in */
		printk("%s: remove headset\n", __func__);
		status = 0;
	} else if (status == 0) {
		status = insert_headset();
	} else
		pr_info("%s: do not detect again\n", __func__);

	/* Don't close micbias if we're using amic */
	if ((status & SND_JACK_HEADSET) != SND_JACK_HEADSET)
		hs_micbias_power(OFF);
	else
		hs_data.ignore_btn = false;
	wake_unlock(&hs_data.detection_lock);
	pr_debug("%s: report_key 0x%x\n", __func__, status);
	return status;
}

static bool is_btn_available(int status)
{
    int jk_status = gpio_get_value_cansleep(hs_gpio[HS_JACK].gpio);

    pr_debug("%s: jk_status 0x%x, status 0x%x, ignore_btn %d\n",
	    __func__, jk_status, status, hs_data.ignore_btn);
    return !hs_data.ignore_btn && jk_status == 0 && ((status & SND_JACK_HEADSET) == SND_JACK_HEADSET);
}

static int button_work(void)
{
	const int TIME_TO_REPORT = 300;
	const int NUM_BUTTON_WORKS =
		sizeof(g_button_work_late)/ sizeof(g_button_work_late[0]);
	struct snd_soc_jack *jack = hs_gpio[HS_HOOK].jack;
	int status = jack->status;
	int k = hs_data.next_bw_idx;
	struct button_work* bw = g_button_work_late + k;
	hs_data.next_bw_idx = (k + 1) % NUM_BUTTON_WORKS;
	/* Check for headset status before processing interrupt */

	if (is_btn_available(status)) {
		bw->key = gpio_get_value_cansleep(hs_gpio[HS_HOOK].gpio);
		pr_debug("leave %s: status 0x%x\n", __func__, status);
		wake_lock(&hs_data.detection_lock);
		queue_delayed_work(g_button_workqueue_late, &(bw->work),
				msecs_to_jiffies(TIME_TO_REPORT));
	} else {
		status = 0;
	}

	return status;
}

static void button_work_late(struct work_struct*w)
{
	struct snd_soc_jack *jack = hs_gpio[HS_HOOK].jack;
	int status = jack->status;
	struct delayed_work* dw = container_of(w, struct delayed_work, work);
	struct button_work* bw = container_of(dw, struct button_work, work);

	if (is_btn_available(status)) {
		if (bw->key) { // set btn bit.
			printk("Pressed\n");
			status |= SND_JACK_BTN_0;
		} else { // clear btn bit.
			printk("Releaseed\n");
			status &= ~SND_JACK_BTN_0;
		}
		snd_soc_jack_report(jack, status, hs_gpio[HS_HOOK].report);
	}
	wake_unlock(&hs_data.detection_lock);
}

int hs_micbias_power(int status)
{
	int ret = -1;
	unsigned bits;
	struct snd_soc_codec*codec = audio_codec;

	if (codec == NULL)
		goto failed;

	pr_debug("HEADSET: Turn %s micbias power\n",
			(status == ON)? "on": "off");

	bits = RT5648_PWR_LDO2 ;
	ret = snd_soc_update_bits(codec, RT5648_PWR_MIXER, bits,
			(status == ON)? bits: 0);
	if (ret < 0)
		goto failed;

	if (status == OFF &&
			codec->dapm.bias_level != SND_SOC_BIAS_OFF) {
		pr_debug("HEADSET: Codec turned it on, leave as was.");
		return 1;
	}

	bits = RT5648_PWR_BG | RT5648_PWR_VREF2 |
		RT5648_PWR_VREF1 | RT5648_PWR_MB;
	ret = snd_soc_update_bits(codec, RT5648_PWR_ANLG1, bits,
			status == ON? bits: 0);
	if (ret < 0)
		goto failed;

	bits = RT5648_PWR_MB1;
	ret = snd_soc_update_bits(codec, RT5648_PWR_ANLG2, bits,
			status == ON? bits: 0);
	if (ret < 0)
		goto failed;
	return ret;
failed:
	pr_err("HEADSET: Codec update failure!\n");
	return ret;
}

EXPORT_SYMBOL(hs_micbias_power);

/**********************************************************
 **  Function: Headset driver init function
 **  Parameter: none
 **  Return value: none
 **
 ************************************************************/
static int __init headset_init(void)
{
	int ret;
	int i;
	printk(KERN_INFO "%s+ #####\n", __func__);

	wake_lock_init(&hs_data.detection_lock, WAKE_LOCK_SUSPEND, "hs_wakelock");

	g_button_workqueue_late = create_workqueue("button_work_late");
	for (i = 0; i < sizeof(g_button_work_late)/sizeof(g_button_work_late[0]); ++i) {
		INIT_DELAYED_WORK(&(g_button_work_late[i].work), button_work_late);
		g_button_work_late[i].key = 0;
	}

	hs_data.sdev.name = "h2w";
	hs_data.sdev.print_name = headset_name_show;
	hs_data.sdev.print_state = headset_state_show;
	hs_data.next_bw_idx = 0;
	hs_data.ignore_btn = true;

	ret = switch_dev_register(&hs_data.sdev);
	if (ret < 0)
		pr_err("Warning: test node creation failed.");

	printk(KERN_INFO "%s- #####\n", __func__);
	return 0;
}

/**********************************************************
 **  Function: Headset driver exit function
 **  Parameter: none
 **  Return value: none
 **
 ************************************************************/
static void __exit headset_exit(void)
{
	switch_dev_unregister(&hs_data.sdev);
}

late_initcall(headset_init);
module_exit(headset_exit);
