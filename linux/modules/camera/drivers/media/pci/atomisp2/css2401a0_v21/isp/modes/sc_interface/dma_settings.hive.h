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

#ifndef _dma_settings_hive_h_
#define _dma_settings_hive_h_

/* ***************************************
 * 					DMA macros
 * ***************************************/

/* ***************************************
 * 					DMA settings
 * ***************************************/

/* application specific DMA settings */
#define DMA_CONNECTION_0      		dma_isp_to_ddr_connection

#define DUMMY_NUM_XFERS 1


/* ****************************************************************
 * parameters for OB grid table communication
 *		ddr -> isp vmem
 * ****************************************************************/
/* isp side */
#define DMA_OBGRID_BLOCK_HEIGHT     1                                           // [lines] lines to write on every DMA transaction
#define DMA_OBGRID_BLOCK_WIDTH_A    OBGRID_VECTORS_PER_LINE                     // [vectors] vectors to write on every DMA transaction
#define DMA_OBGRID_STRIDE_A         DMA_OBGRID_BLOCK_WIDTH_A*ISP_VEC_ALIGN      // [byte address] stride for VMEM. Round next power of two[ (64elem x 14bits/elem) / 8bits/byte ]
#define DMA_OBGRID_ELEMS_A          ISP_VEC_NELEMS                              // [elements] elements per vector
#define DMA_OBGRID_CROPPING_A       0                                           // [elements] elements to skip on every line
#define DMA_OBGRID_SUB_XFERS        NUM_OBGRID_SUB_XFERS                        // Number of sub transfers per DMA transaction

/* xmem side */
#define DMA_OBGRID_ELEMS_B          (HIVE_ISP_DDR_WORD_BITS/16)                                                     // [elements] We will store 2 elements on a 32bits word
#define DMA_OBGRID_BLOCK_WIDTH_B    CEIL_DIV(DMA_OBGRID_BLOCK_WIDTH_A * DMA_OBGRID_ELEMS_A, DMA_OBGRID_ELEMS_B)     // [elements] to write
#define DMA_OBGRID_STRIDE_B         (DMA_OBGRID_BLOCK_WIDTH_B * HIVE_ISP_DDR_WORD_BYTES)                            // [byte address] stride for xmem
#define DMA_OBGRID_CROPPING_B       0                                                                               // [elements] elements to skip on every line


/* ******************************************************
 * parameters for YUV communication (by 1line)
 *    1. output_y_buf  2lines x 2
 *		2. output_uv_buf 1line  x 2
 * ******************************************************/
#define DMA_BLOCK_OUTPUT         ENABLE_BLOCK_OUTPUT

/* isp side */
#define DMA_OUTPUT_BLOCK_HEIGHT  (DMA_BLOCK_OUTPUT ? (VARIABLE_OUTPUT_FORMAT ? OUTPUT_BLOCK_HEIGHT / 2 : OUTPUT_BLOCK_HEIGHT) : (VARIABLE_RESOLUTION ? 1 : 2))       // [lines] lines to write on every DMA transaction
#define DMA_OUTPUT_BLOCK_WIDTH_A (DMA_BLOCK_OUTPUT ? BLOCK_WIDTH : ISP_OUTPUT_CHUNK_VECS)      // [vectors] vectors to write on every DMA transaction
// stride is 2 * BLOCK width, because the dma is split into two jobs, one for the odd lines, one for the even lines. this is also the case for the non block output, but there the 2* is done in _dma_yuv_configure()
#define DMA_OUTPUT_STRIDE_A      (DMA_BLOCK_OUTPUT ? BLOCK_WIDTH * ISP_VEC_ALIGN : MAX_VECTORS_PER_OUTPUT_CHUNK*ISP_VEC_ALIGN ) // [byte address] stride for VMEM. Round next power of two[ (64elem x 14bits/elem) / 8bits/byte ]
#define DMA_OUTPUT_ELEMS_A       ISP_VEC_NELEMS  // [elements] elements per vector
#define DMA_OUTPUT_CROPPING_A    0       // [elements] elements to skip on every line
#define DMA_OUTPUT_SUB_XFERS     NUM_OUTPUT_SUB_XFERS   // Number of sub transfers per DMA transaction

