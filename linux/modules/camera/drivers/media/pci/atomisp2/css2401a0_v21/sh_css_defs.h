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

#ifndef _SH_CSS_DEFS_H_
#define _SH_CSS_DEFS_H_

#if !defined(__SP1)
#include "isp.h"
#else
#include "system_local.h"  /* to get IS_ISP_2500_SYSTEM for SP1*/
#endif
/*#include "vamem.h"*/ /* Cannot include for VAMEM properties this file is visible on ISP -> pipeline generator */

#include "math_support.h"	/* max(), min, etc etc */

/* Macros for Controlling SP1 enabling and compilation */
/* CHECK_IF_SH_CSS_DEFS_INCLUDED is to test whether all SP1 dependent files have included
	sh_css_defs.h file or not */
#define CHECK_IF_SH_CSS_DEFS_INCLUDED

#if defined(IS_ISP_2500_SYSTEM)
#define SWITCH_GACS_TO_SP1	/* Enabling this macro switches the GACs to SP1 */
#define TRACEBUF_SP1 /* For allocating and sending the trace buffer address from SP1 */

#if !defined(ENABLE_SP1)
#undef SWITCH_GACS_TO_SP1
#undef TRACEBUF_SP1
#endif

#endif /*defined(IS_ISP_2500_SYSTEM)*/

/* ID's for refcount */
#define IA_CSS_REFCOUNT_PARAM_SET_POOL  0xCAFE0001
#define IA_CSS_REFCOUNT_PARAM_BUFFER    0xCAFE0002

/* Digital Image Stabilization */
#define SH_CSS_DIS_DECI_FACTOR_LOG2       6

/* UV offset: 1:uv=-128...127, 0:uv=0...255 */
#define SH_CSS_UV_OFFSET_IS_0             0

#if !defined(IS_ISP_2500_SYSTEM)
/* Bits of bayer is adjusted as 13 in ISP */
#define SH_CSS_BAYER_BITS                 13
#else /* defined(IS_ISP_2500_SYSTEM) */
/* Bits of bayer is adjusted as 11 in ISP */
#define SH_CSS_BAYER_BITS                 11
#endif

/* Max value of bayer data (unsigned 13bit in ISP) */
#define SH_CSS_BAYER_MAXVAL               ((1U << SH_CSS_BAYER_BITS) - 1)

/* Bits of yuv in ISP */
#define SH_CSS_ISP_YUV_BITS               8

#define SH_CSS_DP_GAIN_SHIFT              5
#define SH_CSS_BNR_GAIN_SHIFT             13
#define SH_CSS_YNR_GAIN_SHIFT             13
#define SH_CSS_AE_YCOEF_SHIFT             13
#define SH_CSS_AF_FIR_SHIFT               13
#define SH_CSS_YEE_DETAIL_GAIN_SHIFT      8  /* [u5.8] */
#define SH_CSS_YEE_SCALE_SHIFT            8
#define SH_CSS_TNR_COEF_SHIFT             13
#define SH_CSS_MACC_COEF_SHIFT            11 /* [s2.11] for ISP1 */
#define SH_CSS_MACC2_COEF_SHIFT           13 /* [s[exp].[13-exp]] for ISP2 */
#define SH_CSS_DIS_COEF_SHIFT             13

/* enumeration of the bayer downscale factors. When a binary supports multiple
 * factors, the OR of these defines is used to build the mask of supported
 * factors. The BDS factor is used in pre-processor expressions so we cannot
 * use an enum here. */
#define SH_CSS_BDS_FACTOR_1_00	(0)
#define SH_CSS_BDS_FACTOR_1_25	(1)
#define SH_CSS_BDS_FACTOR_1_50	(2)
#define SH_CSS_BDS_FACTOR_2_00	(3)
#define SH_CSS_BDS_FACTOR_2_25	(4)
#define SH_CSS_BDS_FACTOR_2_50	(5)
#define SH_CSS_BDS_FACTOR_3_00	(6)
#define SH_CSS_BDS_FACTOR_4_00	(7)
#define SH_CSS_BDS_FACTOR_4_50	(8)
#define SH_CSS_BDS_FACTOR_5_00	(9)
#define SH_CSS_BDS_FACTOR_6_00	(10)
#define SH_CSS_BDS_FACTOR_8_00	(11)
#define NUM_BDS_FACTORS	        (12)

