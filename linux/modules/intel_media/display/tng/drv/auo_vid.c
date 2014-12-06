/*
 * Copyright (C) 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 * Austin Hu <austin.hu@intel.com>
 */

#include <asm/intel_scu_pmic.h>
#include <asm/intel_mid_rpmsg.h>
#include <asm/intel_mid_remoteproc.h>
#include <linux/lnw_gpio.h>
#include <linux/intel_mid_pm.h>

#include "displays/auo_vid.h"

#define PWMCTRL_REG	0xFF013C00

static int lcm_reset_gpio;
static int lcm_en_gpio;
static int backlight_en_gpio;
static int backlight_pwm_gpio;
static u32 __iomem *pwmctrl_mmio;

union pwmctrl_reg {
	struct {
		u32 pwmtd:8;
		u32 pwmbu:22;
		u32 pwmswupdate:1;
		u32 pwmenable:1;
	} part;
	u32 full;
};

static int
mdfld_dsi_auo_vid_drv_ic_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	/* Set RE0=24h */
	mdfld_dsi_send_mcs_short_lp(sender, 0xE0, 0x24, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	/* Set RE1=24h */
	mdfld_dsi_send_mcs_short_lp(sender, 0xE1, 0x24, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	/* Set RE2=36h */
	mdfld_dsi_send_mcs_short_lp(sender, 0xE2, 0x36, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	/* Set RE3=36h */
	mdfld_dsi_send_mcs_short_lp(sender, 0xE3, 0x36, 1,
				    MDFLD_DSI_SEND_PACKAGE);

	return 0;
}

static
void mdfld_dsi_auo_dsi_controller_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_hw_context *hw_ctx =
		&dsi_config->dsi_hw_context;
	int mipi_vc = 0;
	int mipi_pixel_format = 0x4; //RGB888

	PSB_DEBUG_ENTRY("\n");

	/* Reconfig lane configuration */
	dsi_config->lane_count = 4;
	dsi_config->lane_config = MDFLD_DSI_DATA_LANE_4_0;

	hw_ctx->pll_bypass_mode = 0;

	/* This is for 400 mhz.  Set it to 0 for 800mhz */
	hw_ctx->cck_div = 0;

	hw_ctx->mipi_control = 0x18;
	hw_ctx->intr_en = 0xFFFFFFFF;
	hw_ctx->hs_tx_timeout = 0xFFFFFF;
	hw_ctx->lp_rx_timeout = 0xFFFFFF;

	hw_ctx->turn_around_timeout = 0x3F;
	hw_ctx->device_reset_timer = 0xFFFF;
	hw_ctx->high_low_switch_count = 0x32;
	hw_ctx->clk_lane_switch_time_cnt = 0x440018;
	hw_ctx->dbi_bw_ctrl = 0x0;
	hw_ctx->eot_disable = 0x3 | 1<<9;
	hw_ctx->init_count = 0x7D0;
	hw_ctx->lp_byteclk = 0x6;
	hw_ctx->dphy_param = 0x3D1D851F;

	/* Setup video mode format */
	hw_ctx->video_mode_format = 0xF;

	/* Set up func_prg */
	hw_ctx->dsi_func_prg = ((mipi_pixel_format << 7) | (mipi_vc << 3) |
			dsi_config->lane_count);

	/* Setup mipi port configuration */
	hw_ctx->mipi = MIPI_PORT_EN | PASS_FROM_SPHY_TO_AFE |
		BANDGAP_CHICKEN_BIT;
}

static int mdfld_dsi_auo_power_on(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);

	pr_info("%s+\n", __func__);

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	pmu_set_pwm(PCI_D0);

	pr_info("%s-\n", __func__);
	return 0;
}

static int mdfld_dsi_auo_power_off(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);

	pr_info("%s+\n", __func__);

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	pmu_set_pwm(PCI_D3hot);

	msleep(250);

	if (lcm_reset_gpio)
		gpio_set_value_cansleep(lcm_reset_gpio, 0);

	msleep(120);

	if (lcm_en_gpio)
		gpio_set_value_cansleep(lcm_en_gpio, 0);

	pr_info("%s-\n", __func__);
	return 0;
}

