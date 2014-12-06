/*
 * platform_mt9m114.c: mt9m114 platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/atomisp_platform.h>
#include <asm/intel-mid.h>
#include <asm/intel_scu_ipcutil.h>
#include <media/v4l2-subdev.h>
#include <linux/regulator/consumer.h>
#include <linux/sfi.h>
#include <linux/mfd/intel_mid_pmic.h>
#include <linux/vlv2_plat_clock.h>
#include "platform_camera.h"
#include "platform_mt9m114.h"
#include <linux/acpi_gpio.h>
#include <linux/lnw_gpio.h>

#define CAMERA_1_RESET 10

static int camera_reset;

/*
 * MFLD PR2 secondary camera sensor - MT9M114 platform data
 */
static int mt9m114_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;
	int pin;

	pr_info("%s - E, flag: %d\n", __func__, flag);

	if (camera_reset < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_1_RESET,
				GPIOF_DIR_OUT, 1);
		if (ret < 0)
			return ret;
		camera_reset = ret;
		/* set camera reset pin mode to gpio */
		lnw_gpio_set_alt(camera_reset, LNW_GPIO);
	}

	if (flag) {
		gpio_set_value(camera_reset, 1);
	} else {
		gpio_set_value(camera_reset, 0);
		gpio_free(camera_reset);
		camera_reset = -1;
	}

	return 0;
}

static int mt9m114_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	static const unsigned int clock_khz = 19200;
	int ret = 0;

	pr_info("%s - E, flag: %d\n", __func__, flag);

	if (intel_mid_identify_cpu() != INTEL_MID_CPU_CHIP_VALLEYVIEW2)
		return intel_scu_ipc_osc_clk(OSC_CLK_CAM1,
					     flag ? clock_khz : 0);
	return ret;
}

static int mt9m114_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret = 0;

	pr_info("%s() %s++\n", __func__, (flag) ? ("on") : ("off"));

	if (flag) {
		/* 1V8 on */
		ret = camera_set_vprog_power(CAMERA_VPROG2, 1,
				DEFAULT_VOLTAGE);
		if (ret) {
			pr_err("%s power %s vprog2 failed\n", __func__,
			       flag ? "on" : "off");
			return ret;
		}

		usleep_range(1000, 1200);
		msleep(15);

		/* 2V8 on */
		ret = camera_set_vprog_power(CAMERA_VPROG1, 1,
				DEFAULT_VOLTAGE);
		if (ret) {
			pr_err("%s power %s vprog1 failed\n", __func__,
					flag ? "on" : "off");

			camera_set_vprog_power(CAMERA_VPROG2, 0,
					DEFAULT_VOLTAGE);
		}
		usleep_range(1000, 1200);
	} else {
		/* 2V8 off */
		ret = camera_set_vprog_power(CAMERA_VPROG1, 0,
				DEFAULT_VOLTAGE);
		if (ret) {
			pr_err("%s power %s vprog1 failed\n", __func__,
					flag ? "on" : "off");
			return ret;
		}
		msleep(15);

		/* 1V8 off */
		ret = camera_set_vprog_power(CAMERA_VPROG2, 0,
				DEFAULT_VOLTAGE);
		if (ret)
			pr_err("%s power failed\n", __func__);
	}

	pr_info("%s() %s--\n", __func__, (flag) ? ("on") : ("off"));
	return ret;
}

static int mt9m114_csi_configure(struct v4l2_subdev *sd, int flag)
{
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_SECONDARY, 1,
		ATOMISP_INPUT_FORMAT_RAW_10, atomisp_bayer_order_grbg, flag);
}

static int mt9m114_platform_init(struct i2c_client *client)
{
	pr_info("%s()\n", __func__);

	return 0;
}

static int mt9m114_platform_deinit(void)
{
	pr_info("%s()\n", __func__);

	return 0;
}

static struct camera_sensor_platform_data mt9m114_sensor_platform_data = {
	.gpio_ctrl	= mt9m114_gpio_ctrl,
	.flisclk_ctrl	= mt9m114_flisclk_ctrl,
	.power_ctrl	= mt9m114_power_ctrl,
	.csi_cfg	= mt9m114_csi_configure,
	.platform_init = mt9m114_platform_init,
	.platform_deinit = mt9m114_platform_deinit,
};

void *mt9m114_platform_data(void *info)
{

	pr_info("%s()\n", __func__);

	camera_reset = -1;

	return &mt9m114_sensor_platform_data;
}

