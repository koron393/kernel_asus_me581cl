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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>

#include "gc0339.h"

#define to_gc0339_sensor(sd) container_of(sd, struct gc0339_device, sd)

/*
 * TODO: use debug parameter to actually define when debug messages should
 * be printed.
 */
static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

//Add for ATD read camera status+++
int ATD_gc0339_status = 0;  //Add for ATD read camera status

static ssize_t gc0339_show_status(struct device *dev,struct device_attribute *attr,char *buf)
{
	pr_info("%s: get gc0339 status (%d) !!\n", __func__, ATD_gc0339_status);
	//Check sensor connect status, just do it  in begining for ATD camera status

	return sprintf(buf,"%d\n", ATD_gc0339_status);
}

static DEVICE_ATTR(gc0339_status, S_IRUGO,gc0339_show_status,NULL);

static struct attribute *gc0339_attributes[] = {
	&dev_attr_gc0339_status.attr,
	NULL
};
//Add for ATD read camera status---

static int gc0339_t_vflip(struct v4l2_subdev *sd, int value);
static int gc0339_t_hflip(struct v4l2_subdev *sd, int value);

static int
gc0339_read_reg(struct i2c_client *client, u16 data_length, u8 reg, u8 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[2];

	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	if (data_length != GC0339_8BIT && data_length != GC0339_16BIT
					 && data_length != GC0339_32BIT) {
		v4l2_err(client, "%s error, invalid data length\n", __func__);
		return -EINVAL;
	}

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = MSG_LEN_OFFSET;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8) (reg & 0xff);

	msg[1].addr = client->addr;
	msg[1].len = data_length;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data+1;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err >= 0) {
		*val = 0;
		/* high byte comes first */
		if (data_length == GC0339_8BIT)
			*val = data[1];
		/*else if (data_length == GC0339_16BIT)
			*val = data[1] + (data[0] << 8);
		else
			*val = data[3] + (data[2] << 8) +
				(data[1] << 16) + (data[0] << 24);*/

		return 0;
	}

	dev_err(&client->dev, "read from offset 0x%x error %d", reg, err);
	return err;
}

