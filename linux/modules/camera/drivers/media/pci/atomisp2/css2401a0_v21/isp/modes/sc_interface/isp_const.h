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

#ifndef _COMMON_ISP_CONST_H_
#define _COMMON_ISP_CONST_H_

/*#include "isp.h"*/	/* ISP_VEC_NELEMS */

/* Binary independent constants */

#ifdef MODE
//#error __FILE__ "is mode independent"
#endif

#ifndef NO_HOIST
#  define		NO_HOIST 	HIVE_ATTRIBUTE (( no_hoist ))
#endif

#define NO_HOIST_CSE HIVE_ATTRIBUTE ((no_hoist, no_cse))

#ifdef __HIVECC
#define UNION union
#else
#define UNION struct /* Union constructors not allowed in C++ */
#endif

#define XMEM_WIDTH_BITS              HIVE_ISP_DDR_WORD_BITS
#define XMEM_SHORTS_PER_WORD         (HIVE_ISP_DDR_WORD_BITS/16)
#define XMEM_INTS_PER_WORD           (HIVE_ISP_DDR_WORD_BITS/32)
#define XMEM_POW2_BYTES_PER_WORD      HIVE_ISP_DDR_WORD_BYTES

#define BITS8_ELEMENTS_PER_XMEM_ADDR    CEIL_DIV(XMEM_WIDTH_BITS, 8)
#define BITS16_ELEMENTS_PER_XMEM_ADDR    CEIL_DIV(XMEM_WIDTH_BITS, 16)

#if ISP_VEC_NELEMS == 64
#define ISP_NWAY_LOG2  6
#elif ISP_VEC_NELEMS == 32
#define ISP_NWAY_LOG2  5
#elif ISP_VEC_NELEMS == 16
#define ISP_NWAY_LOG2  4
#elif ISP_VEC_NELEMS == 8
#define ISP_NWAY_LOG2  3
#else
#error "isp_const.h ISP_VEC_NELEMS must be one of {8, 16, 32, 64}"
#endif

/* *****************************
 * ISP input/output buffer sizes
 * ****************************/
/* input image */
#define INPUT_BUF_DMA_HEIGHT          2
#define INPUT_BUF_HEIGHT              2 /* double buffer */
#define OUTPUT_BUF_DMA_HEIGHT         2
#define OUTPUT_BUF_HEIGHT             2 /* double buffer */
#define OUTPUT_NUM_TRANSFERS	      4

/* GDC accelerator: Up/Down Scaling */
/* These should be moved to the gdc_defs.h in the device */
#define UDS_SCALING_N                 HRT_GDC_N
/* AB: This should cover the zooming up to 16MP */
#define UDS_MAX_OXDIM                 5000
/* We support maximally 2 planes with different parameters
       - luma and chroma (YUV420) */
#define UDS_MAX_PLANES                2
#define UDS_BLI_BLOCK_HEIGHT          2
#define UDS_BCI_BLOCK_HEIGHT          4
#define UDS_BLI_INTERP_ENVELOPE       1
#define UDS_BCI_INTERP_ENVELOPE       3
#define UDS_MAX_ZOOM_FAC              64
/* Make it always one FPGA vector. 
   Four FPGA vectors are required and 
   four of them fit in one ASIC vector.*/
#define UDS_MAX_CHUNKS                16

#define UDS_DATA_ICX_LEFT_ROUNDED            0
#define UDS_DATA_OXDIM_FLOORED               1
#define UDS_DATA_OXDIM_LAST                  2
#define UDS_DATA_WOIX_LAST                   3
#define UDS_DATA_IY_TOPLEFT                  4
#define UDS_DATA_CHUNK_CNT                   5
#define UDS_DATA_ELEMENTS_PER_XMEM_ADDR      6
#define UDS_DATA_BLOCK_HEIGHT                7 //free
#define UDS_DATA_DMA_CHANNEL_STRIDE_A        8   //error in param transmit
#define UDS_DATA_DMA_CHANNEL_STRIDE_B        9   //error in param transmit
#define UDS_DATA_DMA_CHANNEL_WIDTH_A        10
#define UDS_DATA_DMA_CHANNEL_WIDTH_B        11
#define UDS_DATA_VECTORS_PER_LINE_IN        12   //error in param transmit
#define UDS_DATA_VECTORS_PER_LINE_OUT       13   //error in param transmit
#define UDS_DATA_VMEM_IN_DIMY               14   //error in param transmit

