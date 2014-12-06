/*
 * Support for gc0339 Camera Sensor.
 *
 * Copyright (c) 2014 ASUSTeK COMPUTER INC. All Rights Reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/spinlock.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <linux/v4l2-mediabus.h>
#include <media/media-entity.h>
#include <linux/atomisp_platform.h>
#include <linux/atomisp.h>

#define V4L2_IDENT_GC0339 8245

#define GC0339_AWB_STEADY	(1<<0)	/* awb steady */
#define GC0339_AE_READY	(1<<3)	/* ae status ready */

/* mask to set sensor vert_flip and horz_mirror */
#define GC0339_VFLIP_MASK	0x0002
#define GC0339_HFLIP_MASK	0x0001
#define GC0339_FLIP_EN		1
#define GC0339_FLIP_DIS		0

/* sensor register that control sensor read-mode and mirror */
#define GC0339_READ_MODE	0xC834
/* sensor ae-track status register */
#define GC0339_AE_TRACK_STATUS	0xA800
/* sensor awb status register */
#define GC0339_AWB_STATUS	0xAC00
/* sensor coarse integration time register */
#define GC0339_COARSE_INTEGRATION_TIME_H 0x03
#define GC0339_COARSE_INTEGRATION_TIME_L 0x04

//registers
#define GC0339_PID		0x00
#define REG_V_START		0x06
#define REG_H_START		0x08
#define REG_HEIGHT_H		0x09
#define REG_HEIGHT_L		0x0A
#define REG_WIDTH_H		0x0B
#define REG_WIDTH_L		0x0C
#define REG_SH_DELAY		0x12
#define REG_DUMMY_H		0x0F
#define REG_H_DUMMY_L		0x01
#define REG_V_DUMMY_L		0x02

#define REG_EXPO_COARSE		0x03

#define REG_GAIN		0x50

#define I2C_RETRY_COUNT		5
#define MSG_LEN_OFFSET		1 /*8-bits addr*/



/* GC0339_CHIP_ID VALUE*/
#define GC0339_MOD_ID		0xc8

#define GC0339_FINE_INTG_TIME_MIN 0
#define GC0339_FINE_INTG_TIME_MAX_MARGIN 0
#define GC0339_COARSE_INTG_TIME_MIN 1
#define GC0339_COARSE_INTG_TIME_MAX_MARGIN 6


#define GC0339_BPAT_RGRGGBGB	(1 << 0)
#define GC0339_BPAT_GRGRBGBG	(1 << 1)
#define GC0339_BPAT_GBGBRGRG	(1 << 2)
#define GC0339_BPAT_BGBGGRGR	(1 << 3)

#define GC0339_FOCAL_LENGTH_NUM	208	/*2.08mm*/
#define GC0339_FOCAL_LENGTH_DEM	100
#define GC0339_F_NUMBER_DEFAULT_NUM	24
#define GC0339_F_NUMBER_DEM	10
#define GC0339_FLICKER_MODE_50HZ	1
#define GC0339_FLICKER_MODE_60HZ	2
/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define GC0339_FOCAL_LENGTH_DEFAULT 0xD00064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define GC0339_F_NUMBER_DEFAULT 0x18000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define GC0339_F_NUMBER_RANGE 0x180a180a

/* Supported resolutions */
enum {
	GC0339_RES_CIF,
	GC0339_RES_VGA,
};

#define GC0339_RES_VGA_SIZE_H		640
#define GC0339_RES_VGA_SIZE_V		480
#define GC0339_RES_CIF_SIZE_H		352
#define GC0339_RES_CIF_SIZE_V		288
#define GC0339_RES_QVGA_SIZE_H		320
#define GC0339_RES_QVGA_SIZE_V		240
#define GC0339_RES_QCIF_SIZE_H		176
#define GC0339_RES_QCIF_SIZE_V		144


/* #defines for register writes and register array processing */
#define SENSOR_WAIT_MS		0
#define GC0339_8BIT		1
#define GC0339_16BIT		2
#define GC0339_32BIT		4
#define SENSOR_TABLE_END	0xFF

struct gc0339_reg_cmd {
	u16 cmd;
	u16 addr;
	u16 val;
};


