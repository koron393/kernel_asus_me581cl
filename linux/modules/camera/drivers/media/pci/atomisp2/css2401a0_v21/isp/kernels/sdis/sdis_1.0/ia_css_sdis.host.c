/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
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

#include "memory_access.h"
#include "assert_support.h"
#include "ia_css_debug.h"
#include "ia_css_sdis_param.h"
#include "ia_css_sdis.host.h"

/* Store the DIS coefficients from the 3A library to DDR where the ISP
   will read them from. The ISP works on a grid that can be larger than
   that of the 3a library. If that is the case, we padd the difference
   with zeroes. */
void store_dis_coefficients(
	const short *dis_hor_coef_tbl,
	const short *dis_ver_coef_tbl,
	const struct ia_css_binary *binary,
	hrt_vaddress ddr_addr_hor,
	hrt_vaddress ddr_addr_ver)
{
	const struct ia_css_sdis_info *dis = &binary->dis;
	unsigned int hor_num_isp, ver_num_isp,
		     hor_num_3a, ver_num_3a,
		     hor_padding, ver_padding;
	int i;
	const short *hor_ptr_3a,
		*ver_ptr_3a;
	hrt_vaddress hor_ptr_isp = ddr_addr_hor,
		ver_ptr_isp = ddr_addr_ver;

	IA_CSS_ENTER_PRIVATE("void");

	assert(binary != NULL);
	assert(ddr_addr_hor != mmgr_NULL);
	assert(ddr_addr_ver != mmgr_NULL);

	hor_num_isp = dis->coef.pad.width,
	ver_num_isp = dis->coef.pad.height,
	hor_num_3a  = dis->coef.dim.width,
	ver_num_3a  = dis->coef.dim.height,
	hor_padding = hor_num_isp - hor_num_3a,
	ver_padding = ver_num_isp - ver_num_3a;
	hor_ptr_3a = dis_hor_coef_tbl,
	ver_ptr_3a = dis_ver_coef_tbl;

	for (i = 0; i < IA_CSS_DVS_NUM_COEF_TYPES; i++) {
		if (dis_hor_coef_tbl != NULL) {
			mmgr_store(hor_ptr_isp,
				hor_ptr_3a, hor_num_3a * sizeof(*hor_ptr_3a));
			hor_ptr_3a  += hor_num_3a;
		} else {
			mmgr_clear(hor_ptr_isp,
				hor_num_3a * sizeof(*hor_ptr_3a));
		}
		hor_ptr_isp += hor_num_3a * sizeof(short);
		mmgr_clear(hor_ptr_isp, hor_padding * sizeof(short));
		hor_ptr_isp += hor_padding * sizeof(short);
	}
	for (i = 0; i < SH_CSS_DIS_VER_NUM_COEF_TYPES(binary); i++) {
		if (dis_ver_coef_tbl != NULL) {
			mmgr_store(ver_ptr_isp,
				ver_ptr_3a, ver_num_3a * sizeof(*ver_ptr_3a));
			ver_ptr_3a  += ver_num_3a;
		} else {
			mmgr_clear(ver_ptr_isp,
				ver_num_3a * sizeof(*ver_ptr_3a));
		}
		ver_ptr_isp += ver_num_3a * sizeof(short);
		mmgr_clear(ver_ptr_isp, ver_padding * sizeof(short));
		ver_ptr_isp += ver_padding * sizeof(short);
	}

	IA_CSS_LEAVE_PRIVATE("void");
}

void ia_css_get_isp_dis_coefficients(
	struct ia_css_stream *stream,
	short *horizontal_coefficients,
	short *vertical_coefficients)
{
	struct ia_css_isp_parameters *params;
	unsigned int hor_num_isp, ver_num_isp;
	int i;
	short *hor_ptr = horizontal_coefficients,
	      *ver_ptr = vertical_coefficients;
	hrt_vaddress hor_ptr_isp;
	hrt_vaddress ver_ptr_isp;
	struct ia_css_binary *dvs_binary;

	IA_CSS_ENTER("void");

	assert(horizontal_coefficients != NULL);
	assert(vertical_coefficients != NULL);

	params = stream->isp_params_configs;

	/* Only video pipe supports DVS */
	hor_ptr_isp = params->pipe_ddr_ptrs[IA_CSS_PIPE_ID_VIDEO].sdis_hor_coef;
	ver_ptr_isp = params->pipe_ddr_ptrs[IA_CSS_PIPE_ID_VIDEO].sdis_ver_coef;
	dvs_binary = ia_css_stream_get_dvs_binary(stream);
	if (!dvs_binary)
		return;

	hor_num_isp = dvs_binary->dis.coef.pad.width;
	ver_num_isp = dvs_binary->dis.coef.pad.height;

	for (i = 0; i < IA_CSS_DVS_NUM_COEF_TYPES; i++) {
		mmgr_load(hor_ptr_isp, hor_ptr, hor_num_isp * sizeof(short));
		hor_ptr_isp += hor_num_isp * sizeof(short);
		hor_ptr     += hor_num_isp;
	}
	for (i = 0; i < SH_CSS_DIS_VER_NUM_COEF_TYPES(dvs_binary); i++) {
		mmgr_load(ver_ptr_isp, ver_ptr, ver_num_isp * sizeof(short));
		ver_ptr_isp += ver_num_isp * sizeof(short);
		ver_ptr     += ver_num_isp;
	}

	IA_CSS_LEAVE("void");
}