#define PACK_BDS_FACTOR(factor)	(1<<(factor))

/*--------------- sRGB Gamma -----------------
CCM        : YCgCo[0,8191] -> RGB[0,4095]
sRGB Gamma : RGB  [0,4095] -> RGB[0,8191]
CSC        : RGB  [0,8191] -> YUV[0,8191]

CCM:
Y[0,8191],CgCo[-4096,4095],coef[-8192,8191] -> RGB[0,4095]

sRGB Gamma:
RGB[0,4095] -(interpolation step16)-> RGB[0,255] -(LUT 12bit)-> RGB[0,4095] -> RGB[0,8191]

CSC:
RGB[0,8191],coef[-8192,8191] -> RGB[0,8191]
--------------------------------------------*/
/* Bits of input/output of sRGB Gamma */
#define SH_CSS_RGB_GAMMA_INPUT_BITS       12 /* [0,4095] */
#define SH_CSS_RGB_GAMMA_OUTPUT_BITS      13 /* [0,8191] */

/* Bits of fractional part of interpolation in vamem, [0,4095]->[0,255] */
#define SH_CSS_RGB_GAMMA_FRAC_BITS        \
	(SH_CSS_RGB_GAMMA_INPUT_BITS - SH_CSS_ISP_RGB_GAMMA_TABLE_SIZE_LOG2)
#define SH_CSS_RGB_GAMMA_ONE              (1 << SH_CSS_RGB_GAMMA_FRAC_BITS)

/* Bits of input of CCM,  = 13, Y[0,8191],CgCo[-4096,4095] */
#define SH_CSS_YUV2RGB_CCM_INPUT_BITS     SH_CSS_BAYER_BITS

/* Bits of output of CCM,  = 12, RGB[0,4095] */
#define SH_CSS_YUV2RGB_CCM_OUTPUT_BITS    SH_CSS_RGB_GAMMA_INPUT_BITS

/* Maximum value of output of CCM */
#define SH_CSS_YUV2RGB_CCM_MAX_OUTPUT     \
	((1 << SH_CSS_YUV2RGB_CCM_OUTPUT_BITS) - 1)

#define SH_CSS_NUM_INPUT_BUF_LINES        4

/* Left cropping only applicable for sufficiently large nway */
#if ISP_VEC_NELEMS == 16
#define SH_CSS_MAX_LEFT_CROPPING          0
#define SH_CSS_MAX_TOP_CROPPING           0
#else
#define SH_CSS_MAX_LEFT_CROPPING          12
#define SH_CSS_MAX_TOP_CROPPING           12
#endif

#define	SH_CSS_SP_MAX_WIDTH               1280

/* This is the maximum grid we can handle in the ISP binaries.
 * The host code makes sure no bigger grid is ever selected. */
#define SH_CSS_MAX_BQ_GRID_WIDTH          80
#define SH_CSS_MAX_BQ_GRID_HEIGHT         60

/* The minimum dvs envelope is 12x12 to make sure the invalid rows/columns
   that result from filter initialization are skipped. */
#define SH_CSS_MIN_DVS_ENVELOPE           12U

/* The FPGA system (vec_nelems == 16) only supports upto 5MP */
#if ISP_VEC_NELEMS == 16
#define SH_CSS_MAX_SENSOR_WIDTH           2560
#define SH_CSS_MAX_SENSOR_HEIGHT          1920
#else
#define SH_CSS_MAX_SENSOR_WIDTH           4608
#define SH_CSS_MAX_SENSOR_HEIGHT          3450
#endif

/* Limited to reduce vmem pressure */
#if ISP_VMEM_DEPTH >= 3072
#define SH_CSS_MAX_CONTINUOUS_SENSOR_WIDTH  SH_CSS_MAX_SENSOR_WIDTH
#define SH_CSS_MAX_CONTINUOUS_SENSOR_HEIGHT SH_CSS_MAX_SENSOR_HEIGHT
#else
#define SH_CSS_MAX_CONTINUOUS_SENSOR_WIDTH  3264
#define SH_CSS_MAX_CONTINUOUS_SENSOR_HEIGHT 2448
#endif
/* When using bayer decimation */
/*
#define SH_CSS_MAX_CONTINUOUS_SENSOR_WIDTH_DEC  4224
#define SH_CSS_MAX_CONTINUOUS_SENSOR_HEIGHT_DEC 3168
*/
#define SH_CSS_MAX_CONTINUOUS_SENSOR_WIDTH_DEC  SH_CSS_MAX_SENSOR_WIDTH
#define SH_CSS_MAX_CONTINUOUS_SENSOR_HEIGHT_DEC SH_CSS_MAX_SENSOR_HEIGHT

