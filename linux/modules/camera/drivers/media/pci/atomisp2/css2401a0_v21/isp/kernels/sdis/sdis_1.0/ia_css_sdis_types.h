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

#ifndef __IA_CSS_SDIS_TYPES_H
#define __IA_CSS_SDIS_TYPES_H

#include "isp/kernels/sdis/common/ia_css_sdis_common_types.h"

/* Some binaries put the vertical coefficients in DMEM instead
   of VMEM to save VMEM. */
#define _SDIS_VER_COEF_TBL_USE_DMEM(mode, enable_sdis, enable_ds, isp_pipe_version) \
	(mode == IA_CSS_BINARY_MODE_VIDEO \
	&& enable_sdis && enable_ds != 2 && isp_pipe_version == 1)
#define SDIS_VER_COEF_TBL__IN_DMEM(b) \
	_SDIS_VER_COEF_TBL_USE_DMEM((b)->info->sp.mode, (b)->info->sp.enable.dis, (b)->info->sp.enable.ds, (b)->info->sp.isp_pipe_version)

#define SH_CSS_DIS_VER_NUM_COEF_TYPES(b) \
  (((b)->info->sp.isp_pipe_version == 2) ? IA_CSS_DVS2_NUM_COEF_TYPES : \
	(SDIS_VER_COEF_TBL__IN_DMEM(b) ? \
		IA_CSS_DVS_COEF_TYPES_ON_DMEM : \
		IA_CSS_DVS_NUM_COEF_TYPES))

/** DVS 1.0 Coefficients.
 *  This structure describes the coefficients that are needed for the dvs statistics.
 */

struct ia_css_dvs_coefficients {
	struct ia_css_dvs_grid_info grid;/**< grid info contains the dimensions of the dvs grid */
	int16_t *hor_coefs;	/**< the pointer to int16_t[grid.num_hor_coefs * IA_CSS_DVS_NUM_COEF_TYPES]
				     containing the horizontal coefficients */
	int16_t *ver_coefs;	/**< the pointer to int16_t[grid.num_ver_coefs * IA_CSS_DVS_NUM_COEF_TYPES]
				     containing the vertical coefficients */
};

/** DVS 1.0 Statistics.
 *  This structure describes the statistics that are generated using the provided coefficients.
 */

struct ia_css_dvs_statistics {
	struct ia_css_dvs_grid_info grid;/**< grid info contains the dimensions of the dvs grid */
	int32_t *hor_proj;	/**< the pointer to int16_t[grid.height * IA_CSS_DVS_NUM_COEF_TYPES]
				     containing the horizontal projections */
	int32_t *ver_proj;	/**< the pointer to int16_t[grid.width * IA_CSS_DVS_NUM_COEF_TYPES]
				     containing the vertical projections */
};

#endif /* __IA_CSS_SDIS_TYPES_H */