size_t
sdis_hor_coef_tbl_bytes(const struct ia_css_binary *binary)
{
	if (binary->info->sp.isp_pipe_version == 1)
		return sizeof(short) * IA_CSS_DVS_NUM_COEF_TYPES  * binary->dis.coef.pad.width;
	else
		return sizeof(short) * IA_CSS_DVS2_NUM_COEF_TYPES * binary->dis.coef.pad.width;
}

size_t
sdis_ver_coef_tbl_bytes(const struct ia_css_binary *binary)
{
	return sizeof(short) * SH_CSS_DIS_VER_NUM_COEF_TYPES(binary) * binary->dis.coef.pad.height;
}

void
ia_css_sdis_init_info(
	struct ia_css_sdis_info *dis,
	unsigned sc_3a_dis_width,
	unsigned sc_3a_dis_padded_width,
	unsigned sc_3a_dis_height,
	unsigned isp_pipe_version,
	unsigned enabled)
{
	if (!enabled) {
		struct ia_css_sdis_info default_dis = IA_CSS_DEFAULT_SDIS_INFO;
		*dis = default_dis;
		return;
	}

	dis->deci_factor_log2 = SH_CSS_DIS_DECI_FACTOR_LOG2;

	dis->grid.dim.width  =
			_ISP_SDIS_HOR_GRID_NUM_3A(sc_3a_dis_width,
						  SH_CSS_DIS_DECI_FACTOR_LOG2);
	dis->grid.dim.height  =
			_ISP_SDIS_VER_GRID_NUM_3A(sc_3a_dis_height,
						  SH_CSS_DIS_DECI_FACTOR_LOG2);
	dis->grid.pad.width =
			_ISP_SDIS_HOR_GRID_NUM_ISP(sc_3a_dis_padded_width,
						SH_CSS_DIS_DECI_FACTOR_LOG2);
	dis->grid.pad.height =
			_ISP_SDIS_VER_GRID_NUM_ISP(sc_3a_dis_height,
						SH_CSS_DIS_DECI_FACTOR_LOG2);

	dis->coef.dim.width  =
			_ISP_SDIS_HOR_COEF_NUM_3A(sc_3a_dis_width,
						  SH_CSS_DIS_DECI_FACTOR_LOG2);
	dis->coef.dim.height  =
			_ISP_SDIS_VER_COEF_NUM_3A(sc_3a_dis_height,
						  SH_CSS_DIS_DECI_FACTOR_LOG2);
	dis->coef.pad.width =
			_ISP_SDIS_HOR_COEF_NUM_ISP(sc_3a_dis_padded_width);
	dis->coef.pad.height =
			_ISP_SDIS_VER_COEF_NUM_ISP(sc_3a_dis_height);
	dis->proj.dim.width  =
			_ISP_SDIS_HOR_PROJ_NUM_3A(
				sc_3a_dis_width,
				sc_3a_dis_height,
				SH_CSS_DIS_DECI_FACTOR_LOG2,
				isp_pipe_version);
	dis->proj.dim.height  =
			_ISP_SDIS_VER_PROJ_NUM_3A(
				sc_3a_dis_width,
				sc_3a_dis_height,
				SH_CSS_DIS_DECI_FACTOR_LOG2,
				isp_pipe_version);
	dis->proj.pad.width =
			__ISP_SDIS_HOR_PROJ_NUM_ISP(sc_3a_dis_padded_width,
				sc_3a_dis_height,
				SH_CSS_DIS_DECI_FACTOR_LOG2,
				isp_pipe_version);
	dis->proj.pad.height =
			__ISP_SDIS_VER_PROJ_NUM_ISP(sc_3a_dis_padded_width,
				sc_3a_dis_height,
				SH_CSS_DIS_DECI_FACTOR_LOG2,
				isp_pipe_version);
}


