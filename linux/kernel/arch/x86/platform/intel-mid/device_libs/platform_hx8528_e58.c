/*
 * platform_r69001.c: r69001 touch platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <linux/lnw_gpio.h>
#include <asm/intel-mid.h>
#include <linux/hx8528_e58.h>
#include "platform_hx8528_e58.h"

void *hx8528_e58_platform_data(void *info)
{
	static struct himax_i2c_platform_data hx_pdata;

	hx_pdata.rst_gpio = get_gpio_by_name("TS_RST"); //EVB: ts_rst
	hx_pdata.intr_gpio = get_gpio_by_name("TS_INT"); //EVB: ts_int

	return &hx_pdata;
}
