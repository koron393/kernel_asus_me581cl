/*
 * platform_cap1106.c: Platform data for Cap Sensor Driver
 *
 * (C) Copyright 2013 Intel Corporation
 *  Author: Joseph Wu <joseph_wu@asus.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <asm/intel-mid.h>
#include "platform_cap1106.h"
#include <linux/i2c/cap1106.h>

static struct cap1106_i2c_platform_data cap1106_pdata = {
    .app2mdm_enable = 0,
    .det_gpio_name = CAP1106_DET_GPIO_NAME,
    .init_table = {
       0x1F, 0x1F, // Data sensitivity
       0x20, 0x20, // MAX duration disable
       0x21, 0x22, // Enable CS2+CS6.
       0x22, 0xFF, // MAX duration time to max , repeat period time to max
       0x24, 0x39, // Digital count update time to 140*64ms
       0x27, 0x22, // Enable INT. for CS2+CS6.
       0x28, 0x22, // Disable repeat irq
       0x2A, 0x00, // All channel run in the same time
       0x31, 0x0A, // Threshold of CS 2
       0x35, 0x0A, // Threshold of CS 6
       0x26, 0x22, // Force re-cal CS2+CS6
       0x44, 0x44, // Disable RF Noise filter
       0x00, 0xC0, // Reset INT. bit.
    },
};

void *cap1106_platform_data(void *info)
{
       struct i2c_board_info *i2c_info = info;

       cap1106_pdata.det_gpio = get_gpio_by_name("sar_det");  // EVB
       printk ("cap1106: get gpio #%d.\n", cap1106_pdata.det_gpio);

       i2c_info->irq = cap1106_pdata.det_gpio;

       return &cap1106_pdata;
}