struct gc0339_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;

	struct camera_sensor_platform_data *platform_data;
	int real_model_id;
	int nctx;
	int power;

	unsigned int bus_width;
	unsigned int mode;
	unsigned int field_inv;
	unsigned int field_sel;
	unsigned int ycseq;
	unsigned int conv422;
	unsigned int bpat;
	unsigned int hpol;
	unsigned int vpol;
	unsigned int edge;
	unsigned int bls;
	unsigned int gamma;
	unsigned int cconv;
	unsigned int res;
	unsigned int dwn_sz;
	unsigned int blc;
	unsigned int agc;
	unsigned int awb;
	unsigned int aec;
	/* extention SENSOR version 2 */
	unsigned int cie_profile;

	/* extention SENSOR version 3 */
	unsigned int flicker_freq;

	/* extension SENSOR version 4 */
	unsigned int smia_mode;
	unsigned int mipi_mode;

	/* Add name here to load shared library */
	unsigned int type;

	/*Number of MIPI lanes*/
	unsigned int mipi_lanes;
	char name[32];

	u8 lightfreq;

       struct attribute_group sensor_i2c_attribute; //Add for ATD read camera status+++
};

struct gc0339_res_struct {
	u8 *desc;
	int res;
	int width;
	int height;
	int fps;
	int skip_frames;
	bool used;
	u16 pixels_per_line;
	u16 lines_per_frame;
	u8 bin_factor_x;
	u8 bin_factor_y;
	u8 bin_mode;
};

struct gc0339_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, int value);
};


/*
 * Modes supported by the gc0339 driver.
 * Please, keep them in ascending order.
 */
static struct gc0339_res_struct gc0339_res[] = {
	{
	.desc	= "CIF",
	.res	= GC0339_RES_CIF,
	.width	= 368,
	.height	= 304,
	.fps	= 30,
	.used	= 0,
	.skip_frames = 1,

	.pixels_per_line = 0x01F0, // consistent with regs arrays
	.lines_per_frame = 0x014F, // consistent with regs arrays
	.bin_factor_x = 1,
	.bin_factor_y = 1,
	.bin_mode = 0,
	},
	{
	.desc	= "VGA",
	.res	= GC0339_RES_VGA,
	.width	= 656,
	.height	= 492,
	.fps	= 30,
	.used	= 0,
	.skip_frames = 2,

	.pixels_per_line = 0x0314, // consistent with regs arrays
	.lines_per_frame = 0x0213, // consistent with regs arrays
	.bin_factor_x = 1,
	.bin_factor_y = 1,
	.bin_mode = 0,
	},
};
#define N_RES (ARRAY_SIZE(gc0339_res))

static const struct i2c_device_id gc0339_id[] = {
	{"gc0339", 0},
	{}
};

static struct gc0339_reg_cmd const gc0339_suspend[] = {
	 {SENSOR_TABLE_END, 0, 0}
};

static struct gc0339_reg_cmd const gc0339_streaming[] = {
	 {SENSOR_TABLE_END, 0, 0}
};

static struct gc0339_reg_cmd const gc0339_standby_reg[] = {
	 {SENSOR_TABLE_END, 0, 0}
};

static struct gc0339_reg_cmd const gc0339_wakeup_reg[] = {
	 {SENSOR_TABLE_END, 0, 0}
};

static struct gc0339_reg_cmd const gc0339_chgstat_reg[] = {
	{SENSOR_TABLE_END, 0, 0}
};
static struct gc0339_reg_cmd const gc0339_cif_init[] = {
{GC0339_8BIT, 0x15, 0x8A}, //CIF
{GC0339_8BIT, 0x62, 0xcc}, //LWC by verrill
{GC0339_8BIT, 0x63, 0x01},

{GC0339_8BIT, 0x06, 0x00},
{GC0339_8BIT, 0x08, 0x00},
{GC0339_8BIT, 0x09, 0x01}, // by Wesley
{GC0339_8BIT, 0x0A, 0x32}, // by Wesley
{GC0339_8BIT, 0x0B, 0x01}, // by Wesley
{GC0339_8BIT, 0x0C, 0x74}, // by Wesley

{SENSOR_TABLE_END, 0, 0}
};

static struct gc0339_reg_cmd const gc0339_qvga_init[] = {

	{SENSOR_TABLE_END, 0, 0}
};

static struct gc0339_reg_cmd const gc0339_vga_init[] = {
{GC0339_8BIT, 0x15, 0x0A},
{GC0339_8BIT, 0x62, 0x34}, //by verrill
{GC0339_8BIT, 0x63, 0x03}, //by verrill

{GC0339_8BIT, 0x06, 0x00},
{GC0339_8BIT, 0x08, 0x00},
{GC0339_8BIT, 0x09, 0x01}, //by Wesley
{GC0339_8BIT, 0x0A, 0xEE}, //by Wesley EE
{GC0339_8BIT, 0x0B, 0x02}, //by Wesley
{GC0339_8BIT, 0x0C, 0x94}, //by Wesley

{SENSOR_TABLE_END, 0x0, 0x0}
};


