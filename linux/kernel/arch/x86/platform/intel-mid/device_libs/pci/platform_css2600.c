/*
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
 *
 * Author: Sakari Ailus <sakari.ailus@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/sfi.h>

#include <asm/intel_scu_ipcutil.h>
#include <asm/intel-mid.h>

#include <media/css2600-isys.h>
#include <media/smiapp.h>

#if IS_ENABLED(CONFIG_VIDEO_CSS2600)

#define IMX135_LANES 4

static int set_xclk(struct v4l2_subdev *sd, int hz)
{
#ifdef CONFIG_INTEL_SCU_IPC_UTIL
	return intel_scu_ipc_osc_clk(OSC_CLK_CAM0, hz / 1000);
#else
	pr_err("imx135 clock is not set.\n");
	return -ENODEV;
#endif
}

static int set_power(struct v4l2_subdev *sd, int poweron)
{
#ifdef CONFIG_INTEL_SCU_IPC_UTIL
	int ret;

	/*
	 * In a sane world all this would be controlled by the regulator
	 * framework
	 */

	if (poweron) {
		ret = gpio_request(2, "imx135 af power");
		if (ret < 0) {
			pr_err("gpio ouch\n");
			return ret;
		}
	}

	ret = gpio_direction_output(2, poweron);
	if (ret) {
		pr_err("can't set af power gpio as %d\n", poweron);
		return ret;
	}

	if (!poweron)
		gpio_free(2);

	ret = intel_scu_ipc_msic_vprog1(poweron);
	if (ret) {
		pr_err("Imx135 power transition for vprog1 failed\n");
		return ret;
	}

	ret = intel_scu_ipc_msic_vprog3(poweron);
	if (ret) {
		pr_err("Imx135 power transition for vprog3 failed\n");
		return ret;
	}

	return ret;
#else
	return -ENODEV;
#endif
}

static struct smiapp_platform_data imx135_pdata = {
	.xshutdown = 9,
	.lanes = IMX135_LANES,
	.ext_clk = 19200000,
	.op_sys_clock = (uint64_t []){ 1996800000 / IMX135_LANES,
				       1804800000 / IMX135_LANES,
				       1368000000 / IMX135_LANES,
				       838400000 / IMX135_LANES,
				       0 },
	.set_power = set_power,
	.set_xclk = set_xclk,
};

static struct css2600_isys_csi2_config imx135_csi2_cfg = {
	.nlanes = 4,
	.port = 0,
};

static struct css2600_isys_subdev_info imx135_sd = {
	.csi2 = &imx135_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(SMIAPP_NAME, SMIAPP_DFL_I2C_ADDR),
			 .platform_data = &imx135_pdata,
		},
		.i2c_adapter_id = 4,
	}
};

static struct css2600_isys_subdev_pdata pdata = {
	.subdevs = (struct css2600_isys_subdev_info *[]) {
		&imx135_sd,
		NULL,
	},
};

#define MATCH2(phone, platform, product) \
	(INTEL_MID_BOARD(2, phone, platform, product, PRO)	\
	 || INTEL_MID_BOARD(2, phone, platform, product, ENG))

static void css2600_quirk(struct pci_dev *pci_dev)
{
	if (MATCH2(PHONE, MOFD, V0))
		pci_dev->dev.platform_data = &pdata;
}

/* Moorefield */
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x1478, css2600_quirk);

#endif /* IS_ENABLED(CONFIG_VIDEO_CSS2600) */