#define SH_CSS_MIN_SENSOR_WIDTH           2
#define SH_CSS_MIN_SENSOR_HEIGHT          2

#if defined(IS_ISP_2400_SYSTEM)
/* MAX width and height set to the same to allow for rotated
 * resolutions. */
#define SH_CSS_MAX_VF_WIDTH               1920
#define SH_CSS_MAX_VF_HEIGHT              1920
#else
#define SH_CSS_MAX_VF_WIDTH               1280
#define SH_CSS_MAX_VF_HEIGHT              960
#endif
/*
#define SH_CSS_MAX_VF_WIDTH_DEC               1920
#define SH_CSS_MAX_VF_HEIGHT_DEC              1080
*/
#define SH_CSS_MAX_VF_WIDTH_DEC               SH_CSS_MAX_VF_WIDTH
#define SH_CSS_MAX_VF_HEIGHT_DEC              SH_CSS_MAX_VF_HEIGHT

/* We use 16 bits per coordinate component, including integer
   and fractional bits */
#define SH_CSS_MORPH_TABLE_GRID               ISP_VEC_NELEMS
#define SH_CSS_MORPH_TABLE_ELEM_BYTES         2
#define SH_CSS_MORPH_TABLE_ELEMS_PER_DDR_WORD \
	(HIVE_ISP_DDR_WORD_BYTES/SH_CSS_MORPH_TABLE_ELEM_BYTES)

#define SH_CSS_MAX_SCTBL_WIDTH_PER_COLOR   (SH_CSS_MAX_BQ_GRID_WIDTH + 1)
#define SH_CSS_MAX_SCTBL_HEIGHT_PER_COLOR   (SH_CSS_MAX_BQ_GRID_HEIGHT + 1)
#define SH_CSS_MAX_SCTBL_ALIGNED_WIDTH_PER_COLOR \
	CEIL_MUL(SH_CSS_MAX_SCTBL_WIDTH_PER_COLOR, ISP_VEC_NELEMS)

/* Each line of this table is aligned to the maximum line width. */
#define SH_CSS_MAX_S3ATBL_WIDTH              SH_CSS_MAX_BQ_GRID_WIDTH


#define MAX_DVS_FRAME_DELAY         2
#define NUM_VIDEO_DELAY_FRAMES	(MAX_DVS_FRAME_DELAY + 1)  /* SN: Should this not always match NUM_REF_FRAMES ?*/
#define NUM_VIDEO_TNR_FRAMES	2

/* Rules: these implement logic shared between the host code and ISP firmware.
   The ISP firmware needs these rules to be applied at pre-processor time,
   that's why these are macros, not functions. */
#define _ISP_BQS(num)  ((num)/2)
#define _ISP_VECS(width) CEIL_DIV(width, ISP_VEC_NELEMS)

#define ISP_BQ_GRID_WIDTH(elements_per_line, deci_factor_log2) \
	CEIL_SHIFT(elements_per_line/2,  deci_factor_log2)
#define ISP_BQ_GRID_HEIGHT(lines_per_frame, deci_factor_log2) \
	CEIL_SHIFT(lines_per_frame/2,  deci_factor_log2)
#define ISP_C_VECTORS_PER_LINE(elements_per_line) \
	_ISP_VECS(elements_per_line/2)

/* The morphing table is similar to the shading table in the sense that we
   have 1 more value than we have cells in the grid. */
#define _ISP_MORPH_TABLE_WIDTH(int_width) \
	(CEIL_DIV(int_width, SH_CSS_MORPH_TABLE_GRID) + 1)
#define _ISP_MORPH_TABLE_HEIGHT(int_height) \
	(CEIL_DIV(int_height, SH_CSS_MORPH_TABLE_GRID) + 1)