static struct gc0339_reg_cmd const gc0339_common[] = {
{GC0339_8BIT, 0xFE, 0x80},
{GC0339_8BIT, 0xFC, 0x10},
{GC0339_8BIT, 0xFE, 0x00},
{GC0339_8BIT, 0xF6, 0x05},
{GC0339_8BIT, 0xF7, 0x01},
{GC0339_8BIT, 0xF7, 0x03},
{GC0339_8BIT, 0xFC, 0x16},

{GC0339_8BIT, 0x06, 0x00},
{GC0339_8BIT, 0x08, 0x00},
{GC0339_8BIT, 0x09, 0x01}, //by Wesley
{GC0339_8BIT, 0x0A, 0xEE}, //by Wesley
{GC0339_8BIT, 0x0B, 0x02}, //by Wesley
{GC0339_8BIT, 0x0C, 0x94}, //by Wesley

{GC0339_8BIT, 0x01, 0x90}, //DummyHor 144
{GC0339_8BIT, 0x02, 0x31}, //Wesley min:0x30

{GC0339_8BIT, 0x0F, 0x00}, //DummyHor 144
{GC0339_8BIT, 0x14, 0x10}, //Wesley, Change CFA Sequence to RGGB
{GC0339_8BIT, 0x1A, 0x21},
{GC0339_8BIT, 0x1B, 0x08},
{GC0339_8BIT, 0x1C, 0x19},
{GC0339_8BIT, 0x1D, 0xEA},
{GC0339_8BIT, 0x20, 0xB0},
{GC0339_8BIT, 0x2E, 0x00},

{GC0339_8BIT, 0x30, 0xB7},
{GC0339_8BIT, 0x31, 0x7F},
{GC0339_8BIT, 0x32, 0x00},
{GC0339_8BIT, 0x39, 0x04},
{GC0339_8BIT, 0x3A, 0x20},
{GC0339_8BIT, 0x3B, 0x20},
{GC0339_8BIT, 0x3C, 0x00},
{GC0339_8BIT, 0x3D, 0x00},
{GC0339_8BIT, 0x3E, 0x00},
{GC0339_8BIT, 0x3F, 0x00},

{GC0339_8BIT, 0x62, 0x34},  //by Wesley
{GC0339_8BIT, 0x63, 0x03}, // by Wesley
{GC0339_8BIT, 0x69, 0x03},
{GC0339_8BIT, 0x60, 0x80},
{GC0339_8BIT, 0x65, 0x20}, //20 -> 21
{GC0339_8BIT, 0x6C, 0x40},
{GC0339_8BIT, 0x6D, 0x01},
{GC0339_8BIT, 0x6A, 0x34}, //BOTH 0x34

{GC0339_8BIT, 0x4A, 0x50},
{GC0339_8BIT, 0x4B, 0x40},
{GC0339_8BIT, 0x4C, 0x40},
{GC0339_8BIT, 0xE8, 0x04},
{GC0339_8BIT, 0xE9, 0xBB},

{GC0339_8BIT, 0x42, 0x20},
{GC0339_8BIT, 0x47, 0x10},

{GC0339_8BIT, 0x50, 0x80},

{GC0339_8BIT, 0xD0, 0x00},
{GC0339_8BIT, 0xD2, 0x00}, //disable AE
{GC0339_8BIT, 0xD3, 0x50},

{GC0339_8BIT, 0x71, 0x01},
{GC0339_8BIT, 0x72, 0x01},
{GC0339_8BIT, 0x73, 0x05}, //Nor:0x05 DOU:0x06 //clk zero 0x05 //data zero 0x7a=0x0a
{GC0339_8BIT, 0x74, 0x01},
{GC0339_8BIT, 0x76, 0x02}, //Nor:0x02 DOU:0x03
{GC0339_8BIT, 0x79, 0x01},
{GC0339_8BIT, 0x7A, 0x05}, //data zero 0x7a default=0x0a
{GC0339_8BIT, 0x7B, 0x02}, //Nor:0x02 DOU:0x03

{SENSOR_TABLE_END, 0x0, 0x0}
};

static struct gc0339_reg_cmd const gc0339_antiflicker_50hz[] = {
	 {SENSOR_TABLE_END, 0, 0}
};

static struct gc0339_reg_cmd const gc0339_antiflicker_60hz[] = {
	 {SENSOR_TABLE_END, 0, 0}
};

static struct gc0339_reg_cmd const gc0339_iq[] = {
	 {SENSOR_TABLE_END, 0, 0}
};

