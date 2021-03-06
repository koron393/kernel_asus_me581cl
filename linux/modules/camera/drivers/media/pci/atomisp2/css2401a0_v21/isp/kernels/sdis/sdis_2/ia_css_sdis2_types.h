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

#ifndef __IA_CSS_SDIS2_TYPES_H
#define __IA_CSS_SDIS2_TYPES_H

#include "isp/kernels/sdis/common/ia_css_sdis_common_types.h"

/** DVS 2.0 Coefficient types. This structure contains 4 pointers to
 *  arrays that contain the coeffients for each type.
 */
struct ia_css_dvs2_coef_types {
	int16_t *odd_real; /**< real part of the odd coefficients*/
	int16_t *odd_imag; /**< imaginary part of the odd coefficients*/
	int16_t *even_real;/**< real part of the even coefficients*/
	int16_t *even_imag;/**< imaginary part of the even coefficients*/
};

/** DVS 2.0 Coefficients. This structure describes the coefficients that are needed for the dvs statistics.
 *  e.g. hor_coefs.odd_real is the pointer to int16_t[grid.num_hor_coefs] containing the horizontal odd real 
 *  coefficients.
 */
struct ia_css_dvs2_coefficients {
	struct ia_css_dvs_grid_info grid;        /**< grid info contains the dimensions of the dvs grid */
	struct ia_css_dvs2_coef_types hor_coefs; /**< struct with pointers that contain the horizontal coefficients */
	struct ia_css_dvs2_coef_types ver_coefs; /**< struct with pointers that contain the vertical coefficients */
};

/** DVS 2.0 Statistic types. This structure contains 4 pointers to
 *  arrays that contain the statistics for each type.
 */
struct ia_css_dvs2_stat_types {
	int32_t *odd_real; /**< real part of the odd statistics*/
	int32_t *odd_imag; /**< imaginary part of the odd statistics*/
	int32_t *even_real;/**< real part of the even statistics*/
	int32_t *even_imag;/**< imaginary part of the even statistics*/
};

/** DVS 2.0 Statistics. This structure describes the statistics that are generated using the provided coefficients.
 *  e.g. hor_prod.odd_real is the pointer to int16_t[grid.aligned_height][grid.aligned_width] containing 
 *  the horizontal odd real statistics. Valid statistics data area is int16_t[0..grid.height-1][0..grid.width-1]
 */
struct ia_css_dvs2_statistics {
	struct ia_css_dvs_grid_info grid;       /**< grid info contains the dimensions of the dvs grid */
	struct ia_css_dvs2_stat_types hor_prod; /**< struct with pointers that contain the horizontal statistics */
	struct ia_css_dvs2_stat_types ver_prod; /**< struct with pointers that contain the vertical statistics */
};

#endif /* __IA_CSS_SDIS2_TYPES_H */