/* xmem side */
#define DMA_OUTPUT_ELEMS_B       (XMEM_WIDTH_BITS/8)   // [elements] We will store 4 values on a 32bits word
#define DMA_OUTPUT_BLOCK_WIDTH_B CEIL_DIV(DMA_OUTPUT_BLOCK_WIDTH_A * DMA_OUTPUT_ELEMS_A, DMA_OUTPUT_ELEMS_B)  // [elements] to write
#define DMA_OUTPUT_STRIDE_B      (DMA_OUTPUT_BLOCK_WIDTH_B * HIVE_ISP_DDR_WORD_BYTES)       // [byte address] stride for xmem
//#define DMA_OUTPUT_STRIDE_B      	2*640//(DMA_OUTPUT_BLOCK_WIDTH_B * HIVE_ISP_DDR_WORD_BYTES)       // [byte address] stride for xmem
#define DMA_OUTPUT_CROPPING_B    0       // [elements] elements to skip on every line

/* ******************************************************
 * parameters for U/V communication (by 1line)
 * ******************************************************/
/* isp side */
#define DMA_C_BLOCK_HEIGHT              (DMA_BLOCK_OUTPUT ? OUTPUT_BLOCK_HEIGHT / 2 : 1)   // [lines] lines to write on every DMA transaction
#define DMA_C_BLOCK_WIDTH_A             (DMA_BLOCK_OUTPUT ? BLOCK_WIDTH / 2 : OUTPUT_C_VECTORS_PER_CHUNK)        // [vectors] vectors to write on every DMA transaction
#define DMA_OUTPUT_C_BLOCK_WIDTH_A      (DMA_BLOCK_OUTPUT ? BLOCK_WIDTH / 2 : ISP_UV_OUTPUT_CHUNK_VECS)
#define DMA_C_STRIDE_A                  (2 * BLOCK_WIDTH * ISP_VEC_ALIGN)        // [byte address] stride for VMEM. Round next power of two[ (64elem x 14bits/elem) / 8bits/byte ]
#define DMA_C_ELEMS_A                   ISP_VEC_NELEMS      // [elements] elements per vector
#define DMA_C_CROPPING_A                0   // [elements] elements to skip on every line
#define DMA_C_SUB_XFERS                 NUM_C_SUB_XFERS     // Number of sub transfers per DMA transaction

/* xmem side */
#define DMA_C_ELEMS_B                   (XMEM_WIDTH_BITS/8) // [elements] We will store 4 values on a 32bits word
#define DMA_C_BLOCK_WIDTH_B             CEIL_DIV(DMA_C_BLOCK_WIDTH_A * DMA_C_ELEMS_A, DMA_C_ELEMS_B )     // [elements] to write
#define DMA_OUTPUT_C_BLOCK_WIDTH_B      CEIL_DIV(DMA_OUTPUT_C_BLOCK_WIDTH_A * DMA_C_ELEMS_A, DMA_C_ELEMS_B)     // [elements] to write
#define DMA_C_STRIDE_B                  (DMA_C_BLOCK_WIDTH_B * HIVE_ISP_DDR_WORD_BYTES)        // [byte address] stride for xmem
#define DMA_OUTPUT_C_STRIDE_B           (DMA_OUTPUT_C_BLOCK_WIDTH_B * HIVE_ISP_DDR_WORD_BYTES)        // [byte address] stride for xmem
#define DMA_C_CROPPING_B                0   // [elements] elements to skip on every line

#define DMA_SCTBL_STRIPE_ROW_HEIGHT ((ISP_ROW_STRIPES_HEIGHT) >> (DECI_FACTOR_LOG2 + 1))

