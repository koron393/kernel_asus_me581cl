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
#include <linux/lnw_gpio.h>
#include <linux/intel_mid_pm.h>

#include "mdfld_dsi_dpi.h"
#include "mdfld_dsi_pkg_sender.h"
#include "displays/innolux_n070iceg02_vid.h"

#define N070ICEG02_WIDTH  94
#define N070ICEG02_HEIGHT 151

#define N070ICEG02_LCM_1V8_EN_GPIO  174		//1.8V power
#define N070ICEG02_LCM_3V3_EN_GPIO  189			//3.3V power
#define N070ICEG02_BL_EN_GPIO   188
#define N070ICEG02_BL_PWM_GPIO  183

#define PWMCTRL_REG 0xFF013C00

union pwmctrl_reg {
	struct {
		u32 pwmtd:8;
		u32 pwmbu:22;
		u32 pwmswupdate:1;
		u32 pwmenable:1;
	} part;
	u32 full;
};

static int lcm_rst_gpio;
static int lcm_1V8_en_gpio;
static int lcm_3V3_en_gpio;
static int bl_en_gpio;
static int bl_pwm_gpio;
static u32 __iomem *pwmctrl_mmio;

static int
innolux_n070iceg02_vid_drv_ic_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	u8 data;

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}
	mdfld_dsi_read_mcs_lp(sender, 0x0A, &data, 1);

	msleep(40);
	gpio_set_value_cansleep(lcm_rst_gpio, 1);
	usleep_range(1000, 1500);
	gpio_set_value_cansleep(lcm_rst_gpio, 0);
	usleep_range(1000, 1500);
	gpio_set_value_cansleep(lcm_rst_gpio, 1);
	msleep(20);
	{
		u8 buf[] = {0xFF, 0xAA, 0x55, 0xA5, 0x80};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 5,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0x6F, 0x11, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xF7, 0x20, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0x6F, 0x06, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	mdfld_dsi_send_mcs_short_lp(sender, 0xF7, 0xA0, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	mdfld_dsi_send_mcs_short_lp(sender, 0x6F, 0x19, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	mdfld_dsi_send_mcs_short_lp(sender, 0xF7, 0x12, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0xC8, 0x80, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xB1, 0x6C, 0x01};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0xB6, 0x08, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	mdfld_dsi_send_mcs_short_lp(sender, 0x6F, 0x02, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	mdfld_dsi_send_mcs_short_lp(sender, 0xB8, 0x08, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xBB, 0x54, 0x54};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBC, 0x05, 0x05};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0xC7, 0x01, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xBD, 0x02, 0xB0, 0x0C, 0x0A, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB0, 0x05, 0x05};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB1, 0x05, 0x05};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBC, 0x8E, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBD, 0x92, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0xCA, 0x00, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	mdfld_dsi_send_mcs_short_lp(sender, 0xC0, 0x04, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xB3, 0x19, 0x19};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB4, 0x12, 0x12};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB9, 0x24, 0x24};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBA, 0x14, 0x14};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x02};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0xEE, 0x02, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xEF, 0x09, 0x06, 0x15, 0x18};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 5,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB0, 0x00, 0x00, 0x00, 0x11, 0x00, 0x27};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 7,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0x6F, 0x06, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xB0, 0x00, 0x36, 0x00, 0x45, 0x00, 0x5F};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 7,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0x6F, 0x0C, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xB0, 0x00, 0x74, 0x00, 0xA5};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 5,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB1, 0x00, 0xCF, 0x01, 0x13, 0x01, 0x47};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 7,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0x6F, 0x06, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xB1, 0x01, 0x98, 0x01, 0xDF, 0x01, 0xE1};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 7,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0x6F, 0x0C, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xB1, 0x02, 0x23, 0x02, 0x6C};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 5,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB2, 0x02, 0x9A, 0x02, 0xD7, 0x03, 0x05};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 7,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0x6F, 0x06, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xB2, 0x03, 0x42, 0x03, 0x68, 0x03, 0x91};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 7,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0x6F, 0x0C, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xB2, 0x03, 0xA5, 0x03, 0xBD};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 5,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB3, 0x03, 0xD7, 0x03, 0xFF};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 5,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBC, 0x00, 0x00, 0x00, 0x11, 0x00, 0x27};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 7,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0x6F, 0x06, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xBC, 0x00, 0x38, 0x00, 0x47, 0x00, 0x61};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 7,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0x6F, 0x0C, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xBC, 0x00, 0x78, 0x00, 0xAB};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 5,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBD, 0x00, 0xD7, 0x01, 0x1B, 0x01, 0x4F};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 7,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0x6F, 0x06, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xBD, 0x01, 0xA1, 0x01, 0xE5, 0x01, 0xE7};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 7,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0x6F, 0x0C, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xBD, 0x02, 0x27, 0x02, 0x70};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 5,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBE, 0x02, 0x9E, 0x02, 0xDB, 0x03, 0x07};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 7,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0x6F, 0x06, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xBE, 0x03, 0x44, 0x03, 0x6A, 0x03, 0x93};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 7,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0x6F, 0x0C, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xBE, 0x03, 0xA5, 0x03, 0xBD};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 5,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBF, 0x03, 0xD7, 0x03, 0xFF};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 5,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x06};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB0, 0x00, 0x17};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB1, 0x16, 0x15};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB2, 0x14, 0x13};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
	u8 buf[] = {0xB3, 0x12, 0x11};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB4, 0x10, 0x2D};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB5, 0x01, 0x08};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB6, 0x09, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB7, 0x31, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB8, 0x31, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB9, 0x31, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBA, 0x31, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBB, 0x31, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBC, 0x31, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBD, 0x31, 0x09};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBE, 0x08, 0x01};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBF, 0x2D, 0x10};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xC0, 0x11, 0x12};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xC1, 0x13, 0x14};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xC2, 0x15, 0x16};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xC3, 0x17, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xE5, 0x31, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xC4, 0x00, 0x17};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xC5, 0x16, 0x15};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xC6, 0x14, 0x13};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xC7, 0x12, 0x11};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xC8, 0x10, 0x2D};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xC9, 0x01, 0x08};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xCA, 0x09, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xCB, 0x31, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xCC, 0x31, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xCD, 0x31, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xCE, 0x31, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xCF, 0x31, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xD0, 0x31, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xD1, 0x31, 0x09};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xD2, 0x08, 0x01};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xD3, 0x2D, 0x10};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xD4, 0x11, 0x12};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xD5, 0x13, 0x14};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xD6, 0x15, 0x16};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xD7, 0x17, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xE6, 0x31, 0x31};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xD8, 0x00, 0x00, 0x00, 0x00, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xD9, 0x00, 0x00, 0x00, 0x00, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0xE7, 0x00, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x03};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB0, 0x20, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB1, 0x20, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB2, 0x05, 0x00, 0x42, 0x00, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB6, 0x05, 0x00, 0x42, 0x00, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBA, 0x53, 0x00, 0x42, 0x00, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBB, 0x53, 0x00, 0x42, 0x00, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0xC4, 0x40, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x05};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB0, 0x17, 0x06};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0xB8, 0x00, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xBD, 0x03, 0x01, 0x01, 0x00, 0x01};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB1, 0x17, 0x06};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB9, 0x00, 0x01};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB2, 0x17, 0x06};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBA, 0x00, 0x01};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB3, 0x17, 0x06};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBB, 0x0A, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB4, 0x17, 0x06};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB5, 0x17, 0x06};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB6, 0x14, 0x03};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xB7, 0x00, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xBC, 0x02, 0x01};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0xC0, 0x05, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	mdfld_dsi_send_mcs_short_lp(sender, 0xC4, 0xA5, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	{
		u8 buf[] = {0xC8, 0x03, 0x30};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xC9, 0x03, 0x51};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 3,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xD1, 0x00, 0x05, 0x03, 0x00, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	{
		u8 buf[] = {0xD2, 0x00, 0x05, 0x09, 0x00, 0x00};
		mdfld_dsi_send_mcs_long_lp(sender, buf, 6,
					   MDFLD_DSI_SEND_PACKAGE);
	}
	mdfld_dsi_send_mcs_short_lp(sender, 0xE5, 0x02, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	mdfld_dsi_send_mcs_short_lp(sender, 0xE6, 0x02, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	mdfld_dsi_send_mcs_short_lp(sender, 0xE7, 0x02, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	mdfld_dsi_send_mcs_short_lp(sender, 0xE9, 0x02, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	mdfld_dsi_send_mcs_short_lp(sender, 0xED, 0x33, 1,
				    MDFLD_DSI_SEND_PACKAGE);

	return 0;
}

static void
innolux_n070iceg02_vid_controller_init(struct mdfld_dsi_config *dsi_config)
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
	hw_ctx->mipi_control = 0x18;
	hw_ctx->intr_en = 0xFFFFFFFF;
	hw_ctx->hs_tx_timeout = 0xFFFFFF;
	hw_ctx->lp_rx_timeout = 0xFFFFFF;
	hw_ctx->device_reset_timer = 0xFFFF;
	hw_ctx->turn_around_timeout = 0x3F;
	hw_ctx->high_low_switch_count = 0x18;
	hw_ctx->clk_lane_switch_time_cnt = 0x1E000D;
	hw_ctx->lp_byteclk = 0x3;
	hw_ctx->dphy_param = 0x190D340B;
	hw_ctx->eot_disable = 0x2;
	hw_ctx->init_count = 0x7D0;
	hw_ctx->dsi_func_prg = (RGB_888_FMT << FMT_DPI_POS) |
		dsi_config->lane_count;
	hw_ctx->mipi = MIPI_PORT_EN | PASS_FROM_SPHY_TO_AFE |
		BANDGAP_CHICKEN_BIT;
	hw_ctx->video_mode_format = 0xF;
}

static int innolux_n070iceg02_vid_power_on(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	mdfld_dsi_send_mcs_short_lp(sender, MIPI_DCS_EXIT_SLEEP_MODE, 0, 0,
				    MDFLD_DSI_SEND_PACKAGE);
	mdfld_dsi_send_mcs_short_lp(sender, MIPI_DCS_SET_DISPLAY_ON, 0, 0,
				    MDFLD_DSI_SEND_PACKAGE);
	pmu_set_pwm(PCI_D0);
	return 0;
}

static int innolux_n070iceg02_vid_power_off(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}
	pmu_set_pwm(PCI_D3hot);
	mdfld_dsi_send_mcs_short_lp(sender, MIPI_DCS_SET_DISPLAY_OFF, 0, 0,
				    MDFLD_DSI_SEND_PACKAGE);
	mdfld_dsi_send_mcs_short_lp(sender, MIPI_DCS_ENTER_SLEEP_MODE, 0, 0,
				    MDFLD_DSI_SEND_PACKAGE);
	msleep(100);

	if (lcm_rst_gpio)
		gpio_set_value_cansleep(lcm_rst_gpio, 0);

	if (lcm_1V8_en_gpio)
		gpio_set_value_cansleep(lcm_1V8_en_gpio, 0);

	if (lcm_3V3_en_gpio)
	gpio_set_value_cansleep(lcm_3V3_en_gpio, 0);

	return 0;
}

static int
innolux_n070iceg02_vid_set_brightness(struct mdfld_dsi_config *dsi_config,
				      int level)
{
	u32 reg_level;
	union pwmctrl_reg pwmctrl;

	/* Backlight level in register setting is reversed */
	reg_level = ~level & 0xFF;

	pwmctrl.part.pwmswupdate = 0x1;
	pwmctrl.part.pwmbu = 0x1000;
	pwmctrl.part.pwmtd = reg_level;

	if (!pwmctrl_mmio)
		pwmctrl_mmio = ioremap_nocache(PWMCTRL_REG, 4);

	if (pwmctrl_mmio) {
		if (level) {
			if (!gpio_get_value(bl_en_gpio)) {
				msleep(134);
				gpio_set_value_cansleep(bl_en_gpio, 1);
				lnw_gpio_set_alt(bl_pwm_gpio, 1);
			}

			pwmctrl.part.pwmenable = 0x1;
			writel(pwmctrl.full, pwmctrl_mmio);
		} else {
			pwmctrl.part.pwmenable = 0;
			writel(pwmctrl.full, pwmctrl_mmio);
			gpio_set_value_cansleep(bl_en_gpio, 0);
			lnw_gpio_set_alt(bl_pwm_gpio, 0);
		}
	} else {
		DRM_ERROR("Cannot map pwmctrl\n");
	}

	return 0;
}

static int
innolux_n070iceg02_vid_panel_reset(struct mdfld_dsi_config *dsi_config)
{
	if (!lcm_1V8_en_gpio) {
		lcm_1V8_en_gpio = N070ICEG02_LCM_1V8_EN_GPIO;
		if (gpio_request(lcm_1V8_en_gpio, "lcm_1V8_en")) {
			DRM_ERROR("Faild to request LCM enable 1V8 gpio\n");
			return -EINVAL;
		}

		gpio_direction_output(lcm_1V8_en_gpio, 0);
	}

	if (!lcm_3V3_en_gpio) {
		lcm_3V3_en_gpio = N070ICEG02_LCM_3V3_EN_GPIO;
		if (gpio_request(lcm_3V3_en_gpio, "lcm_3V3_en")) {
			DRM_ERROR("Faild to request LCM enable 3V3 gpio\n");
			return -EINVAL;
		}

		gpio_direction_output(lcm_3V3_en_gpio, 0);
	}

	if (!bl_en_gpio) {
		bl_en_gpio = N070ICEG02_BL_EN_GPIO;
		if (gpio_request(bl_en_gpio, "backlight_en")) {
			DRM_ERROR("Faild to request backlight enable gpio\n");
			return -EINVAL;
		}

		gpio_direction_output(bl_en_gpio, 0);
	}

	if (!bl_pwm_gpio) {
		bl_pwm_gpio = N070ICEG02_BL_PWM_GPIO;
		if (gpio_request(bl_pwm_gpio, "backlight_pwm")) {
		        DRM_ERROR("Faild to request backlight PWM gpio\n");
		        return -EINVAL;
		}

		lnw_gpio_set_alt(bl_pwm_gpio, 1);
	}

	if (!lcm_rst_gpio) {
		lcm_rst_gpio = get_gpio_by_name("disp0_rst");
		if (lcm_rst_gpio < 0) {
			DRM_ERROR("Faild to get panel reset gpio\n");
			return -EINVAL;
		}

		if (gpio_request(lcm_rst_gpio, "lcm_reset")) {
			DRM_ERROR("Faild to request LCM reset gpio\n");
			return -EINVAL;
		}

		gpio_direction_output(lcm_rst_gpio, 0);
	}

	gpio_set_value_cansleep(lcm_rst_gpio, 0);
	gpio_set_value_cansleep(lcm_1V8_en_gpio, 0);
	gpio_set_value_cansleep(lcm_1V8_en_gpio, 1);
	mdelay(5);
	gpio_set_value_cansleep(lcm_3V3_en_gpio, 0);
	gpio_set_value_cansleep(lcm_3V3_en_gpio, 1);

	return 0;
}

static struct drm_display_mode *innolux_n070iceg02_vid_get_config_mode(void)
{
	struct drm_display_mode *mode = kzalloc(sizeof(*mode), GFP_KERNEL);

	if (!mode)
		return NULL;

	mode->hdisplay = 800;
	mode->hsync_start = mode->hdisplay + 32;
	mode->hsync_end = mode->hsync_start + 2;
	mode->htotal = mode->hsync_end + 32;

	mode->vdisplay = 1280;
	mode->vsync_start = mode->vdisplay + 7;
	mode->vsync_end = mode->vsync_start + 2;
	mode->vtotal = mode->vsync_end + 8;

	mode->vrefresh = 60;
	mode->clock =  mode->vrefresh * mode->vtotal * mode->htotal / 1000;
	mode->type |= DRM_MODE_TYPE_PREFERRED;

	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);
	return mode;
}

static void
innolux_n070iceg02_vid_get_panel_info(int pipe, struct panel_info *pi)
{
	if (!pi) {
		DRM_ERROR("Invalid parameters\n");
		return;
	}

	pi->width_mm = N070ICEG02_WIDTH;
	pi->height_mm = N070ICEG02_HEIGHT;
}

void
innolux_n070iceg02_vid_init(struct drm_device *dev, struct panel_funcs *p_funcs)
{
	if (!p_funcs) {
		DRM_ERROR("Invalid parameters\n");
		return;
	}

	lcm_rst_gpio = 0;
	lcm_1V8_en_gpio = 0;
	lcm_3V3_en_gpio = 0;
	bl_en_gpio = 0;
	bl_pwm_gpio = 0;

	p_funcs->reset = innolux_n070iceg02_vid_panel_reset;
	p_funcs->power_on = innolux_n070iceg02_vid_power_on;
	p_funcs->power_off = innolux_n070iceg02_vid_power_off;
	p_funcs->drv_ic_init = innolux_n070iceg02_vid_drv_ic_init;
	p_funcs->get_config_mode = innolux_n070iceg02_vid_get_config_mode;
	p_funcs->get_panel_info = innolux_n070iceg02_vid_get_panel_info;
	p_funcs->dsi_controller_init = innolux_n070iceg02_vid_controller_init;
	p_funcs->set_brightness = innolux_n070iceg02_vid_set_brightness;
}