static int
gc0339_write_reg(struct i2c_client *client, u16 data_length, u8 reg, u8 val)
{
	int num_msg;
	struct i2c_msg msg;
	unsigned char data[4] = {0};
	u16 *wreg;
	int retry = 0;

	/* TODO: Should we deal with 16bit or 32bit data ? */

	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	if (data_length != GC0339_8BIT && data_length != GC0339_16BIT
					 && data_length != GC0339_32BIT) {
		v4l2_err(client, "%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}

	memset(&msg, 0, sizeof(msg));

again:

	data[0] = (u8) reg;
	data[1] = (u8) (val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 1 + data_length;
	msg.buf = data;

	/* high byte goes out first
	wreg = (u16 *)data;
	*wreg = cpu_to_be16(reg);*/

	if (data_length == GC0339_8BIT) {
		data[2] = (u8)(val);
	} /*else if (data_length == GC0339_16BIT) {
		u16 *wdata = (u16 *)&data[2];
		*wdata = be16_to_cpu((u16)val);
	} else {
		 GC0339_32BIT
		u32 *wdata = (u32 *)&data[2];
		*wdata = be32_to_cpu(val);
	}*/

	num_msg = i2c_transfer(client->adapter, &msg, 1);

	/*
	 * HACK: Need some delay here for Rev 2 sensors otherwise some
	 * registers do not seem to load correctly.
	 */
	mdelay(1);

	if (num_msg >= 0)
		return 0;

	dev_err(&client->dev, "write error: wrote 0x%x to offset 0x%x error %d",
		val, reg, num_msg);
	if (retry < I2C_RETRY_COUNT) {
		dev_dbg(&client->dev, "retrying... %d", retry);
		retry++;
		msleep(20);
		goto again;
	}

	return num_msg;
}
/**
 * gc0339_rmw_reg - Read/Modify/Write a value to a register in the sensor
 * device
 * @client: i2c driver client structure
 * @data_length: 8/16/32-bits length
 * @reg: register address
 * @mask: masked out bits
 * @set: bits set
 *
 * Read/modify/write a value to a register in the  sensor device.
 * Returns zero if successful, or non-zero otherwise.
 */
static int
gc0339_rmw_reg(struct i2c_client *client, u16 data_length, u16 reg,
			 u32 mask, u32 set)
{
	int err;
	u32 val;

	/* Exit when no mask */
	if (mask == 0)
		return 0;

	/* @mask must not exceed data length */
	switch (data_length) {
	case GC0339_8BIT:
		if (mask & ~0xff)
			return -EINVAL;
		break;
	case GC0339_16BIT:
		if (mask & ~0xffff)
			return -EINVAL;
		break;
	case GC0339_32BIT:
		break;
	default:
		/* Wrong @data_length */
		return -EINVAL;
	}

	err = gc0339_read_reg(client, data_length, reg, &val);
	if (err) {
		v4l2_err(client, "gc0339_rmw_reg error exit, read failed\n");
		return -EINVAL;
	}

	val &= ~mask;

	/*
	 * Perform the OR function if the @set exists.
	 * Shift @set value to target bit location. @set should set only
	 * bits included in @mask.
	 *
	 * REVISIT: This function expects @set to be non-shifted. Its shift
	 * value is then defined to be equal to mask's LSB position.
	 * How about to inform values in their right offset position and avoid
	 * this unneeded shift operation?
	 */
	set <<= ffs(mask) - 1;
	val |= set & mask;

	err = gc0339_write_reg(client, data_length, reg, val);
	if (err) {
		v4l2_err(client, "gc0339_rmw_reg error exit, write failed\n");
		return -EINVAL;
	}

	return 0;
}


/*
 * gc0339_write_reg_array - Initializes a list of gc0339 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 */

static int gc0339_write_reg_array(struct i2c_client *client,
				const struct gc0339_reg_cmd reglist[])
{
	int err;
	const struct gc0339_reg_cmd *next;
	u16 val;

	for (next = reglist; next->cmd != SENSOR_TABLE_END; next++) {

		if (next->cmd == SENSOR_WAIT_MS) {
			msleep(next->val);
			continue;
		}

		val = next->val;

		if (next->cmd == GC0339_8BIT)
			err = gc0339_write_reg(client, GC0339_8BIT, next->addr, val);
		else
			pr_info("%s: Unknown cmd: %d\n", __func__, next->cmd);

		if (err) {
			pr_info("%s: err=%d\n", __func__, err);
			return err;
		}
	}
	return 0;
}

static int gc0339_set_suspend(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	pr_info("%s++\n",__func__);
	gc0339_write_reg(client, GC0339_8BIT,  0x60, 0x80); //10bit raw disable
	return 0;
}

static int gc0339_set_streaming(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	pr_info("%s++\n",__func__);

	gc0339_write_reg(client, GC0339_8BIT,  0xFE, 0x50); //Wesley_Kao, enable per frame MIPI reset
	gc0339_write_reg(client, GC0339_8BIT,  0x60, 0x98); //10bit raw enable

		//workaround test code by Q platform +++
	/*gc0339_write_reg(client, GC0339_8BIT,  0xFE, 0x50);
	gc0339_write_reg(client, GC0339_8BIT,  0xFE, 0x50);
	gc0339_write_reg(client, GC0339_8BIT,  0x61, 0x2B);
	gc0339_write_reg(client, GC0339_8BIT,  0x62, 0x20);
	gc0339_write_reg(client, GC0339_8BIT,  0x63, 0x03);
	gc0339_write_reg(client, GC0339_8BIT,  0x69, 0x03);
	gc0339_write_reg(client, GC0339_8BIT,  0x60, 0x88);
	gc0339_write_reg(client, GC0339_8BIT,  0x65, 0x20);
	gc0339_write_reg(client, GC0339_8BIT,  0x6C, 0x40);
	gc0339_write_reg(client, GC0339_8BIT,  0x6D, 0x01);
	gc0339_write_reg(client, GC0339_8BIT,  0x67, 0x10);
	gc0339_write_reg(client, GC0339_8BIT,  0x6A, 0x34);
	gc0339_write_reg(client, GC0339_8BIT,  0x71, 0x01);
	gc0339_write_reg(client, GC0339_8BIT,  0x72, 0x01);
	gc0339_write_reg(client, GC0339_8BIT,  0x73, 0x05);
	gc0339_write_reg(client, GC0339_8BIT,  0x79, 0x01);
	gc0339_write_reg(client, GC0339_8BIT,  0x7A, 0x05);
	gc0339_write_reg(client, GC0339_8BIT,  0x79, 0x01);
	gc0339_write_reg(client, GC0339_8BIT,  0x60, 0x98);*/
		//workaround test code by Q platform ---
	return 0;
}

static int gc0339_init_common(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	pr_info("%s()++\n",__func__);

	ret = gc0339_write_reg_array(client, gc0339_common);

	if (ret)
		pr_info("%s: write seq fail\n", __func__);

	pr_info("%s()--\n",__func__);
	return ret;
}

static int power_up(struct v4l2_subdev *sd)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (NULL == dev->platform_data) {
		dev_err(&client->dev, "no camera_sensor_platform_data");
		return -ENODEV;
	}

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 1);
	if (ret)
		goto fail_power;

	return 0;

fail_power:
	dev->platform_data->power_ctrl(sd, 0);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (NULL == dev->platform_data) {
		dev_err(&client->dev, "no camera_sensor_platform_data");
		return -ENODEV;
	}

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "vprog failed.\n");

	/*according to DS, 20ms is needed after power down*/
	msleep(20);

	return ret;
}

static int gc0339_s_power(struct v4l2_subdev *sd, int power)
{
	pr_info("%s(%d) %s\n", __func__, __LINE__, power ? ("on") : ("off"));
	if (power == 0)
		return power_down(sd);
	else {
		if (power_up(sd))
			return -EINVAL;

		return gc0339_init_common(sd);
	}
}

static int gc0339_try_res(u32 *w, u32 *h)
{
	pr_info("%s++\n",__func__);
	int i;

	/*
	 * The mode list is in ascending order. We're done as soon as
	 * we have found the first equal or bigger size.
	 */
	for (i = 0; i < N_RES; i++) {
		if ((gc0339_res[i].width >= *w) &&
			(gc0339_res[i].height >= *h))
			break;
	}

	/*
	 * If no mode was found, it means we can provide only a smaller size.
	 * Returning the biggest one available in this case.
	 */
	if (i == N_RES)
		i--;

	*w = gc0339_res[i].width;
	*h = gc0339_res[i].height;

	pr_info("tried width = %d, height = %d\n",*w,*h);

	return 0;
}

static struct gc0339_res_struct *gc0339_to_res(u32 w, u32 h)
{
	pr_info("%s++\n",__func__);
	int  index;

	for (index = 0; index < N_RES; index++) {
		if ((gc0339_res[index].width == w) &&
			(gc0339_res[index].height == h))
			break;
	}

	/* No mode found */
	if (index >= N_RES)
		return NULL;

	return &gc0339_res[index];
}

static int gc0339_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	pr_info("%s++\n",__func__);
	return gc0339_try_res(&fmt->width, &fmt->height);
}