/* *****************************************************************************
 * parameters for output pixels communication for view finder (vmem -> xmem)	//TODO: sp dmem
 * ***************************************************************************** */

/* isp side */
#define DMA_VFOUT_BLOCK_HEIGHT  1                       // [lines] lines to write on every DMA transaction
#define DMA_VFOUT_BLOCK_WIDTH_A ISP_VF_OUTPUT_WIDTH_VECS// [vectors] vectors to write on every DMA transaction
#define DMA_VFOUT_LINE_WIDTH_A  __ISP_VF_OUTPUT_WIDTH_VECS(ISP_OUTPUT_WIDTH, VF_LOG_DOWNSCALE) // CHANGEE
#define DMA_VFOUT_STRIDE_A      (ISP_MAX_VF_OUTPUT_CHUNK_VECS*ISP_VEC_ALIGN) // Width of one y or uv line
#define DMA_VFOUT_ELEMS_A       ISP_VEC_NELEMS          // [elements] elements per vector
#define DMA_VFOUT_CROPPING_A    0                       // [elements] elements to skip on every line
#define DMA_VFOUT_SUB_XFERS     NUM_VFOUT_SUB_XFERS     // Number of sub transfers per DMA transaction


/* xmem side */
#define DMA_VFOUT_ELEMS_B       (XMEM_WIDTH_BITS/8)     // [elements] We will store 4 values on a 32bits word
#define DMA_VFOUT_BLOCK_WIDTH_B CEIL_DIV(DMA_VFOUT_BLOCK_WIDTH_A * DMA_VFOUT_ELEMS_A, DMA_VFOUT_ELEMS_B)
#define DMA_VFOUT_LINE_WIDTH_B 	CEIL_DIV(DMA_VFOUT_LINE_WIDTH_A * DMA_VFOUT_ELEMS_A, DMA_VFOUT_ELEMS_B)
#define DMA_VFOUT_STRIDE_B      (DMA_VFOUT_LINE_WIDTH_B * HIVE_ISP_DDR_WORD_BYTES)// [byte address] stride for xmem
#define DMA_VFOUT_CROPPING_B    0                       // [elements] elements to skip on every line

/* *********************************************************************************************
 * 	parameters for TNR (temporal noise reduction) pixels communication
 *			1. input previous frame pixels after CNR,YNR: xmem (16bit unit) ---> vmem (14bit unit)
 * 			2. output current frame pixels after CNR,YNR: xmem (16bit unit) <--- vmem (14bit unit)
 *
 *      YYYY  YYYY  UVUV : VECTORS_PER_LINE x 3
 *				1st VECTORS_PER_LINE : Y for GR,R,GR,R....
 *				2nd VECTORS_PER_LINE : Y for B,GB,B,GB....
 *				3rd VECTORS_PER_LINE : UV for GR,GR,....
 * ********************************************************************************************/

/* isp side */
#define TNR_BUF_HEIGHT          (ENABLE_BLOCK_OUTPUT ? OUTPUT_BLOCK_HEIGHT : 2)
#define TNR_BUF_C_HEIGHT        (ENABLE_BLOCK_OUTPUT ? OUTPUT_BLOCK_HEIGHT/2 :1)
#define TNR_BLOCK_HEIGHT        (TNR_BUF_HEIGHT+TNR_BUF_C_HEIGHT)

#define DMA_TNR_BLOCK_WIDTH_A   (ENABLE_BLOCK_OUTPUT ? BLOCK_WIDTH : (ENABLE_DIS_CROP || ENABLE_UDS ? ISP_OUTPUT_CHUNK_VECS : OUTPUT_VECTORS_PER_CHUNK))    // [vectors] vectors to write on every DMA transaction
#define MAX_TNR_BLOCK_WIDTH_A   (ENABLE_BLOCK_OUTPUT ? BLOCK_WIDTH : (ENABLE_DIS_CROP || ENABLE_UDS ? MAX_VECTORS_PER_OUTPUT_CHUNK : MAX_VECTORS_PER_CHUNK))
#define TNR_STRIDE_A            (MAX_TNR_BLOCK_WIDTH_A * ISP_VEC_ALIGN)    // [byte address] stride for VMEM. Round next power of two[ (64elem x 14bits/elem) / 8bits/byte ]
#define DMA_TNR_ELEMS_A         ISP_VEC_NELEMS                             // [elements] elements per vector
#define DMA_TNR_CROPPING_A      0                                          // [elements] elements to skip on every line
#define DMA_TNR_SUB_XFERS       NUM_TNR_SUB_XFERS // Number of sub transfers per DMA transaction

