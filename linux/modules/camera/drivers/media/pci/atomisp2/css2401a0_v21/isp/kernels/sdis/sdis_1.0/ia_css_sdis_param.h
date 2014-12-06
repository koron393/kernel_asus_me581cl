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

#ifndef __IA_CSS_SDIS_PARAM_H
#define __IA_CSS_SDIS_PARAM_H

/* For YUV upscaling, the internal size is used for DIS statistics */
#define _ISP_SDIS_ELEMS_ISP(input, internal, enable_us) \
	((enable_us) ? (internal) : (input))

#define _ISP_SDIS_HOR_GRID_NUM_3A(in_width, deci_factor_log2) \
	(_ISP_BQS(in_width) >> deci_factor_log2)
#define _ISP_SDIS_VER_GRID_NUM_3A(in_height, deci_factor_log2) \
	(_ISP_BQS(in_height) >> deci_factor_log2)

/* SDIS Number of Grid */
#define _ISP_SDIS_HOR_GRID_NUM_ISP(in_width, deci_factor_log2) \
	CEIL_SHIFT(_ISP_BQS(in_width), deci_factor_log2)
#define _ISP_SDIS_VER_GRID_NUM_ISP(in_height, deci_factor_log2) \
	CEIL_SHIFT(_ISP_BQS(in_height), deci_factor_log2)

/* The number of coefficients used by the 3A library. This excludes
   coefficients from grid cells that do not fall completely within the image. */
#define _ISP_SDIS_HOR_COEF_NUM_3A(in_width, deci_factor_log2) \
	((_ISP_BQS(in_width) >> deci_factor_log2) << deci_factor_log2)
#define _ISP_SDIS_VER_COEF_NUM_3A(in_height, deci_factor_log2) \
	((_ISP_BQS(in_height) >> deci_factor_log2) << deci_factor_log2)

/* SDIS Coefficients: */
/* The ISP uses vectors to store the coefficients, so we round
   the number of coefficients up to vectors. */
#define __ISP_SDIS_HOR_COEF_NUM_VECS(in_width)  _ISP_VECS(_ISP_BQS(in_width))
#define __ISP_SDIS_VER_COEF_NUM_VECS(in_height) _ISP_VECS(_ISP_BQS(in_height))

/* The number of coefficients produced by the ISP */
#define _ISP_SDIS_HOR_COEF_NUM_ISP(in_width) \
	(__ISP_SDIS_HOR_COEF_NUM_VECS(in_width) * ISP_VEC_NELEMS)
#define _ISP_SDIS_VER_COEF_NUM_ISP(in_height) \
	(__ISP_SDIS_VER_COEF_NUM_VECS(in_height) * ISP_VEC_NELEMS)

/* SDIS Projections:
 * SDIS1: Horizontal projections are calculated for each line.
 * Vertical projections are calculated for each column.
 * SDIS2: Projections are calculated for each grid cell.
 * Grid cells that do not fall completely within the image are not
 * valid. The host needs to use the bigger one for the stride but
 * should only return the valid ones to the 3A. */
#define __ISP_SDIS_HOR_PROJ_NUM_ISP(in_width, in_height, deci_factor_log2, \
	isp_pipe_version) \
	((isp_pipe_version == 1) ? \
		CEIL_SHIFT(_ISP_BQS(in_height), deci_factor_log2) : \
		(CEIL_SHIFT(_ISP_BQS(in_width), deci_factor_log2) * \
		 CEIL_SHIFT(_ISP_BQS(in_height), deci_factor_log2)))

#define __ISP_SDIS_VER_PROJ_NUM_ISP(in_width, in_height, deci_factor_log2, \
	isp_pipe_version) \
	((isp_pipe_version == 1) ? \
		CEIL_SHIFT(_ISP_BQS(in_width), deci_factor_log2) : \
		(CEIL_SHIFT(_ISP_BQS(in_width), deci_factor_log2) * \
		 CEIL_SHIFT(_ISP_BQS(in_height), deci_factor_log2)))

#define _ISP_SDIS_HOR_PROJ_NUM_3A(in_width, in_height, deci_factor_log2, \
	isp_pipe_version) \
	((isp_pipe_version == 1) ? \
		(_ISP_BQS(in_height) >> deci_factor_log2) : \
		((_ISP_BQS(in_width) >> deci_factor_log2) * \
		 (_ISP_BQS(in_height) >> deci_factor_log2)))

#define _ISP_SDIS_VER_PROJ_NUM_3A(in_width, in_height, deci_factor_log2, \
	isp_pipe_version) \
	((isp_pipe_version == 1) ? \
		(_ISP_BQS(in_width) >> deci_factor_log2) : \
		((_ISP_BQS(in_width) >> deci_factor_log2) * \
		 (_ISP_BQS(in_height) >> deci_factor_log2)))

#endif /* __IA_CSS_SDIS_PARAM_H */

