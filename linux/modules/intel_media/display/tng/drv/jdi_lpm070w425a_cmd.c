/*
 * Copyright © 2014 Intel Corporation
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
 * Ben Kao <ben.kao@intel.com>
 */

#include <video/mipi_display.h>

#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_pkg_sender.h"
#include "displays/jdi_lpm070w425a_cmd.h"

#define LPM070W425A_WIDTH  95
#define LPM070W425A_HEIGHT 151

static int panel_reset_gpio;
static int panel_en_gpio;
static int backlight_en_gpio;

static u8 gen_mcap[] = {0xB0, 0x00};
static u8 gen_interface_setting[] = {0xB3, 0x04, 0x08, 0x00, 0x22, 0x00};
static u8 gen_interface_id_setting[] = {0xB4, 0x0C};
static u8 gen_dsi_control[] = {0xB6, 0x3A, 0xD3};
static u8 mcs_set_column_address[] = {0x2A, 0x00, 0x00, 0x04, 0xAF};
static u8 mcs_set_page_address[] = {0x2B, 0x00, 0x00, 0x07, 0x7F};
static u8 LTPS_timing_setting[2] = {0xC6, 0x75}; // 0x76(59.3fps), 0x90(49fps), 0xFF(27fps)
static u8 sequencer_timing_control[2] = {0xD6, 0x01};

static int jdi_lpm070w425a_cmd_drv_ic_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int err;

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	err = mdfld_dsi_send_mcs_short_hs(sender, MIPI_DCS_SOFT_RESET, 0, 0,
					  MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("Failed to Soft Reset\n");
		return err;
	}

	usleep_range(5000, 5500);
	err = mdfld_dsi_send_gen_short_hs(sender, gen_mcap[0], gen_mcap[1], 2,
					  MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("Failed to Send MCAP\n");
		return err;
	}

	err = mdfld_dsi_send_gen_long_hs(sender, gen_interface_setting, 6, 0);
	if (err) {
		DRM_ERROR("Failed to Send Interface Setting\n");
		return err;
	}

	err = mdfld_dsi_send_gen_long_hs(sender, gen_interface_id_setting, 2,
					 0);
	if (err) {
		DRM_ERROR("Failed to Send Interface ID Setting\n");
		return err;
	}

	err = mdfld_dsi_send_gen_long_hs(sender, gen_dsi_control, 3, 0);
	if (err) {
		DRM_ERROR("Failed to Send DSI control\n");
		return err;
	}

	err = mdfld_dsi_send_gen_short_hs(sender, LTPS_timing_setting[0],
			LTPS_timing_setting[1], 2,
			MDFLD_DSI_SEND_PACKAGE);

	if (err) {
		DRM_ERROR("%s: %d: Panel LTPS_timing_setting\n", __func__, __LINE__);
		return err;
	}

	err = mdfld_dsi_send_gen_short_hs(sender, sequencer_timing_control[0],
			sequencer_timing_control[1], 2,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Panel sequencer_timing_control\n", __func__, __LINE__);
		return err;
	}

	err = mdfld_dsi_send_mcs_short_hs(sender, write_display_brightness, 4,
					  1, MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("Failed to Set Brightness\n");
		return err;
	}

	err = mdfld_dsi_send_mcs_short_hs(sender, write_ctrl_display, 0x24, 1,
					  MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("Failed to Write Control Display\n");
		return err;
	}

	err = mdfld_dsi_send_mcs_short_hs(sender, MIPI_DCS_SET_PIXEL_FORMAT,
					  0x77, 1, MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("Failed to Set Pixel format\n");
		return err;
	}

	err = mdfld_dsi_send_mcs_short_hs(sender, MIPI_DCS_SET_TEAR_ON, 0, 1,
					  MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("Failed to Set Tear On\n");
		return err;
	}

	err = mdfld_dsi_send_mcs_long_hs(sender, mcs_set_column_address, 5,
					 MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("Failed to Set Column Address\n");
		return err;
	}

	err = mdfld_dsi_send_mcs_long_hs(sender, mcs_set_page_address, 5,
					 MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("Failed to Set Page Address\n");
		return err;
	}

	return 0;
}