#define DMA_TNR_BUF_BLOCK_WIDTH_A	DMA_TNR_BLOCK_WIDTH_A

/* *********************************************************************************************
 * 	parameters for CROP
 * ********************************************************************************************/

/* isp side */
#define DMA_CROP_Y_BLOCK_HEIGHT (2)       // [lines] lines to write on every DMA transaction
#define DMA_CROP_C_BLOCK_HEIGHT (1)       // [lines] lines to write on every DMA transaction
#define DMA_CROP_BLOCK_WIDTH_A  ISP_OUTPUT_CHUNK_VECS    // [vectors] vectors to write on every DMA transaction
#define DMA_CROP_STRIDE_A     	(MAX_VECTORS_PER_OUTPUT_CHUNK * ISP_VEC_ALIGN)    // [byte address] stride for VMEM. Round next power of two[ (64elem x 14bits/elem) / 8bits/byte ]
#define DMA_CROP_ELEMS_A      	ISP_VEC_NELEMS  // [elements] elements per vector
#define DMA_CROP_CROPPING_A   	0       // [elements] elements to skip on every line
#define DMA_CROP_SUB_XFERS       NUM_CROP_SUB_XFERS // Number of sub transfers per DMA transaction

/* xmem side */
#define DMA_CROP_ELEMS_B      	(XMEM_WIDTH_BITS/8)    // [elements] We will store 4 values on a 32bits word
#define DMA_CROP_BLOCK_WIDTH_B	CEIL_DIV(DMA_CROP_BLOCK_WIDTH_A * DMA_CROP_ELEMS_A, DMA_CROP_ELEMS_B)   // [elements] to write
#define DMA_CROP_STRIDE_B     	(DMA_CROP_BLOCK_WIDTH_B * HIVE_ISP_DDR_WORD_BYTES)  // [byte address] stride for xmem
#define DMA_CROP_CROPPING_B   	0       // [elements] elements to skip on every line

/* *********************************************************************************************
 * 	parameters for REF (reference) pixels communication
  * ********************************************************************************************/

/* isp side */
#define DMA_REF_Y_BLOCK_HEIGHT 	(1+(DMA_REF_Y_CHANNEL != DMA_REF_C_CHANNEL))       // [lines] lines to write on every DMA transaction
#define DMA_REF_C_BLOCK_HEIGHT 	1       // [lines] lines to write on every DMA transaction
#define DMA_REF_BLOCK_WIDTH_A	OUTPUT_VECTORS_PER_CHUNK    // [vectors] vectors to write on every DMA transaction
#define DMA_REF_STRIDE_A     	(MAX_VECTORS_PER_CHUNK * ISP_VEC_ALIGN)    // [byte address] stride for VMEM. Round next power of two[ (64elem x 14bits/elem) / 8bits/byte ]
#define DMA_REF_C_STRIDE_A      (CEIL_DIV(OUTPUT_VECTORS_PER_CHUNK,2) * ISP_VEC_ALIGN)
#define DMA_REF_ELEMS_A      	ISP_VEC_NELEMS  // [elements] elements per vector
#define DMA_REF_CROPPING_A   	0       // [elements] elements to skip on every line
#define DMA_REF_SUB_XFERS       NUM_REF_SUB_XFERS // Number of sub transfers per DMA transaction