static int gc0339_res2size(unsigned int res, int *h_size, int *v_size)
{
	pr_info("%s++\n",__func__);
	unsigned short hsize;
	unsigned short vsize;

	switch (res) {
#if 0
	case GC0339_RES_QCIF:
		hsize = GC0339_RES_QCIF_SIZE_H;
		vsize = GC0339_RES_QCIF_SIZE_V;
		break;
	case GC0339_RES_QVGA:
		hsize = GC0339_RES_QVGA_SIZE_H;
		vsize = GC0339_RES_QVGA_SIZE_V;
		break;
#endif
	case GC0339_RES_CIF:
		hsize = GC0339_RES_CIF_SIZE_H;
		vsize = GC0339_RES_CIF_SIZE_V;
		break;
	case GC0339_RES_VGA:
		hsize = GC0339_RES_VGA_SIZE_H;
		vsize = GC0339_RES_VGA_SIZE_V;
		break;
	default:
		WARN(1, "%s: Resolution 0x%08x unknown\n", __func__, res);
		return -EINVAL;
	}

	if (h_size != NULL)
		*h_size = hsize;
	if (v_size != NULL)
		*v_size = vsize;

	return 0;
}

static int gc0339_get_intg_factor(struct i2c_client *client,
				struct camera_mipi_info *info,
				const struct gc0339_res_struct *res)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	struct atomisp_sensor_mode_data *buf = &info->data;
	const unsigned int ext_clk_freq_hz = 19200000;
	const unsigned int pll_invariant_div = 10;
	unsigned int pix_clk_freq_hz;
	u32 pre_pll_clk_div;
	u32 pll_multiplier;
	u32 op_pix_clk_div;
	u8 reg_val;
	u16 val;
	int ret;
	pr_info("%s++\n",__func__);
	dev_err(&client->dev, "%s\n", __func__);

	if (info == NULL)
		return -EINVAL;

	buf->vt_pix_clk_freq_mhz = ext_clk_freq_hz / 2;
	pr_info("vt_pix_clk_freq_mhz = %d\n",buf->vt_pix_clk_freq_mhz);

	/* get integration time */
	buf->coarse_integration_time_min = GC0339_COARSE_INTG_TIME_MIN;
	buf->coarse_integration_time_max_margin =
					GC0339_COARSE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_min = GC0339_FINE_INTG_TIME_MIN;
	buf->fine_integration_time_max_margin =
					GC0339_FINE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_def = GC0339_FINE_INTG_TIME_MIN;

	buf->frame_length_lines = res->lines_per_frame;
	buf->line_length_pck = res->pixels_per_line;
	buf->read_mode = res->bin_mode;

	/* get the cropping and output resolution to ISP for this mode. */
	ret = gc0339_read_reg(client, GC0339_8BIT,  REG_H_START, &reg_val);
	if(ret)
		return ret;
	buf->crop_horizontal_start = reg_val;
	pr_info("crop_horizontal_start = %d\n",buf->crop_horizontal_start);

	ret = gc0339_read_reg(client, GC0339_8BIT,  REG_V_START, &reg_val);
	if(ret)
		return ret;
	buf->crop_vertical_start = reg_val;
	pr_info("crop_vertical_start = %d\n",buf->crop_vertical_start);

	val =0;
	ret = gc0339_read_reg(client, GC0339_8BIT,  REG_WIDTH_H, &reg_val);
	val = reg_val;
	val = val << 8;
	ret = gc0339_read_reg(client, GC0339_8BIT,  REG_WIDTH_L, &reg_val);
	val +=reg_val;
	if(ret)
		return ret;
	buf->output_width = val;
	buf->crop_horizontal_end = buf->crop_horizontal_start+val-1;
	pr_info("crop_horizontal_end = %d\n",buf->crop_horizontal_end);

	val =0;
	ret = gc0339_read_reg(client, GC0339_8BIT,  REG_HEIGHT_H, &reg_val);
	val = reg_val;
	val = val << 8;
	ret = gc0339_read_reg(client, GC0339_8BIT,  REG_HEIGHT_L, &reg_val);
	val +=reg_val;
	if(ret)
		return ret;
	buf->output_height = val;
	buf->crop_vertical_end = buf->crop_vertical_start+val-1;
	pr_info("crop_vertical_end = %d\n",buf->crop_vertical_end);


	val =0;
	ret = gc0339_read_reg(client, GC0339_8BIT,  REG_DUMMY_H, &reg_val);
	val = reg_val;
	val = (val&0x0F) << 8;
	ret = gc0339_read_reg(client, GC0339_8BIT,  REG_H_DUMMY_L, &reg_val);
	val += reg_val;
	pr_info("REG_H_DUMMY = %d\n",val);

	ret = gc0339_read_reg(client, GC0339_8BIT,  REG_SH_DELAY, &reg_val);
	if(ret)
		return ret;
	pr_info("REG_SH_DELAY = %d\n",reg_val);
	val +=reg_val;
	buf->line_length_pck = buf->output_width + val + 4;
	pr_info("line_length_pck = %d\n",buf->line_length_pck);

	val = 0;
	ret = gc0339_read_reg(client, GC0339_8BIT,  REG_DUMMY_H, &reg_val);
	val = reg_val;
	val = (val&0xF0) << 4;
	ret = gc0339_read_reg(client, GC0339_8BIT,  REG_V_DUMMY_L, &reg_val);
	val +=reg_val;
	pr_info("REG_V_DUMMY = %d\n",reg_val);

	buf->frame_length_lines = buf->output_height + reg_val;
	pr_info("frame_length_lines = %d\n",buf->frame_length_lines);

	buf->read_mode = res->bin_mode;
	buf->binning_factor_x = 1;
	buf->binning_factor_y = 1;
	return 0;
}