static void
jdi_lpm070w425a_cmd_controller_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_hw_context *hw_ctx;

	if (!dsi_config || !(&dsi_config->dsi_hw_context)) {
		DRM_ERROR("Invalid parameters\n");
		return;
	}

	dsi_config->lane_count = 4;
	dsi_config->lane_config = MDFLD_DSI_DATA_LANE_4_0;

	hw_ctx = &dsi_config->dsi_hw_context;
	hw_ctx->cck_div = 1;
	hw_ctx->pll_bypass_mode = 0;
	hw_ctx->mipi_control = 0x0;
	hw_ctx->intr_en = 0xFFFFFFFF;
	hw_ctx->hs_tx_timeout = 0xFFFFFF;
	hw_ctx->lp_rx_timeout = 0xFFFFFF;
	hw_ctx->device_reset_timer = 0xFFFF;
	hw_ctx->turn_around_timeout = 0x3F;
	hw_ctx->high_low_switch_count = 0x2F;
	hw_ctx->clk_lane_switch_time_cnt = 0x420015;
	hw_ctx->lp_byteclk = 0x7;
	hw_ctx->dphy_param = 0x2C1E791F;
	hw_ctx->eot_disable = 0x2;
	hw_ctx->init_count = 0x7D0;
	hw_ctx->dbi_bw_ctrl = 0x400;
	hw_ctx->hs_ls_dbi_enable = 0x0;
	hw_ctx->dsi_func_prg = ((DBI_DATA_WIDTH_OPT2 << 13) |
		dsi_config->lane_count);
	hw_ctx->mipi = PASS_FROM_SPHY_TO_AFE | BANDGAP_CHICKEN_BIT |
		TE_TRIGGER_GPIO_PIN;
	hw_ctx->video_mode_format = 0xF;
}

static int jdi_lpm070w425a_cmd_power_on(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int err;

	pr_info("%s +\n", __func__);

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}


	err = mdfld_dsi_send_mcs_short_hs(sender, MIPI_DCS_EXIT_SLEEP_MODE, 0,
					  0, MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("Failed to Exit Sleep Mode\n");
		return err;
	}

	msleep(120);
	err = mdfld_dsi_send_mcs_short_hs(sender, MIPI_DCS_SET_DISPLAY_ON, 0,
					  0, MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("Faild to Set Display On\n");
		return err;
	}

	pr_info("%s -\n", __func__);

	return 0;
}

static int jdi_lpm070w425a_cmd_power_off(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int err;

	pr_info("%s +\n", __func__);

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}


	err = mdfld_dsi_send_mcs_short_hs(sender, MIPI_DCS_SET_DISPLAY_OFF, 0,
					  0, MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("Failed to Set Display Off\n");
		return err;
	}

	msleep(20);
	err = mdfld_dsi_send_mcs_short_hs(sender, MIPI_DCS_SET_TEAR_OFF, 0, 0,
					  MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("Failed to Set Tear OFF\n");
		return err;
	}

	err = mdfld_dsi_send_mcs_short_hs(sender, MIPI_DCS_ENTER_SLEEP_MODE, 0,
					  0, MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("Failed to Enter Sleep Mode\n");
		return err;
	}

	msleep(80);
	gpio_set_value_cansleep(panel_en_gpio, 0);
	msleep(20);
	gpio_set_value_cansleep(panel_reset_gpio, 0);

	pr_info("%s -\n", __func__);

	return 0;
}

static int
jdi_lpm070w425a_cmd_set_brightness(struct mdfld_dsi_config *dsi_config,
				   int level)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	u8 duty_val = 0;
	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	if (level && !gpio_get_value(backlight_en_gpio))
		gpio_set_value_cansleep(backlight_en_gpio, 1);
	else if (!level && gpio_get_value(backlight_en_gpio))
		gpio_set_value_cansleep(backlight_en_gpio, 0);

	duty_val = 0xFF & level;

	pr_info("%s: %d\n", __func__, duty_val);

	mdfld_dsi_send_mcs_short_hs(sender, write_display_brightness, duty_val,
				    1, MDFLD_DSI_SEND_PACKAGE);

	return 0;
}