#define DMA_REF_BUF_Y_BLOCK_HEIGHT	DMA_REF_Y_BLOCK_HEIGHT
#define DMA_REF_BUF_C_BLOCK_HEIGHT	DMA_REF_C_BLOCK_HEIGHT
#define DMA_REF_BUF_BLOCK_WIDTH_A	DMA_REF_BLOCK_WIDTH_A
#define DMA_REF_BUF_STRIDE_A		DMA_REF_STRIDE_A

/* *********************************************************************************************
 * 	Configuration for DVS Input Data (XMEM->ISP)
  * ********************************************************************************************/
#define DMA_DVS_BLOCK_HEIGHT   (DVS_IN_BLOCK_HEIGHT / 2)
#define DMA_DVS_ELEMS_A         ISP_VEC_NELEMS
#define DMA_DVS_BLOCK_WIDTH_A  (DVS_IN_BLOCK_WIDTH)
#define DMA_DVS_STRIDE_A       (DVS_IN_BLOCK_WIDTH * ISP_VEC_ALIGN)
#define DMA_DVS_CROPPING_A      0

#define DMA_DVS_ELEMS_B        (XMEM_WIDTH_BITS / 16)    // [elements] We will store 2 values on a 32-bit word
#define DMA_DVS_BLOCK_WIDTH_B   CEIL_DIV(DMA_DVS_BLOCK_WIDTH_A * DMA_DVS_ELEMS_A, DMA_DVS_ELEMS_B)
#define DMA_DVS_LINE_WIDTH_B    CEIL_DIV(ISP_INPUT_WIDTH_VECS * DMA_DVS_ELEMS_A, DMA_DVS_ELEMS_B)
#define DMA_DVS_STRIDE_B       (DMA_DVS_LINE_WIDTH_B * HIVE_ISP_DDR_WORD_BYTES)
#define DMA_DVS_CROPPING_B      0
#define DMA_DVS_SUB_XFERS       NUM_REF_SUB_XFERS


/* *********************************************************************************************
 * 	Configuration for DVS Coords (XMEM->DMEM)
  * ********************************************************************************************/
#define DMA_COORDS_BLOCK_HEIGHT		1
#define DMA_COORDS_ELEMS_A		HIVE_ISP_CTRL_DATA_BYTES
#define DMA_COORDS_BLOCK_WIDTH_A	 (3 * CEIL_DIV(sizeof(gdc_warp_param_mem_t), DMA_COORDS_ELEMS_A))
#define DMA_COORDS_STRIDE_A		0
#define DMA_COORDS_CROPPING_A		0

#define DMA_COORDS_ELEMS_B	     	HIVE_ISP_DDR_WORD_BYTES
#define DMA_COORDS_BLOCK_WIDTH_B	CEIL_DIV(DMA_COORDS_BLOCK_WIDTH_A * DMA_COORDS_ELEMS_A, DMA_COORDS_ELEMS_B)
#define DMA_COORDS_STRIDE_B		0

#define DMA_NEXT_COORD_OFFSET_B		CEIL_MUL(sizeof(gdc_warp_param_mem_t), HIVE_ISP_DDR_WORD_BYTES)


#define STRIPE_OFFSET_VECS		(VECTORS_PER_LINE - ISP_LEFT_PADDING_VECS)	// stripe width excluding left padded part


/* ****************************************************************
 * parameters for raw image communication
 *		ddr -> isp vmem
 * ****************************************************************/
/* isp side */

#define DMA_RAW_BLOCK_WIDTH_A  		VECTORS_PER_LINE        // [vectors] vectors to write on every DMA transaction
#define MAX_DMA_RAW_BLOCK_WIDTH_A  	MAX_VECTORS_PER_INPUT_STRIPE        // [vectors] vectors to write on every DMA transaction
#define DMA_RAW_ELEMS_A        		ISP_VEC_NELEMS  // [elements] elements per vector
#define DMA_RAW_CROPPING_A     		0       // [elements] elements to skip on every line
#define DMA_RAW_SUB_XFERS      		NUM_RAW_SUB_XFERS       // Number of sub transfers per DMA transaction
#define DMA_RAW_LINE_WIDTH_A    	ISP_INPUT_WIDTH_VECS