static int gc0339_get_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	pr_info("%s++\n",__func__);
	int width, height;
	int ret;

	fmt->code = V4L2_MBUS_FMT_SRGGB10_1X10;

	ret = gc0339_res2size(dev->res, &width, &height);
	if (ret)
		return ret;
	fmt->width = width;
	fmt->height = height;

	return 0;
}

static int gc0339_set_mbus_fmt(struct v4l2_subdev *sd,
				  struct v4l2_mbus_framefmt *fmt)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	struct gc0339_res_struct *res_index;
	u32 width = fmt->width;
	u32 height = fmt->height;
	struct camera_mipi_info *gc0339_info = NULL;
	int ret;
	pr_info("%s()++\n",__func__);

	gc0339_info = v4l2_get_subdev_hostdata(sd);
	pr_info("gc0339_info %d %d %d %d\n",
		gc0339_info->port, gc0339_info->input_format,
		gc0339_info->num_lanes, gc0339_info->raw_bayer_order);

	if (gc0339_info == NULL)
		return -EINVAL;

	gc0339_try_res(&width, &height);
	res_index = gc0339_to_res(width, height);

	/* Sanity check */
	if (unlikely(!res_index)) {
		WARN_ON(1);
		return -EINVAL;
	}

	switch (res_index->res) {
	case GC0339_RES_CIF:
		pr_info("gc0339_set_mbus_fmt: CIG\n");
		ret = gc0339_write_reg_array(c, gc0339_cif_init);
		if (ret){
			pr_info("%s: set CIF seq FAIL.\n", __func__);
			return -EINVAL;
		}
		break;
	case GC0339_RES_VGA:
		pr_info("gc0339_set_mbus_fmt: VGA\n");
		ret = gc0339_write_reg_array(c, gc0339_vga_init);
		if (ret) {
			pr_info("%s: set VGA seq FAIL.\n", __func__);
			return -EINVAL;
		}
		break;
	default:
		v4l2_err(sd, "set resolution: %d failed!\n", res_index->res);
		return -EINVAL;
	}

	ret = gc0339_get_intg_factor(c, gc0339_info,
					&gc0339_res[res_index->res]);
	if (ret) {
		dev_err(&c->dev, "failed to get integration_factor\n");
		return -EINVAL;
	}

	/*
	 * gc0339 - we don't poll for context switch
	 * because it does not happen with streaming disabled.
	 */
	dev->res = res_index->res;

	fmt->width = width;
	fmt->height = height;
	fmt->code = V4L2_MBUS_FMT_SRGGB10_1X10;
	pr_info("%s()--\n",__func__);
	return 0;
}

/* TODO: Update to SOC functions, remove exposure and gain */
static int gc0339_g_focal(struct v4l2_subdev *sd, s32 * val)
{
	*val = (GC0339_FOCAL_LENGTH_NUM << 16) | GC0339_FOCAL_LENGTH_DEM;
	return 0;
}

static int gc0339_g_fnumber(struct v4l2_subdev *sd, s32 * val)
{
	/*const f number for gc0339*/
	*val = (GC0339_F_NUMBER_DEFAULT_NUM << 16) | GC0339_F_NUMBER_DEM;
	return 0;
}

static int gc0339_g_fnumber_range(struct v4l2_subdev *sd, s32 * val)
{
	*val = (GC0339_F_NUMBER_DEFAULT_NUM << 24) |
		(GC0339_F_NUMBER_DEM << 16) |
		(GC0339_F_NUMBER_DEFAULT_NUM << 8) | GC0339_F_NUMBER_DEM;
	return 0;
}

/* Horizontal flip the image. */
static int gc0339_g_hflip(struct v4l2_subdev *sd, s32 * val)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	int ret;
	u32 data;
	ret = gc0339_read_reg(c, GC0339_16BIT,
			(u32)GC0339_READ_MODE, &data);
	if (ret)
		return ret;
	*val = !!(data & GC0339_HFLIP_MASK);

	return 0;
}

static int gc0339_g_vflip(struct v4l2_subdev *sd, s32 * val)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	int ret;
	u32 data;

	ret = gc0339_read_reg(c, GC0339_16BIT,
			(u32)GC0339_READ_MODE, &data);
	if (ret)
		return ret;
	*val = !!(data & GC0339_VFLIP_MASK);

	return 0;
}

static int gc0339_s_freq(struct v4l2_subdev *sd, s32  val)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	int ret;

	if (val != GC0339_FLICKER_MODE_50HZ &&
			val != GC0339_FLICKER_MODE_60HZ)
		return -EINVAL;

	if (val == GC0339_FLICKER_MODE_50HZ) {
		ret = gc0339_write_reg_array(c, gc0339_antiflicker_50hz);
		if (ret < 0)
			return ret;
	} else {
		ret = gc0339_write_reg_array(c, gc0339_antiflicker_60hz);
		if (ret < 0)
			return ret;
	}

	if (ret == 0)
		dev->lightfreq = val;

	return ret;
}

static int gc0339_g_2a_status(struct v4l2_subdev *sd, s32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	unsigned int status_exp, status_wb;
	*val = 0;

	ret = gc0339_read_reg(client, GC0339_16BIT,
			GC0339_AE_TRACK_STATUS, &status_exp);
	if (ret)
		return ret;

	ret = gc0339_read_reg(client, GC0339_16BIT,
			GC0339_AWB_STATUS, &status_wb);
	if (ret)
		return ret;

	if (status_exp & GC0339_AE_READY)
		*val = V4L2_2A_STATUS_AE_READY;

	if (status_wb & GC0339_AWB_STEADY)
		*val |= V4L2_2A_STATUS_AWB_READY;

	return 0;
}

