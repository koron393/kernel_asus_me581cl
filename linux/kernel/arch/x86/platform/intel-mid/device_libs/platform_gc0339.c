/*
 * platform_gc0339.c: gc0339 platform data initilization file
 *
 * (C) Copyright 2014 Asus Corporation
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
#include "platform_gc0339.h"
#include <linux/acpi_gpio.h>
#include <linux/lnw_gpio.h>

#define CAMERA_1_RESET 10

static int camera_reset;

/*
 * MFLD PR2 secondary camera sensor - gc0339 platform data
 */
static int gc0339_gpio_ctrl(struct v4l2_subdev *sd, int flag)
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

static int gc0339_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	static const unsigned int clock_khz = 19200;
	int ret = 0;

	pr_info("%s(), flag: %d\n", __func__, flag);

	if (intel_mid_identify_cpu() != INTEL_MID_CPU_CHIP_VALLEYVIEW2)
		return intel_scu_ipc_osc_clk(OSC_CLK_CAM1,
					     flag ? clock_khz : 0);
	return ret;
}

static int gc0339_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret = 0;

	/* TODO:
	 *   Control 2V8 enable pin for FE375CG/FE375CXG.
	 *   Should we control GPIO-11 for ME572C?
	 */

	pr_info("%s() %s++\n", __func__, (flag) ? ("on") : ("off"));

	if (flag) {
		/* 1V8 on */
		pr_info("%s(%d): 1V8 on\n", __func__, __LINE__);
		ret = intel_scu_ipc_msic_vprog2(1);
		if (ret) {
			pr_err("%s power vprog2 failed\n", __func__);
			return ret;
		}

		usleep_range(10, 12);

		/* 2V8 on */
		pr_info("%s(%d): 2V8 on\n", __func__, __LINE__);
		ret = intel_scu_ipc_msic_vprog1(1);
		if (ret) {
			pr_err("%s power failed\n", __func__);
			return ret;
		}

		usleep_range(1000, 5000);

		/* Enable MCLK: 19.2MHz */
		pr_info("%s(%d): mclk on\n", __func__, __LINE__);
		ret = gc0339_flisclk_ctrl(sd, 1);
		if (ret) {
			pr_err("%s flisclk_ctrl on failed\n", __func__);
			return ret;
		}

		usleep_range(5000, 6000);

		/* Reset */
		pr_info("%s(%d): reset on\n", __func__, __LINE__);
		ret = gc0339_gpio_ctrl(sd, 1);
		if (ret) {
			pr_err("%s gpio_ctrl on failed\n", __func__);
			return ret;
		}
		usleep_range(5, 10); /* Delay for I2C cmds: 100 mclk cycles */
	} else {
		usleep_range(5, 10); /* Delay for I2C cmds: 100 mclk cycles */

		/* Reset */
		pr_info("%s(%d): reset off\n", __func__, __LINE__);
		ret = gc0339_gpio_ctrl(sd, 0);
		if (ret) {
			pr_err("%s gpio_ctrl off failed\n", __func__);
			return ret;
		}

		usleep_range(5000, 6000);

		/* Disable MCLK */
		pr_info("%s(%d): mclk off\n", __func__, __LINE__);
		ret = gc0339_flisclk_ctrl(sd, 0);
		if (ret) {
			pr_err("%s flisclk_ctrl off failed\n", __func__);
			return ret;
		}

		usleep_range(1000, 5000);

		/* 2V8 off */
		pr_info("%s(%d): 2V8 off\n", __func__, __LINE__);
		ret = intel_scu_ipc_msic_vprog1(0);
		if (ret) {
			pr_err("%s power vprog1 failed\n", __func__);
			return ret;
		}

		usleep_range(10, 12);

		/* 1V8 off */
		pr_info("%s(%d): 1V8 off\n", __func__, __LINE__);
		ret = intel_scu_ipc_msic_vprog2(0);
		if (ret)
			pr_err("%s power failed\n", __func__);
	}

	pr_info("%s() %s--\n", __func__, (flag) ? ("on") : ("off"));
	return ret;
}

static int gc0339_csi_configure(struct v4l2_subdev *sd, int flag)
{
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_SECONDARY, 1,
		ATOMISP_INPUT_FORMAT_RAW_10, atomisp_bayer_order_rggb, flag);
}

static int gc0339_platform_init(struct i2c_client *client)
{
	pr_info("%s()\n", __func__);

	return 0;
}

static int gc0339_platform_deinit(void)
{
	pr_info("%s()\n", __func__);

	return 0;
}

static struct camera_sensor_platform_data gc0339_sensor_platform_data = {
	.gpio_ctrl	= gc0339_gpio_ctrl,
	.flisclk_ctrl	= gc0339_flisclk_ctrl,
	.power_ctrl	= gc0339_power_ctrl,
	.csi_cfg	= gc0339_csi_configure,
	.platform_init = gc0339_platform_init,
	.platform_deinit = gc0339_platform_deinit,
};

void *gc0339_platform_data(void *info)
{

	pr_info("%s()\n", __func__);

	camera_reset = -1;

	return &gc0339_sensor_platform_data;
}