/* ************
 * lookup table 
 * ************/
#define CTC_LUT_VALSU                   simd_is7_valsu
#define CTC_LUT_OFFSET                  0
#define CTC_LUT_MEM                     CAT(ISP_CELL_TYPE,_simd_bamem)

#define GAMMA_LUT_VALSU                 simd_is8_valsu
#define GAMMA_LUT_OFFSET                0
#define GAMMA_LUT_MEM                   CAT(ISP_CELL_TYPE,_simd_bamem)

#if defined(HAS_VAMEM_VERSION_2)
#define R_GAMMA_LUT_VALSU               simd_is7_valsu
#define R_GAMMA_LUT_OFFSET              0
#define R_GAMMA_LUT_MEM                 CAT(ISP_CELL_TYPE,_simd_vamem1)
#define G_GAMMA_LUT_VALSU               simd_is8_valsu
#define G_GAMMA_LUT_OFFSET              0
#define G_GAMMA_LUT_MEM                 CAT(ISP_CELL_TYPE,_simd_vamem2)
#define B_GAMMA_LUT_VALSU               simd_is6_valsu
#define B_GAMMA_LUT_OFFSET              0
#define B_GAMMA_LUT_MEM                 CAT(ISP_CELL_TYPE,_simd_vamem3)
#elif defined(HAS_VAMEM_VERSION_1)
/* dummy setting : sRGB Gamma is not supported for ISP2300 */
#define R_GAMMA_LUT_VALSU               simd_is7_valsu
#define R_GAMMA_LUT_OFFSET              0
#define R_GAMMA_LUT_MEM                 CAT(ISP_CELL_TYPE,_simd_vamem1)
#define G_GAMMA_LUT_VALSU               simd_is8_valsu
#define G_GAMMA_LUT_OFFSET              0
#define G_GAMMA_LUT_MEM                 CAT(ISP_CELL_TYPE,_simd_vamem2)
#define B_GAMMA_LUT_VALSU               simd_is7_valsu
#define B_GAMMA_LUT_OFFSET              1024
#define B_GAMMA_LUT_MEM                 CAT(ISP_CELL_TYPE,_simd_vamem1)
#else
#error "Unknown VAMEM version"
#endif

#define XNR_LUT_VALSU                   simd_is8_valsu
#define XNR_LUT_OFFSET                  1024

#define ISP_LEFT_PADDING	_ISP_LEFT_CROP_EXTRA(ISP_LEFT_CROPPING)
#define ISP_LEFT_PADDING_VECS	CEIL_DIV(ISP_LEFT_PADDING, ISP_VEC_NELEMS)

#define CEIL_ROUND_DIV_STRIPE(width, stripe, padding) \
	CEIL_MUL(padding + CEIL_DIV(width - padding, stripe), 2)

/* output (Y,U,V) image, 4:2:0 */
#define MAX_VECTORS_PER_LINE \
	CEIL_ROUND_DIV_STRIPE(CEIL_DIV(ISP_MAX_INTERNAL_WIDTH, ISP_VEC_NELEMS), \
			      ISP_NUM_STRIPES, \
			      ISP_LEFT_PADDING_VECS)

/*
 * '+ VECTORS_PER_ITERATION' explanation:
 * when striping an even number of iterations, one of the stripes is
 * one iteration wider than the other to account for overlap
 * so the calc for the output buffer vmem size is:
 * ((width[vectors]/num_of_stripes) + 2[vectors])
 */