static long gc0339_s_exposure(struct v4l2_subdev *sd,
					struct atomisp_exposure *exposure)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	pr_info("%s++\n",__func__);
	dev_err(&client->dev, "%s(0x%X 0x%X 0x%X)\n", __func__,
		exposure->integration_time[0], exposure->gain[0], exposure->gain[1]);

	int ret=0;

#if 1
	struct gc0339_device *dev = to_gc0339_sensor(sd);

	unsigned int coarse_integration = 0;
	unsigned int fine_integration = 0;
	unsigned int FLines = 0;
	unsigned int FrameLengthLines = 0; //ExposureTime.FrameLengthLines;
	unsigned int AnalogGain, DigitalGain;
	u32 AnalogGainToWrite = 0;
	u16 exposure_local[3];
	u8 expo_coarse_h,expo_coarse_l;
	u32 RegSwResetData = 0;

	coarse_integration = exposure->integration_time[0];
	//fine_integration = ExposureTime.FineIntegrationTime;
	//FrameLengthLines = ExposureTime.FrameLengthLines;
	FLines = gc0339_res[dev->res].lines_per_frame;
	AnalogGain = exposure->gain[0];
	DigitalGain = exposure->gain[1];
	//DigitalGain = 0x400 * (((u16) DigitalGain) >> 8) + ((unsigned int)(0x400 * (((u16) DigitalGain) & 0xFF)) >>8);

	//Hindden Register usage in REG_SW_RESET bit 15. set REG_SW_RESET bit 15 as 1 for group apply.
	// sequence as below, set bit 15 as--> set gain line --> set bit 15 -->0
	//ret = gc0339_read_reg(client, GC0339_16BIT, REG_SW_RESET, &RegSwResetData);
	//RegSwResetData |= 0x8000;
	//ret = gc0339_write_reg(client, GC0339_16BIT, REG_SW_RESET, RegSwResetData);

	//set frame length
	if (FLines < coarse_integration + 6)
		FLines = coarse_integration + 6;
	if (FLines < FrameLengthLines)
		FLines = FrameLengthLines;
	//ret = gc0339_write_reg(client, GC0339_16BIT, 0x300A, FLines);
	if (ret) {
		v4l2_err(client, "%s: fail to set FLines\n", __func__);
		return -EINVAL;
	}

	// Reset group apply as it will be cleared in bayer mode
	//ret = gc0339_write_reg(client, GC0339_16BIT, REG_SW_RESET, RegSwResetData);

	//set coarse/fine integration
	exposure_local[0] = REG_EXPO_COARSE;
	exposure_local[1] = (u16)coarse_integration;
	exposure_local[2] = (u16)fine_integration;
	// 3A provide real exposure time. should not translate to any value here.

	expo_coarse_h = (u8)(coarse_integration>>8);
	expo_coarse_l = (u8)(coarse_integration & 0xff);

	ret = gc0339_write_reg(client, GC0339_8BIT, REG_EXPO_COARSE,expo_coarse_h);
	ret = gc0339_write_reg(client, GC0339_8BIT, REG_EXPO_COARSE+1,expo_coarse_l);

	if (ret) {
		 v4l2_err(client, "%s: fail to set exposure time\n", __func__);
		 return -EINVAL;
	}

	 // Reset group apply as it will be cleared in bayer mode
	 //ret = gc0339_write_reg(client, GC0339_16BIT, REG_SW_RESET, RegSwResetData);

	 /*
	// set analog/digital gain
	switch(AnalogGain)
	{
	  case 0:
		  AnalogGainToWrite = 0x0;
		  break;
	  case 1:
		  AnalogGainToWrite = 0x20;
		  break;
	  case 2:
		  AnalogGainToWrite = 0x60;
		  break;
	  case 4:
		  AnalogGainToWrite = 0xA0;
		  break;
	  case 8:
		  AnalogGainToWrite = 0xE0;
		  break;
	  default:
		  AnalogGainToWrite = 0x20;
		  break;
	}
	*/
	if (DigitalGain >= 16 || DigitalGain <= 1)
		DigitalGain = 1;
	// AnalogGainToWrite = (u16)((DigitalGain << 12) | AnalogGainToWrite);
	AnalogGainToWrite = (u16)((DigitalGain << 12) | (u16)AnalogGain);
	ret = gc0339_write_reg(client, GC0339_8BIT, REG_GAIN, AnalogGain);
	if (ret) {
		v4l2_err(client, "%s: fail to set AnalogGainToWrite\n", __func__);
		return -EINVAL;
	}

	// Reset group apply as it will be cleared in bayer mode
	//ret = gc0339_write_reg(client, GC0339_16BIT, REG_SW_RESET, RegSwResetData);

	//ret = gc0339_read_reg(client, GC0339_16BIT, REG_SW_RESET, &RegSwResetData);
	RegSwResetData &= 0x7FFF;
	//ret = gc0339_write_reg(client, GC0339_16BIT, REG_SW_RESET, RegSwResetData);

//	DoTraceMessage(FLAG_LOG,
//		"%s LocalCmd_SetExposure (%d) vts (%d) analoggain (%d) digitalgain (%d) returns with code: 0x%x\n", DEVICE_NAME,
//		ExposureTime.CoarseIntegrationTime, ExposureTime.FrameLengthLines, ExposureTime.AnalogGain, ExposureTime.DigitalGain, ret);
#endif
	return ret;
}

static long gc0339_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	pr_info("%s++\n",__func__);
	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return gc0339_s_exposure(sd, arg);
	default:
		return -EINVAL;
	}
	return 0;
}


/* This returns the exposure time being used. This should only be used
	for filling in EXIF data, not for actual image processing. */
