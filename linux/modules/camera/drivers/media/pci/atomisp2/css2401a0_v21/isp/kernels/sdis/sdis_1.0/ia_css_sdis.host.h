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

#ifndef __IA_CSS_SDIS_HOST_H
#define __IA_CSS_SDIS_HOST_H

#include "ia_css_sdis_types.h"
#include "ia_css_binary.h"
#include "ia_css_stream.h"
#include "sh_css_params.h"

void store_dis_coefficients(
	const short *dis_hor_coef_tbl,
	const short *dis_ver_coef_tbl,
	const struct ia_css_binary *binary,
	hrt_vaddress ddr_addr_hor,
	hrt_vaddress ddr_addr_ver);

void ia_css_get_isp_dis_coefficients(
	struct ia_css_stream *stream,
	short *horizontal_coefficients,
	short *vertical_coefficients);

size_t sdis_hor_coef_tbl_bytes(const struct ia_css_binary *binary);
size_t sdis_ver_coef_tbl_bytes(const struct ia_css_binary *binary);

void
ia_css_sdis_init_info(
	struct ia_css_sdis_info *dis,
	unsigned sc_3a_dis_width,
	unsigned sc_3a_dis_padded_width,
	unsigned sc_3a_dis_height,
	unsigned isp_pipe_version,
	unsigned enabled);

#endif /* __IA_CSS_SDIS_HOST_H */
