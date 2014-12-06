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

#ifndef __IA_CSS_SDIS2_HOST_H
#define __IA_CSS_SDIS2_HOST_H

#include "ia_css_sdis2_types.h"
#include "ia_css_binary.h"
#include "ia_css_stream.h"
#include "sh_css_params.h"

void store_dvs2_coefficients(
	struct ia_css_isp_parameters *params,
	const struct ia_css_binary *binary,
	hrt_vaddress ddr_addr_hor,
	hrt_vaddress ddr_addr_ver);

void ia_css_get_isp_dvs2_coefficients(
	struct ia_css_stream *stream,
	short *hor_coefs_odd_real,
	short *hor_coefs_odd_imag,
	short *hor_coefs_even_real,
	short *hor_coefs_even_imag,
	short *ver_coefs_odd_real,
	short *ver_coefs_odd_imag,
	short *ver_coefs_even_real,
	short *ver_coefs_even_imag);

#endif /* __IA_CSS_SDIS2_HOST_H */