static int gc0339_g_exposure(struct v4l2_subdev *sd, s32 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	pr_info("%s++\n",__func__);
	u16 coarse;
	u8 reg_val_h,reg_val_l;
	int ret;

	/* the fine integration time is currently not calculated */
	ret = gc0339_read_reg(client, GC0339_8BIT,
				GC0339_COARSE_INTEGRATION_TIME_H, &reg_val_h);
	if (ret)
		return ret;

	coarse = ((u16)(reg_val_h & 0x0f)) <<8;

	ret = gc0339_read_reg(client, GC0339_8BIT,
				GC0339_COARSE_INTEGRATION_TIME_L, &reg_val_l);
	if (ret)
		return ret;

	coarse |= reg_val_l;

	*value = coarse;
	return 0;
}

static struct gc0339_control gc0339_controls[] = {
/*
	{
		.qc = {
			.id = V4L2_CID_VFLIP,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Image v-Flip",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.query = gc0339_g_vflip,
		.tweak = gc0339_t_vflip,
	},
	{
		.qc = {
			.id = V4L2_CID_HFLIP,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Image h-Flip",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.query = gc0339_g_hflip,
		.tweak = gc0339_t_hflip,
	},
*/
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = GC0339_FOCAL_LENGTH_DEFAULT,
			.maximum = GC0339_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = GC0339_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = gc0339_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = GC0339_F_NUMBER_DEFAULT,
			.maximum = GC0339_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = GC0339_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = gc0339_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = GC0339_F_NUMBER_RANGE,
			.maximum =  GC0339_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = GC0339_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = gc0339_g_fnumber_range,
	},
	{
		.qc = {
			.id = V4L2_CID_POWER_LINE_FREQUENCY,
			.type = V4L2_CTRL_TYPE_MENU,
			.name = "Light frequency filter",
			.minimum = 1,
			.maximum =  2, /* 1: 50Hz, 2:60Hz */
			.step = 1,
			.default_value = 1,
			.flags = 0,
		},
		.tweak = gc0339_s_freq,
	},
	{
		.qc = {
			.id = V4L2_CID_2A_STATUS,
			.type = V4L2_CTRL_TYPE_BITMASK,
			.name = "AE and AWB status",
			.minimum = 0,
			.maximum = V4L2_2A_STATUS_AE_READY |
				V4L2_2A_STATUS_AWB_READY,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = gc0339_g_2a_status,
	},
	{
		.qc = {
			.id = V4L2_CID_EXPOSURE_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "exposure",
			.minimum = 0x0,
			.maximum = 0xffff,
			.step = 0x01,
			.default_value = 0x00,
			.flags = 0,
		},
		.query = gc0339_g_exposure,
	},

};
#define N_CONTROLS (ARRAY_SIZE(gc0339_controls))

static struct gc0339_control *gc0339_find_control(__u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++) {
		if (gc0339_controls[i].qc.id == id)
			return &gc0339_controls[i];
	}
	return NULL;
}

static int gc0339_detect(struct gc0339_device *dev, struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	pr_info("%s++\n",__func__);
	u8 retvalue;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: i2c error", __func__);
		return -ENODEV;
	}

	gc0339_read_reg(client, GC0339_8BIT, (u8)GC0339_PID, &retvalue);
	dev->real_model_id = retvalue;
	pr_info("%s(%d): detect module ID = %x\n",
			__func__, __LINE__, retvalue);

	if (retvalue != GC0339_MOD_ID) {
		ATD_gc0339_status = 0;
		dev_err(&client->dev, "%s: failed: client->addr = %x\n",
						__func__, client->addr);
		return -ENODEV;
	} else {
		ATD_gc0339_status = 1;
	}
	pr_info("%s--\n",__func__);
	return 0;
}

static int
gc0339_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	pr_info("%s++\n",__func__);

	if (NULL == platform_data)
		return -ENODEV;

	dev->platform_data =
		(struct camera_sensor_platform_data *)platform_data;

	if (dev->platform_data->platform_init) {
		ret = dev->platform_data->platform_init(client);
		if (ret) {
			v4l2_err(client, "gc0339 platform init err\n");
			return ret;
		}
	}
	ret = gc0339_s_power(sd, 1);
	if (ret) {
		v4l2_err(client, "gc0339 power-up err");
		gc0339_s_power(sd, 0);
		return ret;
	}

	/* config & detect sensor */
	ret = gc0339_detect(dev, client);
	if (ret) {
		v4l2_err(client, "gc0339_detect err s_config.\n");
		goto fail_detect;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	ret = gc0339_set_suspend(sd);
	if (ret) {
		v4l2_err(client, "gc0339 suspend err");
		return ret;
	}

	ret = gc0339_s_power(sd, 0);
	if (ret) {
		v4l2_err(client, "gc0339 power down err");
		return ret;
	}

	return 0;

fail_csi_cfg:
	dev->platform_data->csi_cfg(sd, 0);
fail_detect:
	gc0339_s_power(sd, 0);
	dev_err(&client->dev, "sensor power-gating failed\n");
	return ret;
}

static int gc0339_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct gc0339_control *ctrl = gc0339_find_control(qc->id);
	pr_info("%s++\n",__func__);

	if (ctrl == NULL)
		return -EINVAL;
	*qc = ctrl->qc;
	return 0;
}