/* xmem side */
#define DMA_RAW_ELEMS_B        		(XMEM_WIDTH_BITS/16)  // [elements] We will store 2 elements on a 32bits word
#define DMA_RAW_BLOCK_WIDTH_B  		CEIL_DIV(DMA_RAW_BLOCK_WIDTH_A * DMA_RAW_ELEMS_A, DMA_RAW_ELEMS_B) // [elements] to write
#define DMA_RAW_LINE_WIDTH_B   		CEIL_DIV(DMA_RAW_LINE_WIDTH_A * DMA_RAW_ELEMS_A, DMA_RAW_ELEMS_B)
#define DMA_RAW_STRIDE_B       		(DMA_RAW_LINE_WIDTH_B * HIVE_ISP_DDR_WORD_BYTES)      // [byte address] stride for xmem
#define DMA_RAW_CROPPING_B     		0     // [elements] elements to skip on every line

#define DMA_RAW_STRIPE_OFFSET_B		CEIL_DIV(STRIPE_OFFSET_VECS*DMA_RAW_ELEMS_A, DMA_RAW_ELEMS_B)


#define DMA_RAW_ELEMS_B_PACK(bits_per_pixel)       		(XMEM_WIDTH_BITS/bits_per_pixel)  // [elements] We will store 2 elements on a 32bits word
#define DMA_RAW_BLOCK_WIDTH_B_PACK(bpp)  		CEIL_DIV(DMA_RAW_BLOCK_WIDTH_A * DMA_RAW_ELEMS_A, DMA_RAW_ELEMS_B_PACK(bpp))	// [elements] to write
#define DMA_RAW_LINE_WIDTH_B_PACK(bpp)   		CEIL_DIV(DMA_RAW_LINE_WIDTH_A * DMA_RAW_ELEMS_A, DMA_RAW_ELEMS_B_PACK(bpp))
#define DMA_RAW_STRIDE_B_PACK(bpp)      		(DMA_RAW_LINE_WIDTH_B_PACK(bpp) * HIVE_ISP_DDR_WORD_BYTES)    // [byte address] stride for xmem

#define REMAINING_ELEMS_B_PACK_STRIPE(stripe_id, bpp)  ((CEIL_MUL((stripe_id) * STRIPE_OFFSET_VECS * DMA_RAW_ELEMS_A, DMA_RAW_ELEMS_B_PACK(bpp)) - (stripe_id) * STRIPE_OFFSET_VECS * DMA_RAW_ELEMS_A))  //remaining elems in one DDR word from last DMA transaction
#define EXTRA_ELEMS_B_PACK_STRIPE(bpp) (DMA_RAW_BLOCK_WIDTH_A * DMA_RAW_ELEMS_A - (DMA_RAW_BLOCK_WIDTH_A * DMA_RAW_ELEMS_A / DMA_RAW_ELEMS_B_PACK(bpp)) * DMA_RAW_ELEMS_B_PACK(bpp))  //extra elements needed from last DDR word during one transfer

#define DMA_RAW_BLOCK_WIDTH_B_PACK_STRIPE(stripe_id, bpp)  		((((REMAINING_ELEMS_B_PACK_STRIPE(stripe_id, bpp) >= EXTRA_ELEMS_B_PACK_STRIPE(bpp)) || (REMAINING_ELEMS_B_PACK_STRIPE(stripe_id, bpp) == 0)) ? DMA_RAW_BLOCK_WIDTH_B_PACK(bpp) : (DMA_RAW_BLOCK_WIDTH_B_PACK(bpp) + 1)))	// [elements] to write
#define DMA_RAW_STRIPE_OFFSET_B_PACK_STRIPE(stripe_id, bpp)		(((stripe_id) * STRIPE_OFFSET_VECS * DMA_RAW_ELEMS_A / DMA_RAW_ELEMS_B_PACK(bpp)))
#define DMA_RAW_CROPPING_B_PACK_STRIPE(stripe_id, bpp)	((DMA_RAW_ELEMS_B_PACK(bpp) - (REMAINING_ELEMS_B_PACK_STRIPE(stripe_id, bpp) ? REMAINING_ELEMS_B_PACK_STRIPE(stripe_id, bpp) : DMA_RAW_ELEMS_B_PACK(bpp))))