#define VECTORS_PER_ITERATION 2
#define LINES_PER_ITERATION 2
#define MAX_VECTORS_PER_OUTPUT_LINE \
	(CEIL_DIV(CEIL_DIV(ISP_MAX_OUTPUT_WIDTH, ISP_NUM_STRIPES) + ISP_LEFT_PADDING, ISP_VEC_NELEMS) + \
	VECTORS_PER_ITERATION)

#define MAX_VECTORS_PER_INPUT_LINE	CEIL_DIV(ISP_MAX_INPUT_WIDTH, ISP_VEC_NELEMS)
#define MAX_VECTORS_PER_INPUT_STRIPE	CEIL_ROUND_DIV_STRIPE(CEIL_DIV(ISP_MAX_INPUT_WIDTH, ISP_VEC_NELEMS) , \
							      ISP_NUM_STRIPES, \
							      ISP_LEFT_PADDING_VECS)

/* Add 2 for left croppping */
#define MAX_SP_RAW_COPY_VECTORS_PER_INPUT_LINE	(CEIL_DIV(ISP_MAX_INPUT_WIDTH, ISP_VEC_NELEMS) + 2)

#define MAX_VECTORS_PER_BUF_LINE \
	(MAX_VECTORS_PER_LINE + DUMMY_BUF_VECTORS)
#define MAX_VECTORS_PER_BUF_INPUT_LINE \
	(MAX_VECTORS_PER_INPUT_STRIPE + DUMMY_BUF_VECTORS)
#define MAX_OUTPUT_Y_FRAME_WIDTH \
	(MAX_VECTORS_PER_LINE * ISP_VEC_NELEMS)
#define MAX_OUTPUT_Y_FRAME_SIMDWIDTH \
	MAX_VECTORS_PER_LINE
#define MAX_OUTPUT_C_FRAME_WIDTH \
	(MAX_OUTPUT_Y_FRAME_WIDTH / 2)
#define MAX_OUTPUT_C_FRAME_SIMDWIDTH \
	CEIL_DIV(MAX_OUTPUT_C_FRAME_WIDTH, ISP_VEC_NELEMS)

/* should be even */
#define NO_CHUNKING (OUTPUT_NUM_CHUNKS == 1)

#define MAX_VECTORS_PER_CHUNK \
	(NO_CHUNKING ? MAX_VECTORS_PER_LINE \
				: 2*CEIL_DIV(MAX_VECTORS_PER_LINE, \
					     2*OUTPUT_NUM_CHUNKS))

#define MAX_C_VECTORS_PER_CHUNK \
	(MAX_VECTORS_PER_CHUNK/2)

/* should be even */
#define MAX_VECTORS_PER_OUTPUT_CHUNK \
	(NO_CHUNKING ? MAX_VECTORS_PER_OUTPUT_LINE \
				: 2*CEIL_DIV(MAX_VECTORS_PER_OUTPUT_LINE, \
					     2*OUTPUT_NUM_CHUNKS))

#define MAX_C_VECTORS_PER_OUTPUT_CHUNK \
	(MAX_VECTORS_PER_OUTPUT_CHUNK/2)



/* should be even */
#define MAX_VECTORS_PER_INPUT_CHUNK \
	(INPUT_NUM_CHUNKS == 1 ? MAX_VECTORS_PER_INPUT_STRIPE \
			       : 2*CEIL_DIV(MAX_VECTORS_PER_INPUT_STRIPE, \
					    2*OUTPUT_NUM_CHUNKS))

/* The input buffer should be on a fixed address in vmem, for continuous capture */
#define INPUT_BUF_ADDR 0x0

#define DEFAULT_C_SUBSAMPLING      2

/****** DMA buffer properties */

#define RAW_BUF_LINES    2

#define RAW_BUF_STRIDE \
	(MAX_VECTORS_PER_INPUT_STRIPE)

#endif /* _COMMON_ISP_CONST_H_ */