/* Horizontal flip the image. */
static int gc0339_t_hflip(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	int err;
	/* set for direct mode */
	err = gc0339_write_reg(c, GC0339_16BIT, 0x098E, 0xC850);
	if (value) {
		/* enable H flip ctx A */
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC850, 0x01, 0x01);
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC851, 0x01, 0x01);
		/* ctx B */
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC888, 0x01, 0x01);
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC889, 0x01, 0x01);

		err += gc0339_rmw_reg(c, GC0339_16BIT, GC0339_READ_MODE,
					GC0339_HFLIP_MASK, GC0339_FLIP_EN);

		dev->bpat = GC0339_BPAT_GRGRBGBG;
	} else {
		/* disable H flip ctx A */
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC850, 0x01, 0x00);
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC851, 0x01, 0x00);
		/* ctx B */
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC888, 0x01, 0x00);
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC889, 0x01, 0x00);

		err += gc0339_rmw_reg(c, GC0339_16BIT, GC0339_READ_MODE,
					GC0339_HFLIP_MASK, GC0339_FLIP_DIS);

		dev->bpat = GC0339_BPAT_BGBGGRGR;
	}

	err += gc0339_write_reg(c, GC0339_8BIT, 0x8404, 0x06);
	udelay(10);

	return !!err;
}

/* Vertically flip the image */
static int gc0339_t_vflip(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	int err;
	/* set for direct mode */
	err = gc0339_write_reg(c, GC0339_16BIT, 0x098E, 0xC850);
	if (value >= 1) {
		/* enable H flip - ctx A */
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC850, 0x02, 0x01);
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC851, 0x02, 0x01);
		/* ctx B */
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC888, 0x02, 0x01);
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC889, 0x02, 0x01);

		err += gc0339_rmw_reg(c, GC0339_16BIT, GC0339_READ_MODE,
					GC0339_VFLIP_MASK, GC0339_FLIP_EN);
	} else {
		/* disable H flip - ctx A */
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC850, 0x02, 0x00);
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC851, 0x02, 0x00);
		/* ctx B */
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC888, 0x02, 0x00);
		err += gc0339_rmw_reg(c, GC0339_8BIT, 0xC889, 0x02, 0x00);

		err += gc0339_rmw_reg(c, GC0339_16BIT, GC0339_READ_MODE,
					GC0339_VFLIP_MASK, GC0339_FLIP_DIS);
	}

	err += gc0339_write_reg(c, GC0339_8BIT, 0x8404, 0x06);
	udelay(10);

	return !!err;
}

static int gc0339_s_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	pr_info("%s++\n",__func__);
#if 0
	struct ov2722_device *dev = to_ov2722_sensor(sd);
	dev->run_mode = param->parm.capture.capturemode;

	mutex_lock(&dev->input_lock);
	switch (dev->run_mode) {
	case CI_MODE_VIDEO:
		ov2722_res = ov2722_res_video;
		N_RES = N_RES_VIDEO;
		break;
	case CI_MODE_STILL_CAPTURE:
		ov2722_res = ov2722_res_still;
		N_RES = N_RES_STILL;
		break;
	default:
		ov2722_res = ov2722_res_preview;
		N_RES = N_RES_PREVIEW;
	}
	mutex_unlock(&dev->input_lock);
#endif
	return 0;
}

static int gc0339_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc0339_control *octrl = gc0339_find_control(ctrl->id);
	int ret;

	if (octrl == NULL)
		return -EINVAL;

	ret = octrl->query(sd, &ctrl->value);
	if (ret < 0)
		return ret;

	return 0;
}

static int gc0339_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc0339_control *octrl = gc0339_find_control(ctrl->id);
	int ret;

	pr_info("%s++\n",__func__);

	if (!octrl || !octrl->tweak)
		return -EINVAL;

	ret = octrl->tweak(sd, ctrl->value);
	if (ret < 0)
		return ret;

	return 0;
}

static int gc0339_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	struct i2c_client *c = v4l2_get_subdevdata(sd);

	if (enable) {
		pr_info("gc0339_s_stream: Stream On\n");
		ret = gc0339_set_streaming(sd);
	} else {
		pr_info("gc0339_s_stream: Stream Off\n");
		ret = gc0339_set_suspend(sd);
	}

	return ret;
}

static int
gc0339_enum_framesizes(struct v4l2_subdev *sd, struct v4l2_frmsizeenum *fsize)
{
	pr_info("%s++\n",__func__);
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = gc0339_res[index].width;
	fsize->discrete.height = gc0339_res[index].height;

	/* FIXME: Wrong way to know used mode */
	fsize->reserved[0] = gc0339_res[index].used;

	return 0;
}

static int gc0339_enum_frameintervals(struct v4l2_subdev *sd,
						struct v4l2_frmivalenum *fival)
{
	pr_info("%s++\n",__func__);
	unsigned int index = fival->index;
	int i;

	if (index >= N_RES)
		return -EINVAL;

	/* find out the first equal or bigger size */
	for (i = 0; i < N_RES; i++) {
		if ((gc0339_res[i].width >= fival->width) &&
			(gc0339_res[i].height >= fival->height))
			break;
	}
	if (i == N_RES)
		i--;

	index = i;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = gc0339_res[index].fps;

	return 0;
}

static int
gc0339_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	pr_info("%s++\n",__func__);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_GC0339, 0);
}

static int gc0339_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	pr_info("%s++\n",__func__);
	if (code->index)
		return -EINVAL;

	code->code = V4L2_MBUS_FMT_SRGGB10_1X10;

	return 0;
}

static int gc0339_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh,
	struct v4l2_subdev_frame_size_enum *fse)
{
	pr_info("%s++\n",__func__);
	unsigned int index = fse->index;


	if (index >= N_RES)
		return -EINVAL;

	fse->min_width = gc0339_res[index].width;
	fse->min_height = gc0339_res[index].height;
	fse->max_width = gc0339_res[index].width;
	fse->max_height = gc0339_res[index].height;

	return 0;
}