/* ****************************************************************
 * parameters for raw image communication
 *		isp vmem -> ddr
 * ****************************************************************/
/* isp side */
/* [lines] lines to write on every DMA transaction */
#define DMA_RAW_OUT_BLOCK_HEIGHT 2
/* [byte address] stride for VMEM. Round next power of two[ (64elem x 14bits/elem) / 8bits/byte ] */
#define DMA_RAW_OUT_STRIDE_A     MAX_VECTORS_PER_OUTPUT_LINE*ISP_VEC_ALIGN
/* Number of sub transfers per DMA transaction */
#define DMA_RAW_OUT_SUB_XFERS    NUM_RAW_SUB_XFERS

/* ******************************************************
 * parameters for crop communication
 * ******************************************************/
//#define DMA_CROP_Y_BLOCK_WIDTH_A          OUTPUT_VECTORS_PER_CHUNK
#define DMA_CROP_Y_BLOCK_WIDTH_A          (DMA_BLOCK_OUTPUT ? BLOCK_WIDTH : ISP_OUTPUT_CHUNK_VECS)
#define DMA_CROP_Y_BLOCK_WIDTH_B          CEIL_DIV(DMA_CROP_Y_BLOCK_WIDTH_A * DMA_OUTPUT_ELEMS_A, DMA_OUTPUT_ELEMS_B)
#define DMA_CROP_Y_STRIDE_B               (DMA_CROP_Y_BLOCK_WIDTH_B * HIVE_ISP_DDR_WORD_BYTES)

//#define DMA_CROP_C_BLOCK_WIDTH_A          OUTPUT_C_VECTORS_PER_CHUNK
#define DMA_CROP_C_BLOCK_WIDTH_A          (DMA_BLOCK_OUTPUT ? BLOCK_WIDTH / 2 : ISP_UV_OUTPUT_CHUNK_VECS)
#define DMA_CROP_C_BLOCK_WIDTH_B          CEIL_DIV(DMA_CROP_C_BLOCK_WIDTH_A * DMA_C_ELEMS_A, DMA_C_ELEMS_B)
#define DMA_CROP_C_STRIDE_B               (DMA_CROP_C_BLOCK_WIDTH_B * HIVE_ISP_DDR_WORD_BYTES)

#define DMA_CROP_VFOUT_Y_BLOCK_WIDTH_A    ISP_VF_OUTPUT_WIDTH_VECS
#define DMA_CROP_VFOUT_Y_BLOCK_WIDTH_B    CEIL_DIV(ISP_VF_OUTPUT_WIDTH, DMA_VFOUT_ELEMS_B)
#define DMA_CROP_VFOUT_Y_STRIDE_B         (DMA_CROP_VFOUT_Y_BLOCK_WIDTH_B * HIVE_ISP_DDR_WORD_BYTES)

#define DMA_CROP_VFOUT_C_BLOCK_WIDTH_A    ISP_VF_UV_OUTPUT_WIDTH_VECS
#define DMA_CROP_VFOUT_C_BLOCK_WIDTH_B    CEIL_DIV(ISP_VF_UV_OUTPUT_WIDTH, DMA_VFOUT_ELEMS_B)
#define DMA_CROP_VFOUT_C_STRIDE_B         (DMA_CROP_VFOUT_C_BLOCK_WIDTH_B * HIVE_ISP_DDR_WORD_BYTES)

#endif /* _dma_settings_hive_h_ */