#define _ISP_MORPH_TABLE_ALIGNED_WIDTH(width) \
	CEIL_MUL(_ISP_MORPH_TABLE_WIDTH(width), \
		 SH_CSS_MORPH_TABLE_ELEMS_PER_DDR_WORD)

#define _ISP_SCTBL_WIDTH_PER_COLOR(input_width, deci_factor_log2) \
	(ISP_BQ_GRID_WIDTH(input_width, deci_factor_log2) + 1)
#define _ISP_SCTBL_HEIGHT(input_height, deci_factor_log2) \
	(ISP_BQ_GRID_HEIGHT(input_height, deci_factor_log2) + 1)
#define _ISP_SCTBL_ALIGNED_WIDTH_PER_COLOR(input_width, deci_factor_log2) \
	CEIL_MUL(_ISP_SCTBL_WIDTH_PER_COLOR(input_width, deci_factor_log2), \
		 ISP_VEC_NELEMS)

/* *****************************************************************
 * Statistics for 3A (Auto Focus, Auto White Balance, Auto Exposure)
 * *****************************************************************/
/* if left cropping is used, 3A statistics are also cropped by 2 vectors. */
#define _ISP_S3ATBL_WIDTH(in_width, deci_factor_log2) \
	(_ISP_BQS(in_width) >> deci_factor_log2)
#define _ISP_S3ATBL_HEIGHT(in_height, deci_factor_log2) \
	(_ISP_BQS(in_height) >> deci_factor_log2)
#define _ISP_S3A_ELEMS_ISP_WIDTH(width, left_crop) \
	(width - ((left_crop) ? 2 * ISP_VEC_NELEMS : 0))

#define _ISP_S3ATBL_ISP_WIDTH(in_width, deci_factor_log2) \
	CEIL_SHIFT(_ISP_BQS(in_width), deci_factor_log2)
#define _ISP_S3ATBL_ISP_HEIGHT(in_height, deci_factor_log2) \
	CEIL_SHIFT(_ISP_BQS(in_height), deci_factor_log2)
#define ISP_S3ATBL_VECTORS \
	_ISP_VECS(SH_CSS_MAX_S3ATBL_WIDTH * \
		  (sizeof(struct ia_css_3a_output)/sizeof(int32_t)))
#define ISP_S3ATBL_HI_LO_STRIDE \
	(ISP_S3ATBL_VECTORS * ISP_VEC_NELEMS)
#define ISP_S3ATBL_HI_LO_STRIDE_BYTES \
	(sizeof(unsigned short) * ISP_S3ATBL_HI_LO_STRIDE)

/* Viewfinder support */
#define __ISP_MAX_VF_OUTPUT_WIDTH(width, left_crop) \
	(width - 2*ISP_VEC_NELEMS + ((left_crop) ? 2 * ISP_VEC_NELEMS : 0))

/* Number of vectors per vf line is determined by the chroma width,
 * the luma width is derived from that. That's why we have the +1. */
#define __ISP_VF_OUTPUT_WIDTH_VECS(out_width, vf_log_downscale) \
	(_ISP_VECS((out_width) >> ((vf_log_downscale)+1)) * 2)

#define _ISP_VF_OUTPUT_WIDTH(vf_out_vecs) ((vf_out_vecs) * ISP_VEC_NELEMS)
#define _ISP_VF_OUTPUT_HEIGHT(out_height, vf_log_ds) \
	((out_height) >> (vf_log_ds))

#define _ISP_LOG_VECTOR_STEP(mode) \
	((mode) == IA_CSS_BINARY_MODE_CAPTURE_PP ? 2 : 1)

/* It is preferred to have not more than 2x scaling at one step
 * in GDC (assumption is for capture_pp and yuv_scale stages) */
#define MAX_PREFERRED_YUV_DS_PER_STEP	2

/* Rules for computing the internal width. This is extremely complicated
 * and definitely needs to be commented and explained. */
#define _ISP_LEFT_CROP_EXTRA(left_crop) ((left_crop) > 0 ? 2*ISP_VEC_NELEMS : 0)

#if defined(IS_ISP_2500_SYSTEM)

/* Nitsan - changed because of new iterator scheme for Stages in the line-loop
	    it's no longer true that the min width is dependent on num-of-stages (pipelining)
*/
#define __ISP_MIN_INTERNAL_WIDTH(num_chunks, pipelining, mode)	256