static int jdi_lpm070w425a_cmd_panel_reset(struct mdfld_dsi_config *dsi_config)
{

	pr_info("%s +\n", __func__);

	if (!panel_en_gpio) {
		panel_en_gpio = 189;
		if (gpio_request(panel_en_gpio, "panel_en")) {
			DRM_ERROR("Faild to request panel enable gpio\n");
			return -EINVAL;
		}

		gpio_direction_output(panel_en_gpio, 0);
	}

	if (!panel_reset_gpio) {
		panel_reset_gpio = get_gpio_by_name("disp0_rst");
		if (panel_reset_gpio < 0) {
			DRM_ERROR("Faild to get panel reset gpio\n");
			return -EINVAL;
		}

		if (gpio_request(panel_reset_gpio, "panel_reset")) {
			DRM_ERROR("Faild to request panel reset gpio\n");
			return -EINVAL;
		}

		gpio_direction_output(panel_reset_gpio, 0);
	}

	if (!backlight_en_gpio) {
		backlight_en_gpio = 188;
		if (gpio_request(backlight_en_gpio, "backlight_en")) {
			DRM_ERROR("Faild to request backlight enable gpio\n");
			return -EINVAL;
		}

		gpio_direction_output(backlight_en_gpio, 0);
	}

	gpio_set_value_cansleep(backlight_en_gpio, 0);
	gpio_set_value_cansleep(panel_reset_gpio, 0);
	gpio_set_value_cansleep(panel_en_gpio, 0);

	pr_info("%s -\n", __func__);

	return 0;
}

static int
jdi_lpm070w425a_cmd_exit_deep_standby(struct mdfld_dsi_config *dsi_config)
{
	pr_info("%s +\n", __func__);

	if (!panel_en_gpio || !panel_reset_gpio)
		return -EINVAL;

	gpio_direction_output(panel_en_gpio, 0);
	gpio_direction_output(panel_reset_gpio, 0);
	gpio_set_value_cansleep(panel_en_gpio, 1);
	msleep(20);
	gpio_set_value_cansleep(panel_reset_gpio, 1);
	usleep_range(10000, 10500);

	pr_info("%s -\n", __func__);

	return 0;
}

static struct drm_display_mode *jdi_lpm070w425a_cmd_get_config_mode(void)
{
	struct drm_display_mode *mode = kzalloc(sizeof(*mode), GFP_KERNEL);

	if (!mode)
		return NULL;

	mode->hdisplay = 1200;
	mode->hsync_start = mode->hdisplay;
	mode->hsync_end = mode->hsync_start;
	mode->htotal = mode->hsync_end;

	mode->vdisplay = 1920;
	mode->vsync_start = mode->vdisplay;
	mode->vsync_end = mode->vsync_start;
	mode->vtotal = mode->vsync_end;

	mode->vrefresh = 60;
	mode->clock =  166666;
	mode->type |= DRM_MODE_TYPE_PREFERRED;

	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);

	return mode;
}

static void jdi_lpm070w425a_cmd_get_panel_info(int pipe, struct panel_info *pi)
{
	if (!pi) {
		DRM_ERROR("Invalid parameters\n");
		return;
	}

	pi->width_mm = LPM070W425A_WIDTH;
	pi->height_mm = LPM070W425A_HEIGHT;
}

void
jdi_lpm070w425a_cmd_init(struct drm_device *dev, struct panel_funcs *p_funcs)
{
	if (!p_funcs) {
		DRM_ERROR("Invalid parameters\n");
		return;
	}

	panel_reset_gpio = 0;
	panel_en_gpio = 0;
	backlight_en_gpio = 0;

	p_funcs->reset = jdi_lpm070w425a_cmd_panel_reset;
	p_funcs->power_on = jdi_lpm070w425a_cmd_power_on;
	p_funcs->power_off = jdi_lpm070w425a_cmd_power_off;
	p_funcs->drv_ic_init = jdi_lpm070w425a_cmd_drv_ic_init;
	p_funcs->get_config_mode = jdi_lpm070w425a_cmd_get_config_mode;
	p_funcs->get_panel_info = jdi_lpm070w425a_cmd_get_panel_info;
	p_funcs->dsi_controller_init = jdi_lpm070w425a_cmd_controller_init;
	p_funcs->set_brightness = jdi_lpm070w425a_cmd_set_brightness;
	p_funcs->exit_deep_standby = jdi_lpm070w425a_cmd_exit_deep_standby;
}
