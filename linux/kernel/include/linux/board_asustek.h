/* include/linux/board_asustek.h
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2012, LGE Inc.
 * Copyright (c) 2012-2014, ASUSTek Computer Inc.
 * Author: Paris Yeh <paris_yeh@asus.com>
 *	   Hank Lee  <hank_lee@asus.com>
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

#ifndef __ASM_INTEL_MID_BOARD_ASUSTEK_H
#define __ASM_INTEL_MID_BOARD_ASUSTEK_H
#include <linux/sfi.h>

#ifndef CONFIG_SFI_PCB
#define CONFIG_SFI_PCB
#endif
typedef enum {
	HW_REV_INVALID = -1,
	HW_REV_MAX = 8
} hw_rev;

typedef enum {
	PROJECT_ID_INVALID = -1,
	PROJECT_ID_MAX = 8
} project_id;

typedef enum {
	LCD_TYPE_INVALID = -1,
	LCD_TYPE_MAX = 2
} lcd_type;

typedef enum {
	TP_TYPE_INVALID = -1,
	TP_TYPE_MAX = 4,
	TP_TYPE_1_MAX = 8
} tp_type;

typedef enum {
	CAM_VENDOR_INVALID = -1,
	CAM_VENDOR_2_MAX = 2,
	CAM_VENDOR_MAX = 4
} cam_vendor;

typedef enum {
	CAM_LENS_INVALID = -1,
	CAM_LENS_MAX = 2
} cam_lens;

typedef enum {
	CAM_FRONT_INVALID = -1,
	CAM_FRONT_MAX = 4
} cam_front;

typedef enum {
	CAM_REAR_INVALID = -1,
	CAM_REAR_MAX = 2
} cam_rear;

typedef enum {
	RF_SKU_INVALID = -1,
	RF_SKU_MAX = 8
} rf_sku;

typedef enum {
	SIM_ID_INVALID = -1,
	SIM_ID_MAX = 2
} sim_id;

struct asustek_pcbid_platform_data {
	const char *UUID;
	struct resource *resource0;
	u32 nr_resource0;
	struct resource *resource1;
	u32 nr_resource1;
	struct resource *resource2;
	u32 nr_resource2;
};

hw_rev asustek_get_hw_rev(void);

project_id asustek_get_project_id(void);

lcd_type asustek_get_lcd_type(void);

tp_type asustek_get_tp_type(void);

cam_vendor asustek_get_camera_vendor(void);

cam_lens asustek_get_camera_lens(void);

cam_front asustek_get_camera_front(void);

cam_rear asustek_get_camera_rear(void);

rf_sku asustek_get_rf_sku(void);

sim_id asustek_get_sim_id(void);

int sfi_parse_oem1(struct sfi_table_header *);
void sfi_parsing_done(bool);
#endif /* __ASM_INTEL_MID_BOARD_ASUSTEK_H */