static struct v4l2_mbus_framefmt *
__gc0339_get_pad_format(struct gc0339_device *sensor,
			 struct v4l2_subdev_fh *fh, unsigned int pad,
			 enum v4l2_subdev_format_whence which)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);
	pr_info("%s++\n",__func__);

	if (pad != 0) {
		dev_err(&client->dev,  "%s err. pad %x\n", __func__, pad);
		return NULL;
	}

	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(fh, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &sensor->format;
	default:
		return NULL;
	}
}

static int
gc0339_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct gc0339_device *snr = to_gc0339_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__gc0339_get_pad_format(snr, fh, fmt->pad, fmt->which);
	pr_info("%s++\n",__func__);

	if (format == NULL)
		return -EINVAL;
	fmt->format = *format;

	return 0;
}

static int
gc0339_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct gc0339_device *snr = to_gc0339_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__gc0339_get_pad_format(snr, fh, fmt->pad, fmt->which);
	pr_info("%s++\n",__func__);

	if (format == NULL)
		return -EINVAL;

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		snr->format = fmt->format;

	return 0;
}

static int gc0339_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	int index;
	struct gc0339_device *snr = to_gc0339_sensor(sd);
	pr_info("%s++\n",__func__);

	if (frames == NULL)
		return -EINVAL;

	for (index = 0; index < N_RES; index++) {
		if (gc0339_res[index].res == snr->res)
			break;
	}

	if (index >= N_RES)
		return -EINVAL;

	*frames = gc0339_res[index].skip_frames;

	return 0;
}

static const struct v4l2_subdev_video_ops gc0339_video_ops = {
	.try_mbus_fmt = gc0339_try_mbus_fmt,
	.s_mbus_fmt = gc0339_set_mbus_fmt,
	.g_mbus_fmt = gc0339_get_mbus_fmt,
	.s_stream = gc0339_s_stream,
	.enum_framesizes = gc0339_enum_framesizes,
	.enum_frameintervals = gc0339_enum_frameintervals,
	.s_parm = gc0339_s_parm,
};

static struct v4l2_subdev_sensor_ops gc0339_sensor_ops = {
	.g_skip_frames	= gc0339_g_skip_frames,
};

static const struct v4l2_subdev_core_ops gc0339_core_ops = {
	.g_chip_ident = gc0339_g_chip_ident,
	.queryctrl = gc0339_queryctrl,
	.g_ctrl = gc0339_g_ctrl,
	.s_ctrl = gc0339_s_ctrl,
	.s_power = gc0339_s_power,
	.ioctl = gc0339_ioctl,
};

/* REVISIT: Do we need pad operations? */
static const struct v4l2_subdev_pad_ops gc0339_pad_ops = {
	.enum_mbus_code = gc0339_enum_mbus_code,
	.enum_frame_size = gc0339_enum_frame_size,
	.get_fmt = gc0339_get_pad_format,
	.set_fmt = gc0339_set_pad_format,
};

static const struct v4l2_subdev_ops gc0339_ops = {
	.core = &gc0339_core_ops,
	.video = &gc0339_video_ops,
	.pad = &gc0339_pad_ops,
	.sensor = &gc0339_sensor_ops,
};

static const struct media_entity_operations gc0339_entity_ops = {
	.link_setup = NULL,
};


static int gc0339_remove(struct i2c_client *client)
{
	struct gc0339_device *dev;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	dev = container_of(sd, struct gc0339_device, sd);
	dev->platform_data->csi_cfg(sd, 0);
	if (dev->platform_data->platform_deinit)
		dev->platform_data->platform_deinit();
	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&dev->sd.entity);
	kfree(dev);
	return 0;
}

static int gc0339_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct gc0339_device *dev;
	int ret;
	pr_info("%s()++\n", __func__);

/* TODO: project id check */
#if 0
	/* Project id check. */
	project_id = asustek_get_project_id();
	switch(project_id) {
		case PROJECT_ID_ME572C:
		case PROJECT_ID_FE375CXG:
		break;
		default:
			pr_info("gc0339 is unsupported on this board(0x%x)\n",
				project_id);
		return -EINVAL;
	}
#endif

	/* Setup sensor configuration structure */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&dev->sd, client, &gc0339_ops);

	//Add for ATD read camera status+++
	dev->sensor_i2c_attribute.attrs = gc0339_attributes;

	// Register sysfs hooks
	ret = sysfs_create_group(&client->dev.kobj, &dev->sensor_i2c_attribute);
	if (ret) {
		dev_err(&client->dev, "Not able to create the sysfs\n");
		return ret;
	}
	//Add for ATD read camera status---

	if (client->dev.platform_data) {
		ret = gc0339_s_config(&dev->sd, client->irq,
						client->dev.platform_data);
		if (ret) {
			v4l2_device_unregister_subdev(&dev->sd);
			kfree(dev);
			return ret;
		}
	}

	/*TODO add format code here*/
	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->format.code = V4L2_MBUS_FMT_SRGGB10_1X10;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;


	/* REVISIT: Do we need media controller? */
	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret) {
		gc0339_remove(client);
		return ret;
	}

	/* set res index to be invalid */
	dev->res = -1;
	pr_info("%s()--\n", __func__);
	return 0;
}

MODULE_DEVICE_TABLE(i2c, gc0339_id);

static struct i2c_driver gc0339_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "gc0339"
	},
	.probe = gc0339_probe,
	.remove = gc0339_remove,
	.id_table = gc0339_id,
};

static __init int init_gc0339(void)
{
	return i2c_add_driver(&gc0339_driver);
}

static __exit void exit_gc0339(void)
{
	i2c_del_driver(&gc0339_driver);
}

module_init(init_gc0339);
module_exit(exit_gc0339);

MODULE_AUTHOR("Shuguang Gong <Shuguang.gong@intel.com>");
MODULE_LICENSE("GPL");