#else

#define __ISP_MIN_INTERNAL_WIDTH(num_chunks, pipelining, mode) \
	((num_chunks) * (pipelining) * (1<<_ISP_LOG_VECTOR_STEP(mode)) * \
	 ISP_VEC_NELEMS)
#endif

#define __ISP_PADDED_OUTPUT_WIDTH(out_width, dvs_env_width, left_crop) \
	((out_width) + MAX(dvs_env_width, _ISP_LEFT_CROP_EXTRA(left_crop)))

#define __ISP_CHUNK_STRIDE_ISP(mode) \
	((1<<_ISP_LOG_VECTOR_STEP(mode)) * ISP_VEC_NELEMS)

#define __ISP_CHUNK_STRIDE_DDR(c_subsampling, num_chunks) \
	((c_subsampling) * (num_chunks) * HIVE_ISP_DDR_WORD_BYTES)
#define __ISP_INTERNAL_WIDTH(out_width, \
			     dvs_env_width, \
			     left_crop, \
			     mode, \
			     c_subsampling, \
			     num_chunks, \
			     pipelining) \
	CEIL_MUL2(CEIL_MUL2(MAX(__ISP_PADDED_OUTPUT_WIDTH(out_width, \
							    dvs_env_width, \
							    left_crop), \
				  __ISP_MIN_INTERNAL_WIDTH(num_chunks, \
							   pipelining, \
							   mode) \
				 ), \
			  __ISP_CHUNK_STRIDE_ISP(mode) \
			 ), \
		 __ISP_CHUNK_STRIDE_DDR(c_subsampling, num_chunks) \
		)

#define __ISP_INTERNAL_HEIGHT(out_height, dvs_env_height, top_crop) \
	((out_height) + (dvs_env_height) + top_crop)

/* @GC: Input can be up to sensor resolution when either bayer downscaling
 *	or raw binning is enabled.
 *	Also, during continuous mode, we need to align to 4*NWAY since input
 *	should support binning */
#define _ISP_MAX_INPUT_WIDTH(max_internal_width, enable_ds, enable_fixed_bayer_ds, enable_raw_bin, \
				enable_continuous) \
	((enable_ds) ? \
	   SH_CSS_MAX_SENSOR_WIDTH :\
	 (enable_fixed_bayer_ds) ? \
	   CEIL_MUL(SH_CSS_MAX_CONTINUOUS_SENSOR_WIDTH_DEC, 4*ISP_VEC_NELEMS) : \
	 (enable_raw_bin) ? \
	   CEIL_MUL(SH_CSS_MAX_CONTINUOUS_SENSOR_WIDTH, 4*ISP_VEC_NELEMS) : \
	 (enable_continuous) ? \
	   SH_CSS_MAX_CONTINUOUS_SENSOR_WIDTH \
	   : max_internal_width)

#define _ISP_INPUT_WIDTH(internal_width, ds_input_width, enable_ds) \
	((enable_ds) ? (ds_input_width) : (internal_width))

#define _ISP_MAX_INPUT_HEIGHT(max_internal_height, enable_ds, enable_fixed_bayer_ds, enable_raw_bin, \
				enable_continuous) \
	((enable_ds) ? \
	   SH_CSS_MAX_SENSOR_HEIGHT :\
	 (enable_fixed_bayer_ds) ? \
	   SH_CSS_MAX_CONTINUOUS_SENSOR_HEIGHT_DEC : \
	 (enable_raw_bin || enable_continuous) ? \
	   SH_CSS_MAX_CONTINUOUS_SENSOR_HEIGHT \
	   : max_internal_height)

#define _ISP_INPUT_HEIGHT(internal_height, ds_input_height, enable_ds) \
	((enable_ds) ? (ds_input_height) : (internal_height))

#define SH_CSS_MAX_STAGES 6 /* copy, preisp, anr, postisp, capture_pp, vf_pp */

/* For CSI2+ input system, it requires extra paddinga from vmem */
#ifdef CONFIG_CSI2_PLUS
#define _ISP_EXTRA_PADDING_VECS 2
#else
#define _ISP_EXTRA_PADDING_VECS 0
#endif /* CONFIG_CSI2_PLUS */

#endif /* _SH_CSS_DEFS_H_ */
