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

#include <assert_support.h>
#include "memory_access.h"
#include "ia_css_debug.h"
#include "ia_css_sdis2.host.h"

static void storedvs2_coef(const short *ptr_3a, hrt_vaddress ptr_isp, unsigned num_3a, unsigned padding)
{
	if (ptr_3a != NULL) {
		mmgr_store(ptr_isp, ptr_3a, num_3a * sizeof(*ptr_3a));
	} else {
		mmgr_clear(ptr_isp, num_3a * sizeof(*ptr_3a));
	}
	ptr_isp += num_3a * sizeof(short);
	mmgr_clear(ptr_isp, padding * sizeof(short));

}

void store_dvs2_coefficients(
	struct ia_css_isp_parameters *params,
	const struct ia_css_binary *binary,
	hrt_vaddress ddr_addr_hor,
	hrt_vaddress ddr_addr_ver)
{
	unsigned int hor_num_isp, ver_num_isp,
		     hor_num_3a, ver_num_3a,
		     hor_padding, ver_padding;
	hrt_vaddress hor_ptr_isp = ddr_addr_hor,
		ver_ptr_isp = ddr_addr_ver;

	IA_CSS_ENTER_PRIVATE("void");

	assert(binary != NULL);
	assert(ddr_addr_hor != mmgr_NULL);
	assert(ddr_addr_ver != mmgr_NULL);

	hor_num_isp = binary->dis.coef.pad.width,
	ver_num_isp = binary->dis.coef.pad.height,
	hor_num_3a  = binary->dis.coef.dim.width,
	ver_num_3a  = binary->dis.coef.dim.height,
	hor_padding = hor_num_isp - hor_num_3a,
	ver_padding = ver_num_isp - ver_num_3a;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "store_dvs2_coefficients() enter:\n");

	storedvs2_coef(params->dvs2_hor_coefs.odd_real, hor_ptr_isp, hor_num_3a, hor_padding);
	hor_ptr_isp += hor_num_isp * sizeof(short);
	storedvs2_coef(params->dvs2_hor_coefs.odd_imag, hor_ptr_isp, hor_num_3a, hor_padding);
	hor_ptr_isp += hor_num_isp * sizeof(short);
	storedvs2_coef(params->dvs2_hor_coefs.even_real, hor_ptr_isp, hor_num_3a, hor_padding);
	hor_ptr_isp += hor_num_isp * sizeof(short);
	storedvs2_coef(params->dvs2_hor_coefs.even_imag, hor_ptr_isp, hor_num_3a, hor_padding);

	storedvs2_coef(params->dvs2_ver_coefs.odd_real, ver_ptr_isp, ver_num_3a, ver_padding);
	ver_ptr_isp += ver_num_isp * sizeof(short);
	storedvs2_coef(params->dvs2_ver_coefs.odd_imag, ver_ptr_isp, ver_num_3a, ver_padding);
	ver_ptr_isp += ver_num_isp * sizeof(short);
	storedvs2_coef(params->dvs2_ver_coefs.even_real, ver_ptr_isp, ver_num_3a, ver_padding);
	ver_ptr_isp += ver_num_isp * sizeof(short);
	storedvs2_coef(params->dvs2_ver_coefs.even_imag, ver_ptr_isp, ver_num_3a, ver_padding);

	IA_CSS_LEAVE_PRIVATE("void");
}

void ia_css_get_isp_dvs2_coefficients(
	struct ia_css_stream *stream,
	short *hor_coefs_odd_real,
	short *hor_coefs_odd_imag,
	short *hor_coefs_even_real,
	short *hor_coefs_even_imag,
	short *ver_coefs_odd_real,
	short *ver_coefs_odd_imag,
	short *ver_coefs_even_real,
	short *ver_coefs_even_imag)
{
	struct ia_css_isp_parameters *params;
	unsigned int hor_num_3a, ver_num_3a;
	unsigned int hor_num_isp, ver_num_isp;
	hrt_vaddress hor_ptr_isp;
	hrt_vaddress ver_ptr_isp;
	struct ia_css_binary *dvs_binary;

	IA_CSS_ENTER("void");

	assert(stream != NULL);
	assert(hor_coefs_odd_real != NULL);
	assert(hor_coefs_odd_imag != NULL);
	assert(hor_coefs_even_real != NULL);
	assert(hor_coefs_even_imag != NULL);
	assert(ver_coefs_odd_real != NULL);
	assert(ver_coefs_odd_imag != NULL);
	assert(ver_coefs_even_real != NULL);
	assert(ver_coefs_even_imag != NULL);

	params = stream->isp_params_configs;

	/* Only video pipe supports DVS */
	hor_ptr_isp = params->pipe_ddr_ptrs[IA_CSS_PIPE_ID_VIDEO].sdis_hor_coef;
	ver_ptr_isp = params->pipe_ddr_ptrs[IA_CSS_PIPE_ID_VIDEO].sdis_ver_coef;
	dvs_binary = ia_css_stream_get_dvs_binary(stream);
	if (!dvs_binary)
		return;

	hor_num_3a  = dvs_binary->dis.coef.dim.width;
	ver_num_3a  = dvs_binary->dis.coef.dim.height;
	hor_num_isp = dvs_binary->dis.coef.pad.width;
	ver_num_isp = dvs_binary->dis.coef.pad.height;

	mmgr_load(hor_ptr_isp, hor_coefs_odd_real, hor_num_3a * sizeof(short));
	hor_ptr_isp += hor_num_isp * sizeof(short);
	mmgr_load(hor_ptr_isp, hor_coefs_odd_imag, hor_num_3a * sizeof(short));
	hor_ptr_isp += hor_num_isp * sizeof(short);
	mmgr_load(hor_ptr_isp, hor_coefs_even_real, hor_num_3a * sizeof(short));
	hor_ptr_isp += hor_num_isp * sizeof(short);
	mmgr_load(hor_ptr_isp, hor_coefs_even_imag, hor_num_3a * sizeof(short));

	mmgr_load(ver_ptr_isp, ver_coefs_odd_real, ver_num_3a * sizeof(short));
	ver_ptr_isp += ver_num_isp * sizeof(short);
	mmgr_load(ver_ptr_isp, ver_coefs_odd_imag, ver_num_3a * sizeof(short));
	ver_ptr_isp += ver_num_isp * sizeof(short);
	mmgr_load(ver_ptr_isp, ver_coefs_even_real, ver_num_3a * sizeof(short));
	ver_ptr_isp += ver_num_isp * sizeof(short);
	mmgr_load(ver_ptr_isp, ver_coefs_even_imag, ver_num_3a * sizeof(short));

	IA_CSS_LEAVE("void");
}
