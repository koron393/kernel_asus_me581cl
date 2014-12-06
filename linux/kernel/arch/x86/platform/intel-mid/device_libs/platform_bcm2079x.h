/*
 * platform_BCM2079.h: BCM2079 platform data header file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_BCM2079_H_
#define _PLATFORM_BCM2079_H_

/* MFLD NFC controller (BCM2079) platform init */
#define BCM_HOST_INT_GPIO		"NFC-intr"
#define BCM_ENABLE_GPIO			"NFC-enable"
#define BCM_FW_RESET_GPIO		"NFC-reset"
extern void *bcm2079x_platform_data(void *info) __attribute__((weak));
#endif