static int mdfld_dsi_auo_set_brightness(struct mdfld_dsi_config *dsi_config,
		int level)
{
	u32 reg_level;
	union pwmctrl_reg pwmctrl;
	static int sleep_control;
	int ret;

	if (backlight_en_gpio == 0) {
		backlight_en_gpio = 188;
		ret = gpio_request(backlight_en_gpio, "bl_enable");
		if (ret) {
			DRM_ERROR("Faild to request bl_enable gpio\n");
			return -EINVAL;
		}
	}

	if (backlight_pwm_gpio == 0) {
		backlight_pwm_gpio = 183;
		ret = gpio_request(backlight_pwm_gpio, "bl_pwm");
		if (ret) {
		        DRM_ERROR("Faild to request bl_pwm gpio\n");
		        return -EINVAL;
		}
	}

	/* Backlight level in register setting is reversed */
	reg_level = ~level & 0xFF;

	pwmctrl.part.pwmswupdate = 0x1;
	pwmctrl.part.pwmbu = 0x100;	// 1 kHz
	pwmctrl.part.pwmtd = reg_level;

	pr_info("%s: brightness level=%d, reg_level=0x%x\n", __func__, level, reg_level);

	if (pwmctrl_mmio == 0)
		pwmctrl_mmio = ioremap_nocache(PWMCTRL_REG, 4);

	if (pwmctrl_mmio != 0) {
		if (level) {
			if (!sleep_control) {
				pr_info("%s: sleep 200 ms when resume\n", __func__);
				msleep(200);
				gpio_set_value_cansleep(backlight_en_gpio, 1);
				lnw_gpio_set_alt(backlight_pwm_gpio, 1);
				sleep_control = 1;
			}
			pwmctrl.part.pwmenable = 0x1;
			writel(pwmctrl.full, pwmctrl_mmio);
		}
		else {
			pwmctrl.part.pwmenable = 0;
			writel(pwmctrl.full, pwmctrl_mmio);
			if (sleep_control) {
				pr_info("%s: set brightness to 0 when suspend\n", __func__);
				gpio_set_value_cansleep(backlight_en_gpio, 0);
				lnw_gpio_set_alt(backlight_pwm_gpio, 0);
				sleep_control = 0;
			}
		}
	}
	else
		DRM_ERROR("Cannot map pwmctrl\n");

	return 0;
}

static int mdfld_dsi_auo_panel_reset(struct mdfld_dsi_config *dsi_config)
{
	pr_info("%s+\n", __func__);

	gpio_set_value_cansleep(lcm_en_gpio, 1);
	usleep_range(1000, 1500);
	gpio_set_value_cansleep(lcm_reset_gpio, 1);
	usleep_range(1000, 1500);
	gpio_set_value_cansleep(lcm_reset_gpio, 0);
	usleep_range(10, 50);
	gpio_set_value_cansleep(lcm_reset_gpio, 1);
	usleep_range(5000, 6000);

	pr_info("%s-\n", __func__);
	return 0;
}

static struct drm_display_mode *auo_vid_get_config_mode(void)
{
	struct drm_display_mode *mode;

	PSB_DEBUG_ENTRY("\n");

	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	mode->hdisplay = 1200;
	mode->hsync_start = mode->hdisplay + 86;//HFP
	mode->hsync_end = mode->hsync_start + 4;//HPW
	mode->htotal = mode->hsync_end + 86;//HBP

	mode->vdisplay = 1920;
	mode->vsync_start = mode->vdisplay + 12;//VFP
	mode->vsync_end = mode->vsync_start + 2;//VPW
	mode->vtotal = mode->vsync_end + 4;//VBP

	mode->vrefresh = 60;
	mode->clock =  mode->vrefresh * mode->vtotal *
		mode->htotal / 1000;

	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);

	mode->type |= DRM_MODE_TYPE_PREFERRED;

	return mode;
}

static void auo_vid_get_panel_info(int pipe, struct panel_info *pi)
{
	if (!pi)
		return;

	if (pipe == 0) {
		pi->width_mm = AUO_PANEL_WIDTH;
		pi->height_mm = AUO_PANEL_HEIGHT;
	}

	return;
}

int auo_vid_detect(struct mdfld_dsi_config *dsi_config)
{
	dsi_config->dsi_hw_context.panel_on = true;
	pmu_set_pwm(PCI_D0);

	return MDFLD_DSI_PANEL_CONNECTED;
}

void auo_vid_init(struct drm_device *dev, struct panel_funcs *p_funcs)
{
	int ret = 0;

	PSB_DEBUG_ENTRY("\n");
	pr_info("%s+\n", __func__);

	if (lcm_en_gpio == 0) {
		lcm_en_gpio = 189;
		ret = gpio_request(lcm_en_gpio, "lcm_en");
		if (ret) {
			DRM_ERROR("Faild to request bias_enable gpio\n");
			return -EINVAL;
		}
	}

	if (lcm_reset_gpio == 0) {
		ret = get_gpio_by_name("disp0_rst");
		if (ret < 0) {
			DRM_ERROR("Faild to get panel reset gpio, " \
				  "use default reset pin\n");
			return -EINVAL;
		}
		lcm_reset_gpio = ret;
		ret = gpio_request(lcm_reset_gpio, "lcm_reset");
		if (ret) {
			DRM_ERROR("Faild to request panel reset gpio\n");
			return -EINVAL;
		}
	}

	p_funcs->get_config_mode = auo_vid_get_config_mode;
	p_funcs->get_panel_info = auo_vid_get_panel_info;
	p_funcs->reset = mdfld_dsi_auo_panel_reset;
	p_funcs->dsi_controller_init = mdfld_dsi_auo_dsi_controller_init;
	p_funcs->drv_ic_init = mdfld_dsi_auo_vid_drv_ic_init;
	p_funcs->power_on = mdfld_dsi_auo_power_on;
	p_funcs->power_off = mdfld_dsi_auo_power_off;
	p_funcs->set_brightness = mdfld_dsi_auo_set_brightness;
	p_funcs->detect = auo_vid_detect;

	pr_info("%s-\n", __func__);
}
